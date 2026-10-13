// SPDX-License-Identifier: GPL-3.0-or-later
// Playwright CLI snippet. All records are synthetic and exist only in browser memory.
async (page) => {
  const results={};
  await page.waitForFunction(() => !document.getElementById('load').disabled, null, {timeout:20000});
  results.boot=await page.locator('#status').textContent();
  if (await page.locator('#play').isEnabled()) throw new Error('Play must be disabled without assets');
  await page.evaluate(()=>{const dt=new DataTransfer();dt.items.add(new File(['invalid'],'ALP2.mdl'));const input=document.getElementById('files');input.files=dt.files;input.dispatchEvent(new Event('change'));});
  await page.waitForFunction(() => document.getElementById('error').textContent.includes('Faltan'));
  results.missing=await page.locator('#error').textContent();
  await page.reload();
  await page.waitForFunction(() => !document.getElementById('load').disabled);
  results.canvas=await page.locator('#canvas').evaluate(c=>({width:c.width,height:c.height,webgl2:!!c.getContext('webgl2')}));
  await page.locator('#resolution').selectOption('960x540');
  results.resize=await page.locator('#canvas').evaluate(c=>({width:c.width,height:c.height}));
  if(results.resize.width!==960||results.resize.height!==540)throw new Error('Resolution resize failed');
  // Synthetic binary records exercise the actual C++ parsers, shader and physics.
  // They are test-only, not game assets or a stand-in for verification of ALP2.
  await page.evaluate(()=>{
    function binary(){const parts=[];return {u(v){const b=new ArrayBuffer(4);new DataView(b).setUint32(0,v,true);parts.push(new Uint8Array(b));},f(v){const b=new ArrayBuffer(4);new DataView(b).setFloat32(0,v,true);parts.push(new Uint8Array(b));},raw(v){parts.push(new Uint8Array(v));},done(){const b=new Uint8Array(parts.reduce((n,p)=>n+p.length,0));let i=0;for(const p of parts){b.set(p,i);i+=p.length;}return b;}};}
    function model(points){const b=binary();b.raw([68,72,77,50]);b.u(1);b.u(points.length);b.u(points.length);b.u(2);b.u(2);b.raw([140,200,90,255,140,200,90,255,140,200,90,255,140,200,90,255]);for(const p of points)for(const f of [...p,0,0,1,1,1,1,0])b.f(f);points.forEach((_,i)=>b.u(i));return b.done();}
    const terrain=[[-100,-100,0],[100,-100,0],[-100,100,0],[100,100,0],[-100,100,0],[100,-100,0]];
    const col=binary();col.u(2);for(let i=0;i<terrain.length;i+=3){terrain.slice(i,i+3).flat().forEach(f=>col.f(f));col.u(0);}
    const gates=binary();gates.u(3);gates.u(8050);gates.u(0);[0,1,0,2].forEach(f=>gates.f(f));gates.u(8051);gates.u(1);[0,1,0,80].forEach(f=>gates.f(f));gates.u(8052);gates.u(0);[0,1,0,90].forEach(f=>gates.f(f));
    const start=binary();[0,0,0].forEach(f=>start.f(f));
    const data=[['ALP2.mdl',model(terrain)],['bike1.mdl',model([[-1,0,0],[1,0,0],[0,2,0]])],['ALP2.col',col.done()],['ALP2.gates',gates.done()],['ALP2.start.pts',start.done()]];
    const dt=new DataTransfer();for(const [name,bytes]of data)dt.items.add(new File([bytes],name));const input=document.getElementById('files');input.files=dt.files;input.dispatchEvent(new Event('change'));
  });
  await page.waitForFunction(()=>!document.getElementById('play').disabled,null,{timeout:20000});
  results.syntheticReady=await page.locator('#status').textContent();
  await page.locator('#play').click();
  await page.locator('#canvas').focus();await page.keyboard.down('w');
  await page.waitForFunction(()=>window.downhillMetrics?.speed>10,null,{timeout:10000});await page.keyboard.up('w');
  results.syntheticRide=await page.evaluate(()=>window.downhillMetrics);
  results.glError=await page.locator('#canvas').evaluate(c=>c.getContext('webgl2').getError());
  if(results.glError!==0||results.syntheticRide.calls<2)throw new Error('GPU draw failed');
  await page.evaluate(()=>{const gl=document.getElementById('canvas').getContext('webgl2');window.testLose=gl.getExtension('WEBGL_lose_context');window.testLose.loseContext();});
  await page.waitForFunction(()=>document.getElementById('error').textContent.includes('Contexto WebGL perdido'));
  await page.evaluate(()=>window.testLose.restoreContext());
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('GPU recuperada'),null,{timeout:10000});
  results.restore=await page.locator('#status').textContent();
  await page.evaluate(()=>{window.downhillMetrics=undefined;});
  await page.locator('#play').click();await page.waitForFunction(()=>window.downhillMetrics?.calls>0);
  results.afterRestore=await page.evaluate(()=>window.downhillMetrics);
  await page.locator('#load').focus();
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('En pausa'));
  results.pause=await page.locator('#status').textContent();
  await page.reload();await page.waitForFunction(()=>!document.getElementById('load').disabled);
  return results;
}
