#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create an allowlisted static release plus corresponding source (including SDL).
Run after building; NEVER copies game data, SDK binaries or user files.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
PUBLIC = ('index.html', 'app.js', 'assets.js', 'style.css', 'local-assets.json', 'dhweb.js', 'dhweb.wasm',
          'LICENSE', 'SDL-LICENSE.txt', 'EMSCRIPTEN-LICENSE.txt')
FORBIDDEN = {'.iso','.bin','.img','.cue','.mdl','.msh','.pts','.col','.gates','.ngp',
             '.ptr','.tex','.rtx','.elf','.vag','.p2s','.bmp','.wasm','.exe','.key','.pem'}

def files(repo):
    raw = subprocess.check_output(['git', '-C', str(repo), 'ls-files', '--cached', '--others', '--exclude-standard', '-z'])
    return sorted(set(p.decode('utf-8') for p in raw.split(b'\0') if p))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, default=ROOT/'build-web')
    p.add_argument('--output', type=Path, default=ROOT/'dist')
    p.add_argument('--sdl-source', type=Path)
    a = p.parse_args()
    build, output = a.build.resolve(), a.output.resolve()
    if not (build/'build-source.json').is_file(): p.error('Falta manifiesto de build; recompila con tools/build_web.py.')
    stamp = json.loads((build/'build-source.json').read_text(encoding='utf-8'))
    for name, digest in stamp.items():
        path = (ROOT/name).resolve()
        if not path.is_relative_to(ROOT) or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            p.error('La fuente cambió después de compilar: '+name+'. Recompila antes de empaquetar.')
    sdl = (a.sdl_source or (ROOT/'.tools/SDL' if (ROOT/'.tools/SDL').exists() else build/'_deps/sdl3-src')).resolve()
    if not (sdl/'LICENSE.txt').is_file(): p.error('Falta el código SDL: usa --sdl-source RUTA.')
    revision = subprocess.check_output(['git','-C',str(sdl),'rev-parse','HEAD'],text=True).strip()
    if revision != 'badbf8da4ee72b3ef599c721ffc9899e8d7c8d90': p.error('La revisión SDL no coincide con 3.2.26.')
    missing = [name for name in PUBLIC if not (build/'public'/name).is_file()]
    if missing: p.error('Salida incompleta: '+', '.join(missing)+'. Ejecuta tools/build_web.py.')
    # Refuse mixing a release with previous assets or unknown files.
    if output.exists() and any(f.name not in {*PUBLIC,'source.zip','SOURCE.txt'} for f in output.iterdir()):
        p.error('La salida contiene archivos ajenos al paquete. Usa una carpeta nueva.')
    output.mkdir(parents=True,exist_ok=True)
    manifest=[]
    with zipfile.ZipFile(output/'source.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for repo, prefix in ((ROOT,'downhill'),(sdl,'SDL')):
            for name in files(repo):
                path=(repo/name).resolve()
                if not path.is_relative_to(repo) or not path.is_file(): continue
                if path.suffix.lower() in FORBIDDEN:
                    if repo == sdl: continue  # SDL test media/binaries are not needed to rebuild this library.
                    p.error('Archivo no distribuible en fuente: '+name)
                archive.write(path,prefix+'/'+name)
                if repo == ROOT: manifest.append(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+name)
    for name in PUBLIC: shutil.copyfile(build/'public'/name,output/name)
    index = output/'index.html'
    html = index.read_text(encoding='utf-8').replace('<a href="LICENSE">GPL-3.0-or-later</a>', '<a href="LICENSE">GPL-3.0-or-later</a><a href="source.zip">Código fuente de esta versión</a>')
    index.write_text(html,encoding='utf-8')
    text='Corresponding source: source.zip (modified DownHill-Port-PC + SDL3 3.2.26).\n'
    text+='Build: Emscripten 6.0.12; python tools/build_web.py --sdl-source ../SDL\n'
    text+='Engine: GPL-3.0-or-later. SDL: zlib. Runtime: Emscripten MIT/NCSA.\n'
    text+='No game data included. Source file SHA-256:\n'+'\n'.join(manifest)+'\n'
    (output/'SOURCE.txt').write_text(text,encoding='utf-8')
    print(f'Static release + source: {output}')

if __name__ == '__main__': main()
