// Web Bluetooth transport for Oishia. This source is embedded into dashboard.html
// by tools/embed_dashboard.py. Works from HTTPS or localhost in supported browsers.
class OishiaBluetoothTransport {
  static SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
  static RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
  static TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
  constructor(onDisconnect) {
    this.onDisconnect = onDisconnect;
    this.device = null;
    this.rx = null;
    this.buffer = '';
    this.decoder = new TextDecoder();
    this.pending = new Map();
    this.sequence = 0;
    this.serial = Promise.resolve();
    this.generation = 0;
  }
  async connect() {
    if (!window.isSecureContext || !navigator.bluetooth) {
      throw new Error('Bluetooth needs a supported browser on HTTPS or localhost. Use Wi-Fi here, or open the local Bluetooth controller.');
    }
    this.device = await navigator.bluetooth.requestDevice({filters:[{namePrefix:'Oishia-'}],optionalServices:[OishiaBluetoothTransport.SERVICE]});
    this.device.addEventListener('gattserverdisconnected', () => {
      this.generation++;
      this.rx = null;
      this.buffer = '';
      this.decoder = new TextDecoder();
      for (const request of this.pending.values()) {
        clearTimeout(request.timer);
        request.reject(new Error('Bluetooth disconnected. Reconnect to Oishia.'));
      }
      this.pending.clear();
      this.onDisconnect?.();
    });
    try {
      const server = await this.device.gatt.connect();
      const service = await server.getPrimaryService(OishiaBluetoothTransport.SERVICE);
      this.rx = await service.getCharacteristic(OishiaBluetoothTransport.RX);
      const tx = await service.getCharacteristic(OishiaBluetoothTransport.TX);
      tx.addEventListener('characteristicvaluechanged', event => this.receive(event.target.value));
      await tx.readValue(); // Finish passkey pairing before starting request timeouts.
      await tx.startNotifications();
    } catch (error) { this.disconnect(); throw error; }
  }
  receive(value) {
    this.buffer += this.decoder.decode(value,{stream:true});
    if(this.buffer.length>12000){this.disconnect();return;}
    let newline;
    while((newline=this.buffer.indexOf('\n'))>=0){
      const line=this.buffer.slice(0,newline);this.buffer=this.buffer.slice(newline+1);
      try{
        const response=JSON.parse(line), pending=this.pending.get(response.id);
        if(!pending)continue;
        this.pending.delete(response.id);clearTimeout(pending.timer);
        if(response.status>=400){const error=new Error(response.body?.error||'Oishia could not complete that request.');error.status=response.status;pending.reject(error);}
        else pending.resolve(response.body);
      }catch{/* Ignore malformed or unrelated frames; the request has its own timeout. */}
    }
  }
  request(path,body,key) {
    // Serialize complete requests, not just individual writes. Never interleave frames.
    const generation = this.generation;
    const task = this.serial.then(()=>{if(generation!==this.generation)throw new Error('Bluetooth session ended.');return this.send(path,body,key);});
    this.serial=task.catch(()=>{});
    return task;
  }
  async send(path,body,key) {
    if(!this.rx || !this.device?.gatt.connected)throw new Error('Reconnect Bluetooth to Oishia.');
    const operations={'/api/settings':'settings','/api/auth':'auth','/api/logout':'logout','/api/status':'status','/api/history':'history','/api/events':'events','/api/command':'command','/api/wifi':'wifi','/api/networks':'networks','/api/wifi/forget':'forget'};
    if(!operations[path])throw new Error('Unsupported Bluetooth operation.');
    const id=++this.sequence;
    const frame=new TextEncoder().encode(JSON.stringify({...body,id,key,op:operations[path]})+'\n');
    if(frame.length>1537)throw new Error('Request is too large for Bluetooth.');
    const result=new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>{
        this.pending.delete(id);
        reject(new Error('Bluetooth reply timed out. Check Oishia before trying the command again.'));
        // Drop partial frames and any queued commands after a timeout.
        this.disconnect();
      },12000);
      this.pending.set(id,{resolve,reject,timer});
    });
    // Register rejection handling before writes; a disconnect can reject during a write.
    result.catch(()=>{});
    try{
      for(let offset=0;offset<frame.length;offset+=20){
        if(!this.rx)throw new Error('Bluetooth disconnected.');
        await this.rx.writeValueWithResponse(frame.slice(offset,offset+20));
      }
    }catch(error){
      const pending=this.pending.get(id);
      if(pending){clearTimeout(pending.timer);this.pending.delete(id);pending.reject(error);}
      this.disconnect();
    }
    return result;
  }
  disconnect(){
    this.generation++;
    this.rx=null;
    if(this.device?.gatt.connected)this.device.gatt.disconnect();
  }
}
