# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Owned Linux Secret Service fixture; never use or activate a desktop provider.

Uses installed dbus-daemon, dbus-send, gnome-keyring-daemon and secret-tool.
No mock credential replies: clients use the real libsecret/Secret Service path.
"""
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import tempfile
import time
from xml.sax.saxutils import escape


class SecretServiceSession:
    def __init__(self, environment, evidence):
        self.env = environment.copy()
        self.evidence = Path(evidence)
        self.directory = None
        self.processes = []
        self.logs = []
        self.record = {"backend": "disposable GNOME Keyring / real Secret Service"}

    def start(self):
        tools = {name: shutil.which(name, path=self.env.get("PATH")) for name in
                 ("dbus-daemon", "dbus-send", "gnome-keyring-daemon", "secret-tool")}
        missing = [name for name, path in tools.items() if not path]
        if missing:
            raise RuntimeError("Isolated credential prerequisites missing: " + ", ".join(missing)
                               + ". Install the distribution's D-Bus, GNOME Keyring and libsecret tools; no desktop fallback is allowed.")
        self.tools = tools
        self.directory = tempfile.TemporaryDirectory(prefix="raynmea-credentials-")
        root = Path(self.directory.name)
        for name in ("home", "data", "config", "cache", "state", "runtime", "control", "empty"):
            (root / name).mkdir(mode=0o700)
        for name in list(self.env):
            if name.startswith(("DBUS_", "GNOME_KEYRING_")):
                self.env.pop(name)
        self.env.update(HOME=str(root / "home"), XDG_DATA_HOME=str(root / "data"),
                        XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"),
                        XDG_STATE_HOME=str(root / "state"),
                        XDG_RUNTIME_DIR=str(root / "runtime"), XDG_DATA_DIRS=str(root / "empty"),
                        XDG_CONFIG_DIRS=str(root / "empty"), GNOME_KEYRING_CONTROL=str(root / "control"),
                        DBUS_SESSION_BUS_ADDRESS="unix:path=" + str(root / "runtime/bus"),
                        DBUS_SYSTEM_BUS_ADDRESS="unix:path=" + str(root / "runtime/no-system-bus"))
        # No service directories/includes/systemd activation: the bus cannot
        # activate an installed desktop provider or prompt helper.
        config = root / "bus.conf"
        config.write_text('<busconfig><type>session</type><auth>EXTERNAL</auth>'
                          '<listen>' + escape(self.env['DBUS_SESSION_BUS_ADDRESS']) + '</listen>'
                          '<policy context="default"><allow user="' + str(os.getuid()) + '"/>'
                          '<allow own="*"/><allow send_destination="*"/>'
                          '<allow receive_sender="*"/></policy></busconfig>')
        bus = self._start([tools['dbus-daemon'], '--nofork', '--config-file=' + str(config)], 'bus')
        self._wait_owner('org.freedesktop.DBus', bus)
        provider = self._start([tools['gnome-keyring-daemon'], '--foreground', '--components=secrets',
                                '--control-directory=' + str(root / 'control'), '--unlock'], 'keyring',
                               secrets.token_hex(32).encode())
        self._wait_owner('org.freedesktop.secrets', provider)
        self.record.update(bus_pid=bus.pid, provider_pid=provider.pid, owner_pid_verified=True,
                           service_activation=False, private_data_directory=True)
        # Independent client processes prove create/read/replace/remove rather
        # than treating provider readiness or a fake success as coverage.
        attrs = ['application', 'RayNmeaHarness', 'credential', secrets.token_hex(16)]
        value = secrets.token_hex(24).encode()
        for expected in (value, value + b'-replacement'):
            self._secret(['store', '--label=Disposable regression probe', *attrs], expected, 0)
            result = self._secret(['lookup', *attrs], None, 0)
            if result.stdout != expected:
                raise RuntimeError('Disposable Secret Service readback did not match the synthetic value')
        self._secret(['clear', *attrs], None, 0)
        result = self._secret(['lookup', *attrs], None, 1)
        if result.stdout or result.stderr:
            raise RuntimeError('Disposable Secret Service removal verification failed')
        self.record['synthetic_create_read_replace_remove'] = True
        self._save()
        return self.env.copy()

    def _start(self, command, label, password=None):
        log = (self.evidence / ('credential-' + label + '.txt')).open('wb')
        self.logs.append(log)
        process = subprocess.Popen(command, env=self.env, stdin=subprocess.PIPE if password else subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT)
        self.processes.append(process)
        if password:
            process.stdin.write(password)
            process.stdin.close()
        return process

    def _wait_owner(self, name, process):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('Disposable credential process exited before readiness; see private fixture logs')
            result = subprocess.run([self.tools['dbus-send'], '--bus=' + self.env['DBUS_SESSION_BUS_ADDRESS'],
                                     '--print-reply', '--reply-timeout=500', '--dest=org.freedesktop.DBus',
                                     '/org/freedesktop/DBus', 'org.freedesktop.DBus.GetConnectionUnixProcessID',
                                     'string:' + name], env=self.env, capture_output=True, timeout=2)
            match = re.search(rb'uint32\s+(\d+)', result.stdout)
            if result.returncode == 0 and match:
                if int(match.group(1)) != process.pid:
                    raise RuntimeError('Disposable credential bus owner is not the owned process; refusing tests')
                return
            time.sleep(0.05)
        raise RuntimeError('Disposable credential service readiness timed out; no desktop fallback is allowed')

    def _secret(self, arguments, value, expected_code):
        result = subprocess.run([self.tools['secret-tool'], *arguments], input=value, env=self.env,
                                capture_output=True, timeout=8)
        if result.returncode != expected_code:
            # Never copy backend stdout/stderr into a user-facing error.
            raise RuntimeError('Disposable Secret Service synthetic credential operation failed')
        return result

    def _save(self):
        (self.evidence / 'credential-session.json').write_text(json.dumps(self.record, indent=2))

    def close(self):
        for process in reversed(self.processes):
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        for log in self.logs:
            log.close()
        path = Path(self.directory.name) if self.directory else None
        if self.directory:
            self.directory.cleanup()
        self.record.update(owned_processes_stopped=all(p.poll() is not None for p in self.processes),
                           private_directory_removed=path is None or not path.exists())
        self._save()
