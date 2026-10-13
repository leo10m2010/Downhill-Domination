#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Local static development server. No upload or game execution endpoint."""
import argparse
import json
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

GAME_FILES = {'ALP2.mdl','ALP2.col','ALP2.gates','ALP2.start.pts','bike1.mdl','ALP2.sky.mdl','ALP2.dome.mdl'}

class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm', '.js': 'text/javascript'}
    def __init__(self, *args, game_directory=None, local_manifest=None, **kwargs):
        self.game_directory = game_directory
        self.local_manifest = local_manifest or {'enabled':False}
        super().__init__(*args, **kwargs)

    def send_head(self):
        path = unquote(urlsplit(self.path).path)
        if path == '/local-assets.json':
            import io
            body = json.dumps(self.local_manifest).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type','application/json')
            self.send_header('Content-Length',str(len(body)))
            self.end_headers()
            return io.BytesIO(body)
        if path.startswith('/local-game/'):
            name = path[len('/local-game/'):]
            if not self.game_directory or name not in GAME_FILES or name not in {f['name'] for f in self.local_manifest.get('files',[])}:
                self.send_error(404, 'Local resource not available')
                return None
        return super().send_head()

    def translate_path(self, path):
        route = unquote(urlsplit(path).path)
        if self.game_directory and route.startswith('/local-game/') and route[len('/local-game/'):] in GAME_FILES:
            return str(self.game_directory/route[len('/local-game/'):])
        return super().translate_path(path)
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        super().end_headers()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--directory', type=Path, default=Path(__file__).resolve().parent.parent / 'build-web/public')
    p.add_argument('--port', type=int, default=8080)
    p.add_argument('--game-assets', type=Path, default=Path(__file__).resolve().parent.parent/'out/local-game')
    p.add_argument('--public', action='store_true', help='Disable private automatic assets; serve public build only')
    a = p.parse_args()
    root = a.directory.resolve()
    if not (root / 'index.html').is_file():
        p.error('Falta index.html. Ejecuta tools/build_web.py primero.')
    game_directory = None
    manifest = {'enabled':False}
    if not a.public and (a.game_assets/'manifest.json').is_file():
        game_directory = a.game_assets.resolve()
        manifest = json.loads((game_directory/'manifest.json').read_text(encoding='utf-8'))
        if manifest.get('enabled') is not True or any(f['name'] not in GAME_FILES or not (game_directory/f['name']).is_file() or (game_directory/f['name']).stat().st_size != f['size'] for f in manifest.get('files',[])):
            p.error('El manifiesto privado no coincide con los archivos. Repite prepare_local_game.py.')
    server = ThreadingHTTPServer(('127.0.0.1', a.port), partial(Handler, directory=str(root), game_directory=game_directory, local_manifest=manifest))
    print(f'http://127.0.0.1:{a.port} (Ctrl+C para salir)', flush=True)
    if game_directory:
        print('ALP2 privado: carga automática habilitada; la ISO no se sirve.',flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()

if __name__ == '__main__':
    main()
