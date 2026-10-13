# SPDX-License-Identifier: GPL-3.0-or-later
"""Local route and privacy regression tests with synthetic text files only."""
import functools
from http.server import ThreadingHTTPServer
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('web_server', ROOT/'tools/serve_web.py')
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)

class WebServerTest(unittest.TestCase):
    def test_only_explicit_local_resources_are_exposed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            public, game = root/'public', root/'private'
            public.mkdir(); game.mkdir()
            (public/'index.html').write_text('public')
            (game/'ALP2.mdl').write_bytes(b'synthetic')
            (game/'secret.iso').write_bytes(b'not a game; must not be served')
            manifest={'enabled':True,'files':[{'name':'ALP2.mdl','size':9}]}
            factory=functools.partial(server_module.Handler,directory=str(public),game_directory=game,local_manifest=manifest)
            server=ThreadingHTTPServer(('127.0.0.1',0),factory)
            thread=threading.Thread(target=server.serve_forever,daemon=True); thread.start()
            url=f'http://127.0.0.1:{server.server_port}'
            try:
                with urlopen(url+'/local-assets.json') as r:
                    self.assertTrue(json.load(r)['enabled'])
                with urlopen(url+'/local-game/ALP2.mdl') as r:
                    self.assertEqual(r.read(),b'synthetic')
                for route in ('/local-game/secret.iso','/local-game/../secret.iso','/local-game/%2e%2e/secret.iso','/secret.iso'):
                    with self.assertRaises(HTTPError) as err: urlopen(url+route)
                    self.assertEqual(err.exception.code,404)
            finally:
                server.shutdown(); server.server_close(); thread.join()

if __name__ == '__main__': unittest.main()
