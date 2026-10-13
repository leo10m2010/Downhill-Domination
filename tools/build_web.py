#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build only code and public UI; never copies game assets into the static output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', type=Path, default=ROOT / '.tools/emsdk')
    p.add_argument('--build', type=Path, default=ROOT / 'build-web')
    p.add_argument('--sdl-source', type=Path)
    p.add_argument('--debug', action='store_true')
    p.add_argument('--test', action='store_true')
    a = p.parse_args()
    sdk = a.sdk.resolve()
    env = os.environ.copy()
    scripts = ROOT / '.venv' / ('Scripts' if os.name == 'nt' else 'bin')
    if scripts.exists():
        env['PATH'] = str(scripts) + os.pathsep + env['PATH']
    local = sdk / 'upstream/emscripten'
    if local.exists():
        env['PATH'] = str(local) + os.pathsep + env['PATH']
        env['EM_CONFIG'] = str(sdk / '.emscripten')
    emcmake = shutil.which('emcmake', path=env['PATH'])
    if not emcmake:
        p.error('No se encuentra emcmake. Instala y activa Emscripten 6.0.12; consulta web/README.md.')
    cmake = shutil.which('cmake')
    if not cmake:
        p.error('Instala CMake >= 3.20.')
    build = a.build.resolve()
    cmd = [emcmake, cmake, '-S', str(ROOT), '-B', str(build),
           '-DCMAKE_BUILD_TYPE=' + ('Debug' if a.debug else 'Release'),
           '-DDH_WEB_TESTS=' + ('ON' if a.test else 'OFF')]
    sdl = a.sdl_source or (ROOT / '.tools/SDL' if (ROOT / '.tools/SDL').exists() else None)
    if sdl:
        cmd += ['-DFETCHCONTENT_SOURCE_DIR_SDL3=' + str(sdl.resolve())]
    subprocess.run(cmd, env=env, check=True)
    subprocess.run([cmake, '--build', str(build), '--parallel', '4'], env=env, check=True)
    if a.test:
        subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], env=env, check=True)
    runtime_license = Path(emcmake).parent / 'LICENSE'
    if runtime_license.exists():
        shutil.copyfile(runtime_license, build / 'public/EMSCRIPTEN-LICENSE.txt')
    inputs = [ROOT/'CMakeLists.txt', ROOT/'tools/build_web.py']
    for directory in ('src', 'cmake', 'web'):
        inputs.extend(f for f in (ROOT/directory).rglob('*') if f.is_file())
    stamp = {str(f.relative_to(ROOT)).replace('\\', '/'): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(inputs)}
    (build/'build-source.json').write_text(json.dumps(stamp, indent=2), encoding='utf-8')
    print(f'Public output (no game data): {build / "public"}')

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
