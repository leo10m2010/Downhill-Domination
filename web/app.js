// SPDX-License-Identifier: GPL-3.0-or-later
import { selectAssets, REQUIRED, OPTIONAL, MAX_TOTAL, MAX_FILE } from './assets.js';
const $ = id => document.getElementById(id);
const canvas = $('canvas');
let engine, ready = false, running = false, busy = false, lost = false, fatal = false, localMode = false;
const held = new Set();
const keyBits = {KeyW:1,ArrowUp:1,KeyS:2,ArrowDown:2,KeyA:4,ArrowLeft:4,KeyD:8,ArrowRight:8,KeyQ:16,KeyE:32,ShiftLeft:128,ShiftRight:128};
const yieldFrame = () => new Promise(resolve => requestAnimationFrame(() => setTimeout(resolve, 0)));
function log(text) { $('log').textContent += `${text}\n`; }
function status(text, progress) { $('status').textContent = text; if (progress !== undefined) $('progress').value = progress; }
function error(text) { $('error').hidden = false; $('error').textContent = text; log(text); }
function sync() { $('play').disabled = !ready || busy || lost || fatal; $('load').hidden = localMode; $('load').disabled = !engine || busy || lost || fatal; }
function input(action = 0, hop = false) {
  let bits = hop ? 64 : 0;
  for (const key of held) bits |= keyBits[key] || 0;
  if (engine && running && !lost && !busy && !fatal) engine._dh_input(bits, action);
}
function clear() { held.clear(); input(); }
function pause() { clear(); if (engine && running && !lost) engine._dh_pause(1); if (running) { $('play').textContent = 'Continuar'; status('En pausa. Pulsa Continuar para volver.'); } }
async function gpuStages() {
  for (let stage = 0; stage <= 4; ++stage) {
    status(`Preparando GPU · etapa ${stage + 1}/5`, .8 + .2 * stage / 5);
    await yieldFrame();
    if (lost || fatal) throw new Error('El contexto gráfico no está disponible.');
    if (!engine._dh_stage(stage)) throw new Error(engine.UTF8ToString(engine._dh_error()));
  }
}
async function loadFiles(files) {
  if (busy || lost || fatal) return;
  pause(); ready = false; running = false; busy = true; sync();
  $('play').textContent = 'Jugar'; $('error').hidden = true; $('empty').hidden = false; $('hud').hidden = true;
  const started = performance.now();
  try {
    const { selected, total } = selectAssets(files);
    engine.FS.mkdirTree('/game');
    for (const name of engine.FS.readdir('/game')) if (name !== '.' && name !== '..') engine.FS.unlink(`/game/${name}`);
    let completed = 0;
    for (const [name, file] of selected) {
      status(`Leyendo ${name}`, .5 * completed / total); await yieldFrame();
      const bytes = new Uint8Array(await file.arrayBuffer(loaded => status(`Cargando ${name}`, .5 * (completed + loaded) / total)));
      if (lost || fatal) throw new Error('Carga interrumpida: contexto gráfico perdido.');
      engine.FS.writeFile(`/game/${name}`, bytes); completed += file.size;
      // Local identity only. This does not certify provenance or compare to game hashes.
      if (crypto.subtle) {
        const hash = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
        const hex = Array.from(hash, x => x.toString(16).padStart(2, '0')).join('');
        if (file.sha256 && hex !== file.sha256) throw new Error(`${name}: no coincide la integridad SHA-256. Prepara de nuevo los recursos locales.`);
        log(`${name} · ${file.size} bytes · SHA-256 ${hex}`);
      }
      status(`Leído ${name}`, .5 * completed / total);
    }
    for (let stage = 0; stage <= 6; ++stage) {
      status(`Validando y cargando motor · etapa ${stage + 1}/7`, .5 + .3 * stage / 7); await yieldFrame();
      if (lost || fatal) throw new Error('Carga interrumpida: contexto gráfico perdido.');
      if (!engine._dh_load(stage)) throw new Error(engine.UTF8ToString(engine._dh_error()));
    }
    await gpuStages(); ready = true;
    const seconds = (performance.now() - started) / 1000;
    $('loadtime').textContent = `${seconds.toFixed(2)} s de carga`;
    log(`Carga CPU/GPU finalizada en ${seconds.toFixed(2)} s. Cielo: ${selected.has('ALP2.sky.mdl')}; panorama: ${selected.has('ALP2.dome.mdl')}.`);
    status('Motor y recursos listos.', 1);
    if (localMode) {
      $('empty').querySelector('h2').textContent = 'ALP2 listo';
      $('empty').querySelector('p').textContent = 'Pulsa Jugar para iniciar el descenso.';
    }
  } catch (e) { error(e.message); status('No se pudo preparar el escenario.', 0); }
  finally {
    // CPU model copies are retained for context restoration; remove duplicate MEMFS data.
    try { for (const name of engine.FS.readdir('/game')) if (name !== '.' && name !== '..') engine.FS.unlink(`/game/${name}`); } catch {}
    busy = false; sync(); $('files').value = '';
  }
}
$('load').onclick = () => $('files').click();
$('files').onchange = e => loadFiles(e.target.files);
$('play').onclick = () => {
  if (!ready || busy || lost || fatal || !engine._dh_play()) return;
  running = true; $('empty').hidden = true; $('hud').hidden = false; $('play').textContent = 'Continuar';
  canvas.focus(); clear(); status('En pista · controles activos al enfocar el canvas.', 1);
};
$('resolution').onchange = () => {
  if (!engine || lost || fatal) return;
  const [w, h] = $('resolution').value.split('x').map(Number); engine._dh_resize(w, h);
  log(`Resolución interna ${w} × ${h}.`);
};
$('fullscreen').onclick = async () => {
  try { if (!document.fullscreenElement) await $('viewport').requestFullscreen(); else await document.exitFullscreen(); canvas.focus(); } catch (e) { error(`Pantalla completa: ${e.message}`); }
};
canvas.addEventListener('keydown', e => {
  if (!(e.code in keyBits) && !['Space', 'Enter', 'KeyT'].includes(e.code)) return;
  e.preventDefault(); if (e.repeat) return; held.add(e.code);
  input(e.code === 'Enter' ? 1 : e.code === 'KeyT' ? 2 : 0, e.code === 'Space');
});
window.addEventListener('keyup', e => { held.delete(e.code); input(); });
canvas.addEventListener('blur', pause);
canvas.addEventListener('pointerdown', () => { if (running && ready && !lost && !busy && !fatal) { canvas.focus(); engine._dh_pause(0); status('En pista.'); } });
window.addEventListener('blur', pause);
document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
canvas.addEventListener('webglcontextlost', e => {
  e.preventDefault(); lost = true; if (engine) engine._dh_pause(2); held.clear(); sync(); error('Contexto WebGL perdido. Esperando recuperación de la GPU.');
});
canvas.addEventListener('webglcontextrestored', async () => {
  lost = false;
  if (!ready || busy || fatal) { ready = false; status(localMode ? 'Contexto recuperado. Recarga la página.' : 'Contexto recuperado. Vuelve a cargar los archivos.'); sync(); return; }
  busy = true; sync();
  try { await gpuStages(); $('error').hidden = true; status('GPU recuperada. Pulsa Continuar.', 1); }
  catch (e) { ready = false; error(`No se pudo recuperar la GPU: ${e.message}`); }
  finally { busy = false; sync(); }
});
function metrics(m) {
  $('fps').textContent = `${m.fps.toFixed(0)} FPS`;
  $('frame').textContent = `${m.frameMs.toFixed(1)} ms/fotograma · ${m.cpuMs.toFixed(1)} ms CPU`;
  $('memory').textContent = `${(m.heap / 1048576).toFixed(0)} MiB WASM`;
  $('speed').textContent = `${m.speed.toFixed(0)} km/h`;
  $('gates').textContent = `${m.gates}/${m.total} puertas`;
  $('race').textContent = m.state === 1 ? `Salida en ${m.countdown}` : m.state >= 3 ? `Meta · ${m.time.toFixed(2)} s` : `${m.time.toFixed(2)} s`;
  window.downhillMetrics = m;
}
async function automaticAssets() {
  // Private assets are exposed only by the developer's loopback server.
  if (!['localhost','127.0.0.1','[::1]'].includes(location.hostname)) return;
  const response = await fetch('local-assets.json', {cache:'no-store'});
  if (!response.ok) return;
  const manifest = await response.json();
  if (!manifest.enabled) return;
  localMode = true; sync();
  $('asset-help').hidden = true;
  document.querySelector('.brief').textContent = 'ALP2 preparado desde tu copia local. Pulsa Jugar cuando termine la carga. El motor se ejecuta en tu navegador.';
  $('empty').querySelector('h2').textContent = 'Preparando ALP2';
  $('empty').querySelector('p').textContent = 'Cargando los recursos de tu copia local.';
  if (!Array.isArray(manifest.files) || manifest.files.length > REQUIRED.length + OPTIONAL.length) throw new Error('Manifiesto local incompatible.');
  const allowed = new Set([...REQUIRED,...OPTIONAL]);
  let total = 0;
  const files = manifest.files.map(record => {
    if (!allowed.has(record.name) || !Number.isSafeInteger(record.size) || record.size <= 0 || record.size > MAX_FILE || !/^[0-9a-f]{64}$/.test(record.sha256)) throw new Error('Registro de recurso local inválido.');
    total += record.size;
    return {name:record.name,size:record.size,sha256:record.sha256,async arrayBuffer(progress) {
      const response = await fetch(`local-game/${encodeURIComponent(record.name)}`, {cache:'no-store'});
      if (!response.ok) throw new Error(`No se pudo cargar ${record.name} (${response.status}).`);
      const reader = response.body.getReader();
      const bytes = new Uint8Array(record.size); let loaded = 0;
      while (true) {
        const {done,value} = await reader.read(); if (done) break;
        if (loaded + value.length > bytes.length) { await reader.cancel(); throw new Error(`${record.name}: tamaño incompatible.`); }
        bytes.set(value,loaded); loaded += value.length; progress(loaded);
      }
      if (loaded !== bytes.length) throw new Error(`${record.name}: descarga local truncada.`);
      return bytes.buffer;
    }};
  });
  if (total > MAX_TOTAL) throw new Error('Los recursos locales exceden el presupuesto de memoria.');
  await loadFiles(files);
}
async function boot() {
  try {
    if (!window.WebAssembly) throw new Error('Este navegador no admite WebAssembly.');
    const probe = document.createElement('canvas'); if (!probe.getContext('webgl2')) throw new Error('WebGL 2 no disponible. Revisa la aceleración gráfica del navegador.');
    await new Promise((resolve, reject) => { const script = document.createElement('script'); script.src = 'dhweb.js'; script.onload = resolve; script.onerror = () => reject(new Error('Falta dhweb.js. Compila con tools/build_web.py y sirve build-web/public.')); document.head.append(script); });
    engine = await window.createDownhill({canvas, onMetrics: metrics, print: log, printErr: log,
      setStatus: text => { if (text) status(text); },
      onAbort: message => { fatal = true; ready = false; sync(); error(`El motor se detuvo: ${message}. Recarga la página.`); }
    });
    const reason = engine.UTF8ToString(engine._dh_error()); if (reason) throw new Error(reason);
    $('resolution').dispatchEvent(new Event('change')); status('Motor listo. Carga los archivos convertidos.', 0); sync();
    await automaticAssets();
  } catch (e) { fatal = true; error(e.message); status('El motor no está disponible.'); sync(); }
}
boot();
