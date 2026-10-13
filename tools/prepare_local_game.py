#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract only ALP2 + three bike parts from a local ISO, convert with upstream tools.
Writes private ignored folders only; does not build, publish or upload anything.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
NAMES = ['ALP2.mdl', 'ALP2.col', 'ALP2.gates', 'ALP2.start.pts', 'bike1.mdl']

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('iso', type=Path)
    a = p.parse_args()
    if not a.iso.is_file(): p.error('No existe la ISO indicada.')
    try:
        import pycdlib
        from unpack_ie import unpack_ie_data
    except ImportError:
        p.error('Instala pycdlib, numpy y pillow en tu entorno Python.')
    started = time.monotonic()
    converted = ROOT/'out/local-game'
    converted.mkdir(parents=True, exist_ok=True)
    iso = pycdlib.PyCdlib()
    try:
        iso.open(str(a.iso.resolve()))
        for directory, base in [('LVL','ALP2'),('BIKE','CANFIELD'),('BIKE','MAN100'),('BIKE','WHBRIAN')]:
            for ext in ('.NGP','.PTR','.TEX','.RTX'):
                name = base+ext
                extracted = ROOT/'iso_extract'/directory/name
                unpacked = ROOT/'unpacked'/directory/name
                extracted.parent.mkdir(parents=True, exist_ok=True)
                unpacked.parent.mkdir(parents=True, exist_ok=True)
                with extracted.open('wb') as target:
                    iso.get_file_from_iso_fp(target, iso_path=f'/{directory}/{name};1')
                raw = unpack_ie_data(extracted.read_bytes())
                unpacked.write_bytes(raw)
                print(f'Extraído y descomprimido: {directory}/{name} ({len(raw)} bytes)', flush=True)
    finally:
        iso.close()
    env = os.environ.copy()
    env['PYTHONIOENCODING'] = 'utf-8'
    def run(script, *args):
        print(f'Convirtiendo: {script}', flush=True)
        subprocess.run([sys.executable,str(ROOT/'tools'/script),*map(str,args)], cwd=ROOT, env=env, check=True)
    run('extract_model.py',ROOT/'unpacked/LVL/ALP2',converted/'ALP2.mdl')
    run('collision.py',ROOT/'unpacked/LVL/ALP2.NGP',converted/'ALP2.col','--instances')
    run('markers.py',ROOT/'unpacked/LVL/ALP2.NGP','x',converted/'ALP2.start.pts',converted/'ALP2.gates')
    parts = ROOT/'out/play'
    parts.mkdir(parents=True,exist_ok=True)
    models=[]
    for name in ('CANFIELD','MAN100','WHBRIAN'):
        model = parts/f'{name}.part.mdl'
        run('extract_model.py',ROOT/'unpacked/BIKE'/name,model)
        models.append(model)
    run('assemble_bike.py',converted/'bike1.mdl',*models)
    names = NAMES+[n for n in ('ALP2.sky.mdl','ALP2.dome.mdl') if (converted/n).is_file()]
    records=[]
    for name in names:
        f = converted/name
        records.append({'name':name,'size':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
    manifest={'enabled':True,'level':'ALP2','files':records}
    tmp = converted/'manifest.json.tmp'
    tmp.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    tmp.replace(converted/'manifest.json')
    print(f'Recursos privados listos: {converted} ({sum(f["size"] for f in records)/1048576:.1f} MiB; {time.monotonic()-started:.1f} s)',flush=True)

if __name__ == '__main__': main()
