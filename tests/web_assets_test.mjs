// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
const source = await readFile(new URL('../web/assets.js', import.meta.url), 'utf8');
const { selectAssets, REQUIRED, MAX_FILE, MAX_TOTAL } = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);
const files = REQUIRED.map(name => ({name, size: 100}));
assert.equal(selectAssets(files).total, 500);
assert.throws(() => selectAssets(files.slice(1)), /ALP2.mdl/);
assert.throws(() => selectAssets([...files, files[0]]), /duplicado/);
assert.throws(() => selectAssets([...files, {name:'game.iso',size:20}]), /no admitido/);
assert.throws(() => selectAssets(files.map(f => ({...f,size:MAX_FILE+1}))), /256 MiB/);
assert.throws(() => selectAssets(files.map(f => ({...f,size:MAX_TOTAL/4}))), /320 MiB/);
assert.throws(() => selectAssets(files.map(f => ({...f,size:0}))), /vacío/);
assert.equal(selectAssets(files.map(f=>({...f,name:f.name.toUpperCase()}))).selected.size, 5);
console.log('web asset selection OK');
