// Integration checks against the real page with a simulated ESP32 API.
// Start Chrome with --headless --remote-debugging-port=9333 and run with Node 22+.
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const html = await readFile(root + 'learning/dashboard.html', 'utf8');
const bleSource = await readFile(root + 'learning/dashboard-ble.js', 'utf8');
const faceSource = await readFile(root + 'learning/dashboard-face.js', 'utf8');
const useBle = process.env.OISHIA_TEST_BLE === '1';
const endpoint = process.env.OISHIA_CHROME_URL || 'http://127.0.0.1:9333';
const tab = await (await fetch(endpoint + '/json/new?about:blank', { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
let sequence = 0;
const pending = new Map();
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => { const id = ++sequence; pending.set(id, { resolve, reject }); ws.send(JSON.stringify({ id, method, params })); });
}
const key = '0123';
const token = '0123456789abcdef0123456789abcdef';
let tokenActive = false;
const status = {
  pet: { mood: 'HAPPY', message: 'Happy you are here!', happiness: 78, friendship: 3, pets: 61, visits: 24, sleeping: false, muted: false, memoryHealthy:true, memoryPending:false, tiltReady:true, temperature: 24.5, humidity: 52, light: 1200, dark: false, motion: true, pirWarming:false, pirStuck:false, clockVisible: false, presenceQuietSeconds: 0, distance: null },
  network: { connected: false, connecting: false, setupActive: true, hotspot: 'OishiaPikku', hostname: 'oishia-123456', ip: '', ssid: '', settingsResult: 'idle', storageReady: true, testing: false, bluetoothReady:true, bluetoothConnected:useBle },
  device:{firmware:'1.0.0',apiVersion:3,freeHeap:150000,largestFreeBlock:100000,maxLoopMicros:900}
};
const settingsDefaults={petName:'Oishia',ownerName:'Sathu',clockWhenAway:true,clockAfterSeconds:120,clock24Hour:true,clockShowSeconds:true,clockStyle:0,portraitEnabled:true,soundProfile:1,timezone:'Asia/Tokyo',autoSleep:true,tiltInvert:false,sleepAfterSeconds:30,motionCooldownSeconds:10,pirStuckSeconds:300,darkThreshold:800,brightThreshold:1100,comfortableMinTenths:180,comfortableMaxTenths:290,comfortableMinHumidity:30,comfortableMaxHumidity:78,displayDayContrast:180,displayNightContrast:35,displayOffQuiet:false,quietHoursEnabled:false,quietStartHour:22,quietStartMinute:0,quietEndHour:7,quietEndMinute:0,routines:Array.from({length:3},()=>({enabled:false,action:0,hour:8,minute:0,weekdays:127,message:'Good morning!'}))};
status.settings={...settingsDefaults};status.settingsDefaults=settingsDefaults;
status.time={synced:false,timezone:'Asia/Tokyo',source:'NTP',local:null};
let rejectSettings=false, settingsSaves=0;
let offline = false, rejectCommand = false, scanPolls = 0, forgets = 0;
const commands = [], wifiRequests = [], browserErrors = [];
async function intercept(params) {
  const { requestId, request } = params;
  const url = new URL(request.url);
  if (offline && url.pathname.startsWith('/api/')) { await cdp('Fetch.failRequest', { requestId, errorReason: 'ConnectionFailed' }); return; }
  let code = 200, result, contentType = 'application/json';
  const headers = Object.fromEntries(Object.entries(request.headers).map(([k, v]) => [k.toLowerCase(), v]));
  if (url.pathname === '/') { result = html; contentType = 'text/html'; }
  else if (url.pathname === '/dashboard-ble.js') { result = bleSource; contentType = 'text/javascript'; }
  else if (url.pathname === '/dashboard-face.js') { result = faceSource; contentType = 'text/javascript'; }
  else if (url.pathname === '/favicon.ico') { result = ''; code = 204; }
  else if (url.pathname === '/api/auth' && headers['x-oishia-key'] === key) { tokenActive=true; result={token,expiresIn:1800}; }
  else if (!tokenActive || headers['x-oishia-key'] !== token) { code = 401; result = { error: 'That PIN does not match Oishia.' }; }
  else if (url.pathname === '/api/logout') {tokenActive=false;result={message:'Dashboard locked.'};}
  else if(url.pathname==='/api/settings'){if(rejectSettings){code=503;result={error:'Settings could not be saved. Previous values are still active.'};}else{status.settings=JSON.parse(request.postData);settingsSaves++;result={settings:status.settings};}}
  else if (url.pathname === '/api/status') result = status;
  else if (url.pathname === '/api/history') result={uptimeSeconds:[60,120],epoch:[null,null],temperature:[24.2,24.5],humidity:[51,52],light:[1180,1200]};
  else if (url.pathname === '/api/events') result={events:[{uptimeSeconds:1,epoch:null,type:'system',text:'Oishia started'}]};
  else if (url.pathname === '/api/command') {
    if (rejectCommand) { code = 503; result = { error: 'Oishia is busy. Please try again.' }; }
    else {
      const body = JSON.parse(request.postData); commands.push(body);
      if (body.action === 'sleep') { status.pet.sleeping = true; status.pet.mood = 'SLEEP'; }
      if (body.action === 'wake') { status.pet.sleeping = false; status.pet.mood = 'HAPPY'; }
      if (body.action === 'love') { status.pet.sleeping = false; status.pet.mood = 'LOVE'; }
      if (body.action === 'message') status.pet.message = body.text;
      if (body.action === 'mute') status.pet.muted = true;
      if (body.action === 'unmute') status.pet.muted = false;
      if (body.action === 'set_time') { assert.ok(body.epoch>=1704067200); status.time={synced:true,timezone:status.settings.timezone,source:'dashboard',local:'2026-09-27 12:34:56'}; }
      code = 202; result = { message: 'Sent to Oishia.' };
    }
  } else if (url.pathname === '/api/networks') {
    result = ++scanPolls === 1 ? {scanning:true} : {scanning:false,networks:[{ssid:'Home <&> Wi-Fi',rssi:-42,open:false}]};
  } else if (url.pathname === '/api/wifi/forget') {
    assert.equal(JSON.parse(request.postData).confirm,'forget-wifi');forgets++;code=202;result={message:'Returning to setup. Friendship memory is kept.'};status.network.settingsResult='forgotten';
  } else if (url.pathname === '/api/wifi') {
    wifiRequests.push(JSON.parse(request.postData)); status.network.testing = true; status.network.settingsResult = 'testing'; code = 202; result = { message: 'Trying your network.' };
  } else { code = 404; result = { error: 'Missing' }; }
  await cdp('Fetch.fulfillRequest', { requestId, responseCode: code, responseHeaders: [{ name: 'Content-Type', value: contentType }, { name: 'Cache-Control', value: 'no-store' }], body: Buffer.from(Buffer.isBuffer(result)?result:typeof result === 'string' ? result : JSON.stringify(result)).toString('base64') });
}
ws.onmessage = event => {
  const message = JSON.parse(event.data);
  if (message.id) { const p = pending.get(message.id); pending.delete(message.id); if (message.error) p.reject(message.error); else p.resolve(message.result); }
  else if (message.method === 'Fetch.requestPaused') intercept(message.params).catch(error => browserErrors.push(String(error)));
  else if (message.method === 'Runtime.exceptionThrown') browserErrors.push(message.params.exceptionDetails.text);
};
async function evaluate(expression) {
  const result = await cdp('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
  if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result.value;
}
async function until(expression, description) {
  const deadline = Date.now() + 10000;
  while (Date.now() < deadline) { if (await evaluate(expression)) return; await new Promise(resolve => setTimeout(resolve, 100)); }
  throw new Error('Timed out: ' + description);
}
async function screenshot(name, width, height) {
  await cdp('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile: width < 600 });
  await evaluate('window.scrollTo(0,0)');
  assert.equal(await evaluate('document.documentElement.scrollWidth <= innerWidth'), true, 'No horizontal overflow at ' + width);
  const layout = await cdp('Page.getLayoutMetrics');
  const size = layout.cssContentSize;
  const shot = await cdp('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true, clip: { x: 0, y: 0, width: size.width, height: size.height, scale: 1 } });
  await writeFile(root + 'designs/' + name, Buffer.from(shot.data, 'base64'));
}
try {
  await cdp('Page.enable'); await cdp('Runtime.enable'); await cdp('Fetch.enable', { patterns: [{ urlPattern: '*' }] });
  if(useBle) await cdp('Page.addScriptToEvaluateOnNewDocument',{source:`
    // Emulate only the GATT boundary. The production transport handles framing,
    // request IDs, chunked writes, response parsing, serialization and reconnect.
    Object.defineProperty(navigator,'bluetooth',{value:{async requestDevice(){
      const device=new EventTarget(), tx=new EventTarget();let input='';
      tx.readValue=async()=>new DataView(new ArrayBuffer(0));
      tx.startNotifications=async()=>tx;
      const rx={async writeValueWithResponse(bytes){
        if(bytes.length>20)throw new Error('Oversized BLE write');
        input+=new TextDecoder().decode(bytes);
        if(!input.endsWith('\\n'))return;
        const request=JSON.parse(input);input='';
        const paths={settings:'/api/settings',auth:'/api/auth',logout:'/api/logout',status:'/api/status',history:'/api/history',events:'/api/events',command:'/api/command',wifi:'/api/wifi',networks:'/api/networks',forget:'/api/wifi/forget'};
        const read=['status','history','events','networks'].includes(request.op);
        try{
          const response=await fetch(paths[request.op],{method:read?'GET':'POST',headers:{'X-Oishia-Key':request.key,'Content-Type':'application/json'},body:read?undefined:JSON.stringify(request)});
          const frame=new TextEncoder().encode(JSON.stringify({id:request.id,status:response.status,body:await response.json()})+'\\n');
          for(let i=0;i<frame.length;i+=13){
            const part=frame.slice(i,i+13);tx.value=new DataView(part.buffer);tx.dispatchEvent(new Event('characteristicvaluechanged'));
          }
        }catch(error){device.gatt.disconnect();throw error;}
      }};
      const service={async getCharacteristic(uuid){return uuid.startsWith('6e400002')?rx:tx;}};
      device.gatt={connected:false,async connect(){this.connected=true;return{async getPrimaryService(){return service;}};},disconnect(){if(this.connected){this.connected=false;device.dispatchEvent(new Event('gattserverdisconnected'));}}};
      window.testBleDevice=device;return device;
    }}});
  `});
  await cdp('Emulation.setDeviceMetricsOverride', { width: 1280, height: 1100, deviceScaleFactor: 1, mobile: false });
  await cdp('Page.navigate', { url: useBle ? 'https://oishia.test/' : 'http://oishia.test/' });
  await until('document.readyState === "complete" && !!document.getElementById("loginForm")', 'login and transport loaded');
  await evaluate(`document.querySelector('input[value=${useBle?'ble':'wifi'}]').click()`);
  await evaluate(`document.getElementById('accessKey').value='9999'; document.querySelector('#loginForm button').click()`);
  await until(`document.getElementById('loginNotice').textContent.includes('does not match')`, 'incorrect key rejected');
  await evaluate(`document.getElementById('accessKey').value='${key}'; document.querySelector('#loginForm button').click()`);
  await until(`!document.getElementById('dashboard').hidden`, 'dashboard unlocked');
  assert.equal(await evaluate(`sessionStorage.getItem('oishiaKey')`), token, 'store token, never the PIN');
  assert.equal(await evaluate(`document.getElementById('temperature').textContent`), '24.5°C');
  assert.equal(await evaluate(`document.getElementById('wifiDetails').open`), true);
  assert.equal(await evaluate(`document.getElementById('petName').value`),'Oishia');
  await until(`document.getElementById('petFace').dataset.mood==='HAPPY'`,'OLED expression rendered after authentication');
  // Every OLED expression must render in both modes, without external images.
  assert.equal(await evaluate(`document.querySelector('#petIllustration img')===null`),true);
  const faceChecks=await evaluate(`(() => {
    const canvas=document.createElement('canvas');canvas.width=128;canvas.height=64;
    return [true,false].map(portrait=>['IDLE','HAPPY','LOVE','SLEEP','THINKING','CURIOUS','EXCITED','SAD','ANGRY','GREETING'].map(mood=>{
      drawOishiaFace(canvas,mood,portrait);
      const pixels=canvas.getContext('2d').getImageData(0,0,128,64).data;
      return {mood,lit:pixels.some((v,i)=>i%4===0&&v>0),image:canvas.toDataURL()};
    }));
  })()`);
  for(const faces of faceChecks){
    assert.ok(faces.every(face=>face.lit),'all OLED moods draw visible pixels');
    assert.equal(new Set(faces.slice(0,9).map(face=>face.image)).size,9,'each mood has its own expression');
  }
  assert.equal(await evaluate(`document.getElementById('timezone').value`),'Asia/Tokyo');
  await evaluate(`document.getElementById('historyDetails').open=true`);
  await until(`document.getElementById('historySummary').textContent.includes('2 samples')`,'sensor history rendered');
  assert.equal(await evaluate(`document.getElementById('eventList').textContent.includes('Oishia started')`),true,'activity event rendered');
  assert.equal(await evaluate(`document.getElementById('healthGrid').textContent.includes('1.0.0')`),true,'health summary rendered');
  await until(`document.getElementById('timeStatus').textContent.includes('Time ready from dashboard clock')`,'browser clock fallback');
  assert.equal(commands.some(command=>command.action==='set_time'),true);
  assert.equal(await evaluate(`document.getElementById('timeStatus').textContent.includes('Motion is detected')`),true);
  await evaluate(`document.getElementById('personalDetails').open=true;document.getElementById('previewClock').click()`);
  await until(`document.getElementById('settingsNotice').textContent.includes('Clock preview sent')`,'clock preview sent');
  assert.equal(commands.at(-1).action,'clock');
  await evaluate(`document.getElementById('personalDetails').open=true;document.getElementById('petName').value='Pikku';document.getElementById('ownerName').value='Alex';document.getElementById('timezone').value='America/New_York';document.getElementById('clockAfterSeconds').value=300;document.getElementById('quietHoursEnabled').checked=true;document.getElementById('soundProfile').value=2;document.getElementById('portraitEnabled').checked=false;document.getElementById('routine0Enabled').checked=true;document.getElementById('routine0Time').value='07:30';document.getElementById('routine0Message').value='Good morning Alex';document.getElementById('petName').dispatchEvent(new Event('input',{bubbles:true}));`);
  await new Promise(resolve=>setTimeout(resolve,2300));
  assert.equal(await evaluate(`document.getElementById('petName').value`),'Pikku','poll preserves unsaved edits');
  rejectSettings=true;
  await evaluate(`document.getElementById('settingsSave').click()`);
  await until(`document.getElementById('settingsNotice').textContent.includes('could not be saved')`,'failed persistence reported');
  assert.equal(settingsSaves,0);assert.equal(status.settings.petName,'Oishia');rejectSettings=false;
  await evaluate(`document.getElementById('settingsSave').click()`);
  await until(`document.title.startsWith('Pikku')`,'personal name applied');
  assert.equal(status.settings.ownerName,'Alex');
  assert.equal(status.settings.timezone,'America/New_York');
  assert.equal(status.settings.clockAfterSeconds,300);
  assert.equal(status.settings.quietHoursEnabled,true);
  assert.equal(status.settings.soundProfile,2);
  assert.deepEqual(status.settings.routines[0],{enabled:true,action:0,hour:7,minute:30,weekdays:127,message:'Good morning Alex'});
  await until(`document.getElementById('petFace').dataset.detail==='false'`,'simple pet face applied');
  await evaluate(`document.getElementById('petName').value='<img src=x>';document.getElementById('settingsSave').click()`);
  await until(`document.title.startsWith('<img src=x>')`,'name is literal text');
  assert.equal(await evaluate(`document.querySelector('[data-pet-name]').children.length`),0);
  await evaluate(`document.getElementById('settingsDefaults').click()`);
  assert.equal(status.settings.petName,'<img src=x>','defaults do not save automatically');
  assert.equal(await evaluate(`document.getElementById('petName').value`),'Oishia');
  await evaluate(`document.getElementById('settingsSave').click()`);
  await until(`document.title.startsWith('Oishia')`,'default names saved');
  await evaluate(`document.getElementById('darkThreshold').value=1200;document.getElementById('settingsSave').click()`);
  await until(`document.getElementById('settingsNotice').textContent.includes('Bright must')`,'invalid thresholds rejected');
  await evaluate(`document.getElementById('settingsCancel').click();document.getElementById('personalDetails').open=false;`);
  await evaluate(`document.getElementById('scanButton').click()`);
  await until(`document.querySelector('#networkList button')?.textContent.includes('Home <&> Wi-Fi')`, 'network discovery');
  await evaluate(`document.querySelector('#networkList button').click()`);
  assert.equal(await evaluate(`document.getElementById('ssid').value`),'Home <&> Wi-Fi');
  await evaluate(`document.getElementById('ssid').value='My unfinished network'; document.querySelector('[data-action="love"]').click()`);
  await until(`document.getElementById('actionNotice').textContent.includes('little love')`, 'love sent');
  assert.equal(commands.at(-1).action, 'love');
  await evaluate(`document.querySelector('[data-action="sleep"]').click()`);
  await until(`(document.getElementById('petFace').dataset.mood==='SLEEP')`, 'sleep status reflected');
  assert.equal(await evaluate(`document.getElementById('ssid').value`), 'My unfinished network', 'polling preserves form');
  await evaluate(`document.querySelector('[data-action="wake"]').click()`);
  await until(`!(document.getElementById('petFace').dataset.mood==='SLEEP')`, 'wake status reflected');
  await evaluate(`document.getElementById('muteButton').click()`);
  await until(`document.getElementById('muteButton').getAttribute('aria-pressed')==='true'`, 'mute state reflected');
  await evaluate(`document.getElementById('message').value='Hello ♡'; document.querySelector('#messageForm button').click()`);
  await until(`document.getElementById('actionNotice').textContent.includes('Use 1–21')`, 'unsupported characters rejected');
  const injection = '<img src=x onerror=1>';
  await evaluate(`document.getElementById('message').value=${JSON.stringify(injection)}; document.querySelector('#messageForm button').click()`);
  await until(`document.getElementById('petMessage').textContent===${JSON.stringify(injection)}`, 'note displayed as text');
  assert.equal(await evaluate(`document.getElementById('petMessage').children.length`), 0, 'message cannot inject markup');
  rejectCommand = true;
  await evaluate(`document.querySelector('[data-action="love"]').click()`);
  await until(`document.getElementById('actionNotice').textContent.includes('Oishia is busy')`, 'server rejection surfaced');
  rejectCommand = false;
  await evaluate(`document.getElementById('wifiPassword').value='short';document.getElementById('wifiSubmit').click()`);
  await until(`document.getElementById('wifiNotice').textContent.includes('8–63')`, 'short password rejected');
  assert.equal(wifiRequests.length, 0);
  await evaluate(`document.getElementById('wifiPassword').value='correct horse';document.getElementById('wifiSubmit').click()`);
  await until(`document.getElementById('wifiNotice').textContent.includes('Trying Wi-Fi')`, 'Wi-Fi submitted');
  assert.equal(wifiRequests.length, 1);
  assert.equal(wifiRequests[0].ssid, 'My unfinished network');
  assert.equal(await evaluate(`document.getElementById('wifiPassword').value`), '', 'password cleared after submission');
  status.network.testing = false; status.network.settingsResult = 'failed';
  await until(`document.getElementById('wifiResult').textContent.includes('previous settings were kept')`, 'failed setup is explained');
  await until(`document.getElementById('motion').textContent==='Detected'`, 'PIR motion displayed');
  status.pet.motion = false;
  await until(`document.getElementById('motion').textContent==='Quiet'`, 'no motion is not missing data');
  status.pet.motion = null; status.pet.temperature = null;
  await until(`document.getElementById('motion').textContent==='—'&&document.getElementById('temperature').textContent==='—'`, 'missing readings are not zero');
  offline = true;
  await until(`!document.getElementById('connectionWarning').hidden`, 'offline feedback');
  assert.equal(await evaluate(`document.querySelector('[data-action="love"]').disabled`), true);
  const count = commands.length;
  offline = false;
  if(useBle)await evaluate(`document.getElementById('reconnectBle').click()`);
  await until(`document.getElementById('connectionWarning').hidden`, 'reconnected');
  assert.equal(commands.length, count, 'commands are not replayed');
  status.pet.temperature = 24.5; status.pet.motion = true; status.pet.message = 'Happy you are here!';status.pet.mood = 'HAPPY';
  status.network.connected = true;status.network.ssid = 'Our little home';status.network.ip = '192.168.1.42';status.network.settingsResult = 'saved';
  await until(`document.getElementById('homeLink').textContent==='http://192.168.1.42'`, 'new dashboard address shown');
  await evaluate(`document.getElementById('wifiDetails').open=false;document.getElementById('actionNotice').textContent='';`);
  if(!useBle){await screenshot('oishia-dashboard-desktop.png', 1280, 1100);await screenshot('oishia-dashboard-mobile.png', 390, 844);}
  await evaluate(`document.getElementById('forgetButton').click()`);
  assert.equal(forgets,0,'forget requires explicit confirmation');
  await evaluate(`document.getElementById('cancelForget').click()`);
  assert.equal(forgets,0,'cancel preserves Wi-Fi');
  await evaluate(`document.getElementById('forgetButton').click();document.getElementById('confirmForget').click()`);
  await until(`document.getElementById('networkNotice').textContent.includes('Friendship memory is kept')`,'forget result');
  assert.equal(forgets,1);
  await evaluate(`document.getElementById('lockButton').click()`);
  await until(`document.getElementById('dashboard').hidden`, 'logout acknowledged and dashboard locked');
  assert.equal(tokenActive,false,'logout revokes the device session');
  assert.equal(await evaluate(`sessionStorage.getItem('oishiaKey')`), null);
  if(useBle){
    await cdp('Page.reload');
    await until(`document.readyState === 'complete' && !!document.querySelector('input[value=ble]')`, 'reload login and transport');
    assert.equal(await evaluate(`document.querySelector('input[value=ble]').checked`),true,'BLE selection survives reload without replay');
    assert.equal(await evaluate(`document.getElementById('dashboard').hidden`),true,'BLE reload requires a fresh connection');
  }
  assert.deepEqual(browserErrors, []);
  console.log((useBle?'Bluetooth':'Wi-Fi')+' dashboard checks passed: authentication, controls, messages, network discovery, forget confirmation, form preservation, setup feedback, missing sensors, offline/reconnect, and no replay.');
} catch(error) {
  console.error(await evaluate(`JSON.stringify({login:document.getElementById('loginNotice')?.textContent,badge:document.getElementById('connectionBadge')?.textContent,mode:document.querySelector('input[name=transport]:checked')?.value})`).catch(()=>''));
  throw error;
} finally {
  await cdp('Page.close').catch(()=>{});
  ws.close();
}
