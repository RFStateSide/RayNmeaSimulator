# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Check shipped GUI/TLS loader requirements; install only verified distro runtime packages.

Package names below are supported Debian/Ubuntu/Mint runtime SONAME providers,
confirmed against the qualified package inventories. No SDK, system Qt or libc repair.
"""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

PATH = '/usr/sbin:/usr/bin:/sbin:/bin'
# These providers are used only when the actual shipped loader reports a missing SONAME.
PROVIDERS = {'libEGL.so.1': 'libegl1',
 'libFLAC.so.12': 'libflac12',
 'libFLAC.so.14': 'libflac14',
 'libGL.so.1': 'libgl1',
 'libGLX.so.0': 'libglx0',
 'libGLdispatch.so.0': 'libglvnd0',
 'libICE.so.6': 'libice6',
 'libOpenGL.so.0': 'libopengl0',
 'libSM.so.6': 'libsm6',
 'libX11-xcb.so.1': 'libx11-xcb1',
 'libX11.so.6': 'libx11-6',
 'libXau.so.6': 'libxau6',
 'libXcursor.so.1': 'libxcursor1',
 'libXdmcp.so.6': 'libxdmcp6',
 'libXext.so.6': 'libxext6',
 'libXfixes.so.3': 'libxfixes3',
 'libXi.so.6': 'libxi6',
 'libXrandr.so.2': 'libxrandr2',
 'libXrender.so.1': 'libxrender1',
 'libXss.so.1': 'libxss1',
 'libapparmor.so.1': 'libapparmor1',
 'libasound.so.2': 'libasound2',
 'libasyncns.so.0': 'libasyncns0',
 'libatomic.so.1': 'libatomic1',
 'libbsd.so.0': 'libbsd0',
 'libbz2.so.1.0': 'libbz2-1.0',
 'libcap.so.2': 'libcap2',
 'libcom_err.so.2': 'libcom-err2',
 'libdbus-1.so.3': 'libdbus-1-3',
 'libdecor-0.so.0': 'libdecor-0-0',
 'libdrm.so.2': 'libdrm2',
 'libexpat.so.1': 'libexpat1',
 'libffi.so.8': 'libffi8',
 'libfontconfig.so.1': 'libfontconfig1',
 'libfreetype.so.6': 'libfreetype6',
 'libgbm.so.1': 'libgbm1',
 'libgcrypt.so.20': 'libgcrypt20',
 'libglib-2.0.so.0': 'libglib2.0-0',
 'libgpg-error.so.0': 'libgpg-error0',
 'libgssapi_krb5.so.2': 'libgssapi-krb5-2',
 'libgthread-2.0.so.0': 'libglib2.0-0',
 'libjpeg.so.62': 'libjpeg62-turbo',
 'libk5crypto.so.3': 'libk5crypto3',
 'libkeyutils.so.1': 'libkeyutils1',
 'libkrb5.so.3': 'libkrb5-3',
 'libkrb5support.so.0': 'libkrb5support0',
 'liblz4.so.1': 'liblz4-1',
 'liblzma.so.5': 'liblzma5',
 'libmd.so.0': 'libmd0',
 'libmp3lame.so.0': 'libmp3lame0',
 'libmpg123.so.0': 'libmpg123-0',
 'libogg.so.0': 'libogg0',
 'libopus.so.0': 'libopus0',
 'libpcre2-8.so.0': 'libpcre2-8-0',
 'libpng16.so.16': 'libpng16-16',
 'libpulse.so.0': 'libpulse0',
 'libpulsecommon-16.1.so': 'libpulse0',
 'libpulsecommon-17.0.so': 'libpulse0',
 'libsamplerate.so.0': 'libsamplerate0',
 'libsndfile.so.1': 'libsndfile1',
 'libsystemd.so.0': 'libsystemd0',
 'libudev.so.1': 'libudev1',
 'libuuid.so.1': 'libuuid1',
 'libvorbis.so.0': 'libvorbis0a',
 'libvorbisenc.so.2': 'libvorbisenc2',
 'libwayland-client.so.0': 'libwayland-client0',
 'libwayland-cursor.so.0': 'libwayland-cursor0',
 'libwayland-egl.so.1': 'libwayland-egl1',
 'libwayland-server.so.0': 'libwayland-server0',
 'libxcb-cursor.so.0': 'libxcb-cursor0',
 'libxcb-glx.so.0': 'libxcb-glx0',
 'libxcb-icccm.so.4': 'libxcb-icccm4',
 'libxcb-image.so.0': 'libxcb-image0',
 'libxcb-keysyms.so.1': 'libxcb-keysyms1',
 'libxcb-randr.so.0': 'libxcb-randr0',
 'libxcb-render-util.so.0': 'libxcb-render-util0',
 'libxcb-render.so.0': 'libxcb-render0',
 'libxcb-shape.so.0': 'libxcb-shape0',
 'libxcb-shm.so.0': 'libxcb-shm0',
 'libxcb-sync.so.1': 'libxcb-sync1',
 'libxcb-util.so.1': 'libxcb-util1',
 'libxcb-xfixes.so.0': 'libxcb-xfixes0',
 'libxcb-xkb.so.1': 'libxcb-xkb1',
 'libxcb.so.1': 'libxcb1',
 'libxkbcommon-x11.so.0': 'libxkbcommon-x11-0',
 'libxkbcommon.so.0': 'libxkbcommon0'}
PROVIDERS.update({'libssl.so.3': 'libssl3', 'libcrypto.so.3': 'libssl3'})
TARGETS = ['bin/RayNmeaSimulator', 'plugins/platforms/libqxcb.so',
           'plugins/tls/libqopensslbackend.so', 'plugins/imageformats/libqsvg.so',
           'plugins/iconengines/libqsvgicon.so']


def environment():
    env = {k: v for k, v in os.environ.items() if not k.startswith(('LD_', 'QT_', 'QML'))}
    env.update(PATH=PATH, LC_ALL='C')
    env.pop('APT_CONFIG', None)
    return env


def run(command, **kwargs):
    return subprocess.run(command, env=environment(), **kwargs)


def os_info(path=Path('/etc/os-release')):
    result = {}
    for line in path.read_text().splitlines():
        if '=' in line and not line.startswith('#'):
            k, v = line.split('=', 1)
            fields = shlex.split(v)
            result[k] = fields[0] if fields else ''
    return result


def supported(info):
    # Do not infer permission to run apt on arbitrary ID_LIKE derivatives.
    return info.get('ID') in ('debian', 'ubuntu', 'linuxmint')


def provider(soname, info):
    package = PROVIDERS.get(soname)
    release = info.get('VERSION_ID', '').split('.')[0]
    t64 = ((info.get('ID') == 'ubuntu' and release.isdigit() and int(release) >= 24)
           or (info.get('ID') == 'linuxmint' and release.isdigit() and int(release) >= 22)
           or (info.get('ID') == 'debian' and release.isdigit() and int(release) >= 13))
    return package + 't64' if t64 and package in ('libssl3', 'libglib2.0-0', 'libasound2', 'libmpg123-0', 'libpng16-16') else package


def missing_libraries(root):
    ldd = shutil.which('ldd', path=PATH)
    if not ldd:
        raise RuntimeError('The system ldd command is required to verify this package; no installation changed.')
    missing = set()
    for name in TARGETS:
        target = root / name
        if not target.is_file():
            raise RuntimeError('Required packaged runtime component is missing: ' + name)
        result = run([ldd, str(target)], text=True, capture_output=True)
        output = result.stdout + result.stderr
        absent = set(re.findall(r'^\s*(\S+)\s+=>\s+not found\s*$', output, re.M))
        missing.update(absent)
        # ABI/version failures are not repaired by upgrading the OS or installing Qt.
        if (result.returncode and not absent) or re.search(r"version [`'][^\n]+not found", output):
            raise RuntimeError('Runtime/ABI verification failed for ' + name + ':\n' + output
                               + '\nUse a compatible package; do not replace system glibc or Qt.')
    # The Qt OpenSSL backend loads these dynamically, so ldd alone cannot prove availability.
    code = ('import ctypes,sys\nfor n in sys.argv[1:]:\n'
            ' try: ctypes.CDLL(n)\n except OSError: print(n)\n')
    result = run([sys.executable, '-I', '-c', code, 'libssl.so.3', 'libcrypto.so.3'],
                 text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError('Could not check the system OpenSSL runtime: ' + result.stderr)
    missing.update(result.stdout.split())
    if any(not re.fullmatch(r'[A-Za-z0-9_.+-]+', name) for name in missing):
        raise RuntimeError('Unexpected runtime dependency name; no package manager action taken.')
    return sorted(missing)


def install_command(packages):
    apt = shutil.which('apt-get', path=PATH)
    if not apt:
        raise RuntimeError('apt-get is unavailable; install the listed runtime libraries through your distribution tools.')
    return [apt, '--no-remove', '--no-upgrade', '--no-install-recommends', 'install', *packages]


def checked_plan(command):
    simulation = run([command[0], '--simulate', *command[1:]], capture_output=True, text=True)
    if simulation.returncode:
        raise RuntimeError('Dependency installation could not be planned (offline, stale indexes or unavailable packages).\n'
                           + simulation.stdout + simulation.stderr)
    changes = []
    for line in simulation.stdout.splitlines():
        if line.startswith('Remv '):
            raise RuntimeError('Package manager proposed removal; no changes made.')
        if line.startswith('Inst '):
            match = re.match(r'Inst ([A-Za-z0-9+.:_-]+)(?: \[([^\]]+)\])?', line)
            if not match:
                raise RuntimeError('Unrecognized package plan; no changes made.')
            name, previous = match.groups()
            plain = name.split(':')[0]
            if previous or plain in ('libc6', 'libc-bin', 'libstdc++6', 'libgcc-s1') or plain.startswith(('libqt', 'qt6-')):
                raise RuntimeError('Package plan would upgrade an installed library or change glibc/system Qt; '
                                   'no changes made: ' + line)
            changes.append(name)
    if not changes:
        raise RuntimeError('No installable missing runtime packages found; no changes made.')
    return changes


def ensure(root, policy='ask', non_interactive=False):
    missing = missing_libraries(root)
    if not missing:
        print('Required GUI/X11 and OpenSSL runtime libraries are available.')
        return
    info = os_info()
    print('Missing system runtime libraries: ' + ', '.join(missing), flush=True)
    packages = sorted({provider(name, info) for name in missing if provider(name, info)})
    unknown = [name for name in missing if provider(name, info) is None]
    if unknown or not supported(info):
        raise RuntimeError('Automatic dependency installation is not supported for this distribution or these libraries: '
                           + ', '.join(unknown or missing) + '. Install matching distribution runtime packages manually; '
                           'no system or application installation changed.')
    command = install_command(packages)
    manual = 'sudo ' + shlex.join(command)
    print('Required packages: ' + ', '.join(packages) + '\nManual command: ' + manual, flush=True)
    interactive = not non_interactive and sys.stdin.isatty() and sys.stdout.isatty()
    if policy == 'check' or (policy == 'ask' and not interactive):
        raise RuntimeError('Dependencies are missing. No packages installed and no application installation changed.\n'
                           'Use the command above, or explicitly opt in with --dependencies install.')
    additions = checked_plan(command)
    print('Package-manager plan (new packages only): ' + ', '.join(additions), flush=True)
    if policy == 'ask':
        try:
            consent = input('Install these runtime packages using administrator privileges? [y/N] ').strip().lower()
        except EOFError:
            consent = ''
        if consent not in ('y', 'yes'):
            raise RuntimeError('Dependency installation declined; no packages or application installation changed.\n'
                               + 'To install manually: ' + manual)
    # --dependencies install is explicit consent, including noninteractive automation.
    privileged = command
    if os.geteuid() != 0:
        sudo = shutil.which('sudo', path=PATH)
        if not sudo:
            raise RuntimeError('sudo is unavailable. Ask an administrator to run: ' + manual)
        privileged = [sudo, *(['-n'] if not interactive else []), *command]
    privileged.insert(len(privileged) - len(command) + 1, '-y')
    print('Installing explicitly approved distribution runtime dependencies: ' + shlex.join(privileged), flush=True)
    try:
        result = run(privileged)
    except OSError as error:
        raise RuntimeError('Could not start dependency installation; application installation unchanged. ' + str(error)) from error
    if result.returncode:
        raise RuntimeError('Dependency installation failed (network/package-manager/authorization error). '
                           'Application installation unchanged. Some system dependencies may have been installed; '
                           'review package-manager output, then retry. Manual command: ' + manual)
    remaining = missing_libraries(root)
    if remaining:
        raise RuntimeError('Runtime dependencies remain unavailable after package installation: ' + ', '.join(remaining)
                           + '. Application installation unchanged.')
    print('Runtime dependency installation verified.')
