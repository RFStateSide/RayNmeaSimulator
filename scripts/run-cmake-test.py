# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Run a selected test with disposable data and bounded, private evidence.

This launcher owns only its fresh profile and optional localhost virtual bridge.
It never reuses a normal application profile or connects to a physical CAN bus.
"""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import signal
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
for key in ("name", "root", "build", "qt-root"):
    parser.add_argument("--" + key, required=True)
parser.add_argument("--sdl-root", default="")
parser.add_argument("--timeout", type=float, default=150)
parser.add_argument("--interactive", action="store_true", help="Bounded manual UI session for isolated helpers only")
parser.add_argument("--credentials", action="store_true", help="Use an owned Linux D-Bus/Secret Service fixture")
parser.add_argument("command", nargs=argparse.REMAINDER)
args = parser.parse_args()
if not 0 < args.timeout <= 150:
    parser.error("--timeout must be greater than zero and at most 150 seconds")
if args.interactive and args.name not in ("RayNmeaSimulator-isolated", "route_acceptance_fixture"):
    parser.error("--interactive is only available for isolated manual helpers")
root, build, qt = (Path(value).resolve() for value in (args.root, args.build, args.qt_root))
evidence = build / "test-evidence"
evidence.mkdir(exist_ok=True)
run = Path(tempfile.mkdtemp(prefix=args.name + "-", dir=evidence))
work = run / "work"
work.mkdir()
env = os.environ.copy()
for name in list(env):
    if (name.startswith(("RAY_NETWORK_", "RAY_MULTI_", "RAYNMEA_DIAGNOSTIC_REPLAY", "RAYNMEA_ROUTE_TRANSCRIPT", "RAY_M240_CAPTURE", "M219_CAPTURE"))
            or name in ("RAY_ROUTE_OWNER_FIXTURE", "M216C_TEST_LAYOUT")
            or name.endswith(("_EVIDENCE", "_IMAGE_DIR", "_PREVIEW", "_SNAPSHOT", "_CAPTURE"))):
        env.pop(name)
env.update(RAYNMEA_TEST_HOME=str(run / "profile"), XDG_CONFIG_HOME=str(run / "config"),
           RAYNMEA_TEST_DEFAULT=str(root / "tests/fixtures/defaultLayout.json"),
           QT_QPA_PLATFORM=env.get("RAYNMEA_TEST_PLATFORM", "offscreen"),
           QT_PLUGIN_PATH=str(qt / "plugins"), PYTHONDONTWRITEBYTECODE="1",
           NMEA0183_SENTENCE_OUTPUT=str(run / "seven-sentences.nmea"))
if sys.platform == "darwin":
    # Darwin's default /var temporary path traverses a symlink. TestProfile
    # intentionally rejects symlink ancestors, so Qt temporary profiles must
    # also live under this owned canonical run directory.
    temporary = run / "tmp"
    temporary.mkdir()
    env["TMPDIR"] = str(temporary) + "/"
runtime_paths = [str(qt / "bin")]
if args.sdl_root:
    runtime_paths.append(str(Path(args.sdl_root).resolve() / "bin"))
env["PATH"] = os.pathsep.join(runtime_paths + [env.get("PATH", "")])
if sys.platform.startswith("linux"):
    env["LD_LIBRARY_PATH"] = str(qt / "lib") + os.pathsep + env.get("LD_LIBRARY_PATH", "")
elif os.name == "nt":
    # Match the established Windows offscreen qualification environment.
    env.setdefault("QT_QPA_FONTDIR", str(Path(env.get("WINDIR", "C:/Windows")) / "Fonts"))
# Historical fixture prefixes are created only inside this fresh working folder.
source = root / "tests" / (args.name + ".cpp")
if source.exists():
    for prefix in set(re.findall(r'build/(m[A-Za-z0-9_-]+)', source.read_text(encoding="utf-8"))):
        (work / "build" / prefix).mkdir(parents=True, exist_ok=True)
# Copy only example routes explicitly embedded in the accepted resource list.
for item in ET.parse(root / "RayNmeaSimulator.qrc").getroot().iter("file"):
    if item.text.startswith("routes/"):
        destination = work / item.text
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / item.text, destination)
command = args.command[1:] if args.command[:1] == ["--"] else args.command
if not command:
    parser.error("Provide a built test executable after --")
qt_results = run / "qt-results.xml"
if source.exists() and re.search(r'QTEST_(?:MAIN|GUILESS_MAIN|APPLESS_MAIN)|QTest::qExec', source.read_text(encoding="utf-8")) and "-o" not in command:
    command += ["-o", str(qt_results) + ",xml"]
bridge = None
key_file = None
credential_session = None
result = {"name": args.name, "exit_code": 1}
try:
    if sys.platform.startswith("linux"):
        # Namespacing alone does not isolate the desktop bus/provider. Tests
        # without credential integration also must not inherit desktop access.
        for name in list(env):
            if name.startswith(("DBUS_", "GNOME_KEYRING_")):
                env.pop(name)
        env["DBUS_SESSION_BUS_ADDRESS"] = "unix:path=" + str(run / "no-session-bus")
        env["DBUS_SYSTEM_BUS_ADDRESS"] = "unix:path=" + str(run / "no-system-bus")

        def terminate_test(signum, frame):
            raise RuntimeError("Test launcher interrupted; cleaning up owned processes")

        signal.signal(signal.SIGTERM, terminate_test)
        if args.credentials:
            from test_secret_service import SecretServiceSession
            credential_session = SecretServiceSession(env, run)
            env = credential_session.start()
    elif args.credentials:
        raise RuntimeError("--credentials is a Linux Secret Service integration option")
    if args.name in ("network_can_tests", "device_discovery_tests"):
        key_file = work / "build/m239b8/bridge-key-private.txt"
        key_file.parent.mkdir(parents=True, exist_ok=True)
        key_file.write_text(secrets.token_hex(32))
        key_file.chmod(0o600)
        bridge_log = (run / "virtual-bridge.txt").open("w")
        bridge = subprocess.Popen([sys.executable, "-u", "-B", str(root / "tools/network-can/raynmea_bridge.py"),
                                   "--virtual", "--interface", "lab", "--bind", "127.0.0.1", "--port", "0",
                                   "--key-file", str(key_file)], stdout=bridge_log, stderr=subprocess.STDOUT, env=env)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            for line in (run / "virtual-bridge.txt").read_text().splitlines():
                port = re.search(r'RN2K/1 port=(\d+);', line)
                if port:
                    env["RAY_NETWORK_TEST_PORT"] = port.group(1)
                try:
                    record = json.loads(line)
                    if "port" in record:
                        env["RAY_NETWORK_TEST_PORT"] = str(record["port"])
                except ValueError:
                    pass
            if env.get("RAY_NETWORK_TEST_PORT"):
                break
            if bridge.poll() is not None:
                raise RuntimeError("Virtual bridge failed; see private run evidence")
            time.sleep(0.02)
        else:
            raise RuntimeError("Virtual bridge readiness timed out")
    with (run / "output.txt").open("w") as log:
        process = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT,
                                 timeout=3600 if args.interactive else args.timeout)
    result = {"name": args.name, "exit_code": process.returncode}
    sentence_file = run / "seven-sentences.nmea"
    if args.name == "nmea0183_compatibility_tests" and process.returncode == 0 and sentence_file.exists():
        with (run / "independent-sentences.txt").open("w") as log:
            validation = subprocess.run([sys.executable, str(root / "tests/validate_sentences.py"), str(sentence_file)],
                                        env=env, stdout=log, stderr=subprocess.STDOUT, timeout=20)
        result["independent_sentence_validation"] = validation.returncode
        if validation.returncode:
            result["exit_code"] = validation.returncode
    if qt_results.exists():
        report = ET.parse(qt_results).getroot()
        counts = {}
        for incident in report.iter("Incident"):
            kind = incident.get("type")
            counts[kind] = counts.get(kind, 0) + 1
        result["qt_incidents"] = counts
        print("QtTest incidents:", counts)
        for function in report.findall("TestFunction"):
            for incident in function.findall("Incident"):
                if incident.get("type") == "fail":
                    print("FAIL", function.get("name"), incident.findtext("Description", ""))
    print((run / "output.txt").read_text())
except subprocess.TimeoutExpired:
    result.update(exit_code=124, reason="Test timed out")
    print("FAIL: test timed out")
except (OSError, RuntimeError, ET.ParseError) as error:
    result.update(exit_code=127 if isinstance(error, FileNotFoundError) else 1, reason=str(error))
    print("FAIL:", error)
    if isinstance(error, FileNotFoundError):
        print("Build the selected test target and check its prerequisites first.")
finally:
    if credential_session is not None:
        credential_session.close()
        result["credential_isolation"] = credential_session.record
    if bridge is not None:
        if bridge.poll() is None:
            bridge.terminate()
        try:
            bridge.wait(timeout=3)
        except subprocess.TimeoutExpired:
            bridge.kill()
            bridge.wait(timeout=3)
        bridge_log.close()
        result["virtual_bridge_stopped"] = bridge.poll() is not None
    if key_file is not None:
        key_file.unlink(missing_ok=True)
    (run / "result.json").write_text(json.dumps(result, indent=2))
    print("Private test evidence:", run)
sys.exit(result["exit_code"])
