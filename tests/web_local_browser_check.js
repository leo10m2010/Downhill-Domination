// SPDX-License-Identifier: GPL-3.0-or-later
// Requires the user's private ALP2 package prepared with prepare_local_game.py.
async (page) => {
  const requests=[];
  page.on('request',r=>requests.push({method:r.method(),url:r.url()}));
  await page.reload();
  const origin=await page.evaluate(()=>location.origin);
  await page.waitForFunction(()=>!document.getElementById('play').disabled || !document.getElementById('error').hidden,null,{timeout:30000});
  if(await page.locator('#play').isDisabled())throw new Error(await page.locator('#error').textContent());
  if(await page.locator('#load').isVisible())throw new Error('Manual file button must be hidden in local mode');
  if(await page.locator('#asset-help').isVisible())throw new Error('File preparation menu must be hidden in local mode');
  const ready=await page.locator('#status').textContent();
  const loading=await page.locator('#loadtime').textContent();
  await page.locator('#play').click();
  await page.locator('#canvas').focus();await page.keyboard.down('w');
  await page.waitForFunction(()=>window.downhillMetrics?.state===2&&window.downhillMetrics?.speed>10,null,{timeout:15000});
  await page.keyboard.up('w');
  const ride=await page.evaluate(()=>window.downhillMetrics);
  const gl=await page.locator('#canvas').evaluate(c=>c.getContext('webgl2').getError());
  if(gl!==0||ride.total!==28)throw new Error(`ALP2 failed: GL=${gl}, gates=${ride.total}`);
  await page.evaluate(()=>{const gl=document.getElementById('canvas').getContext('webgl2');window.testLose=gl.getExtension('WEBGL_lose_context');window.testLose.loseContext();});
  await page.waitForFunction(()=>document.getElementById('error').textContent.includes('Contexto WebGL perdido'));
  await page.evaluate(()=>window.testLose.restoreContext());
  await page.waitForFunction(()=>document.getElementById('status').textContent.includes('GPU recuperada'),null,{timeout:10000});
  await page.evaluate(()=>{window.downhillMetrics=undefined;});
  await page.locator('#play').click();
  await page.waitForFunction(()=>window.downhillMetrics?.calls>0);
  const restored=await page.evaluate(()=>window.downhillMetrics);
  if(requests.some(r=>r.method!=='GET'||!r.url.startsWith(origin+'/')||r.url.toLowerCase().includes('.iso')))throw new Error('Unexpected external request or upload');
  return {ready,loading,ride,restored,gl,requests};
}
