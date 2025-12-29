
function setDeviceInfo(dev){
  if(!dev) return;
  setText("ui-board", dev.board ?? dev.Board ?? dev.board_name ?? "-");
  setText("ui-serial", dev.serial ?? dev.sn ?? dev.board_sn ?? "-");
  setText("ui-fw", dev.fw ?? dev.FW ?? dev.firmware ?? "-");
  setText("ui-hw", dev.hw ?? dev.HW ?? dev.hardware ?? "-");
  setText("ui-mac", dev.mac ?? dev.MAC ?? "-");
  setText("ui-mfg", dev.mfg ?? dev.manufacturer ?? "-");
  setText("ui-year", dev.year ?? dev.make_year ?? "-");
}
/* CortexLink A8R-M MQTT Dashboard (Browser)
 * - Loads MQTT connection info from /api/mqttinfo (served by ESP32)
 * - Connects to Mosquitto via WebSockets (mqtt.js)
 * - Subscribes to firmware topics from MQTT_Manager.cpp
 */

let mqttClient = null;
let mqttInfo = null;

const ui = {
  mqttStatus: document.getElementById("ui-mqtt-status"),
  mqttDot: document.getElementById("ui-dot-mqtt"),
  debug: document.getElementById("ui-debug"),

  apSsid: document.getElementById("ui-ap-ssid"),
  apIp: document.getElementById("ui-ap-ip"),
  rtc: document.getElementById("ui-rtc"),

  clientId: document.getElementById("ui-clientid"),
  fullBase: document.getElementById("ui-fullbase"),
  online: document.getElementById("ui-online"),

  // device info
  board: document.getElementById("ui-board"),
  serial: document.getElementById("ui-serial"),
  fw: document.getElementById("ui-fw"),
  hw: document.getElementById("ui-hw"),
  mfg: document.getElementById("ui-mfg"),
  year: document.getElementById("ui-year"),
  mac: document.getElementById("ui-mac"),
  // MAC group
  macEth: document.getElementById("ui-mac-eth"),
  macSta: document.getElementById("ui-mac-sta"),
  macAp: document.getElementById("ui-mac-ap"),
  macEfuse: document.getElementById("ui-mac-efuse"),

  relays: document.getElementById("ui-relays"),
  dis: document.getElementById("ui-dis"),
  ais: document.getElementById("ui-ais"),
  sensors: document.getElementById("ui-sensors"),
  dacs: document.getElementById("ui-dacs"),

  errorBox: document.getElementById("ui-errorbox"),
  errText: document.getElementById("ui-errortext"),
};

const NUM_RELAYS = 6;
const NUM_DIS = 8;

function showError(msg) {
  if (!ui.errorBox || !ui.errText) return;
  ui.errorBox.style.display = "block";
  ui.errText.textContent = msg;
}

function clearError() {
  if (!ui.errorBox || !ui.errText) return;
  ui.errorBox.style.display = "none";
  ui.errText.textContent = "";
}

function updateMacSummary() {
  // prefer ETH then STA then AP then EFUSE
  const best = (ui.macEth?.textContent && ui.macEth.textContent !== "-") ? ui.macEth.textContent
    : (ui.macSta?.textContent && ui.macSta.textContent !== "-") ? ui.macSta.textContent
    : (ui.macAp?.textContent && ui.macAp.textContent !== "-") ? ui.macAp.textContent
    : (ui.macEfuse?.textContent && ui.macEfuse.textContent !== "-") ? ui.macEfuse.textContent
    : "-";
  if (ui.mac) ui.mac.textContent = best;
}

function setMqttUi(connected, text) {
  ui.mqttStatus.textContent = text;
  ui.mqttDot.classList.toggle("ok", !!connected);
}

function safeJsonParse(payload) {
  try { return JSON.parse(payload); } catch { return null; }
}

function buildStaticUi() {
  // Relays
  ui.relays.innerHTML = "";
  for (let i = 1; i <= NUM_RELAYS; i++) {
    const el = document.createElement("div");
    el.className = "relay";
    el.innerHTML = `
      <div class="relay-head">
        <div class="relay-name">Relay ${i}</div>
        <div class="badge" id="relay-badge-${i}">?</div>
      </div>
      <label class="switch">
        <input type="checkbox" id="relay-sw-${i}">
        <span class="slider"></span>
      </label>
    `;
    ui.relays.appendChild(el);

    el.querySelector(`#relay-sw-${i}`).addEventListener("change", (e) => {
      publishRelaySet(i, e.target.checked ? 1 : 0);
    });
  }

  // Digital Inputs
  ui.dis.innerHTML = "";
  for (let i = 1; i <= NUM_DIS; i++) {
    const el = document.createElement("div");
    el.className = "di";
    el.innerHTML = `
      <div class="di-name">DI${i}</div>
      <div class="di-dot" id="di-dot-${i}"></div>
      <div class="di-val" id="di-val-${i}">-</div>
    `;
    ui.dis.appendChild(el);
  }

  // Analog placeholders
  ui.ais.innerHTML = `
    <div class="analog"><div class="analog-name">V1</div><div class="analog-val" id="ai-v1">-</div><div class="analog-sub">mV</div></div>
    <div class="analog"><div class="analog-name">V2</div><div class="analog-val" id="ai-v2">-</div><div class="analog-sub">mV</div></div>
    <div class="analog"><div class="analog-name">I1</div><div class="analog-val" id="ai-i1">-</div><div class="analog-sub">mA</div></div>
    <div class="analog"><div class="analog-name">I2</div><div class="analog-val" id="ai-i2">-</div><div class="analog-sub">mA</div></div>
  `;

  ui.sensors.innerHTML = `
    <div class="sensor"><div class="sensor-name">DHT1</div><div class="sensor-val" id="s-dht1">-</div><div class="sensor-sub" id="s-dht1-sub">-</div></div>
    <div class="sensor"><div class="sensor-name">DHT2</div><div class="sensor-val" id="s-dht2">-</div><div class="sensor-sub" id="s-dht2-sub">-</div></div>
    <div class="sensor"><div class="sensor-name">DS18B20</div><div class="sensor-val" id="s-ds18">-</div><div class="sensor-sub" id="s-ds18-sub">-</div></div>
  `;

  ui.dacs.innerHTML = `
    <div class="dac">
      <div class="dac-name">DAC1</div>
      <div class="dac-val" id="dac1-val">-</div>
      <div class="dac-row">
        <input id="dac1-mv" type="number" min="0" step="10" value="0">
        <button class="btn small" id="dac1-set">Set mV</button>
      </div>
    </div>
    <div class="dac">
      <div class="dac-name">DAC2</div>
      <div class="dac-val" id="dac2-val">-</div>
      <div class="dac-row">
        <input id="dac2-mv" type="number" min="0" step="10" value="0">
        <button class="btn small" id="dac2-set">Set mV</button>
      </div>
    </div>
  `;

  document.getElementById("dac1-set").addEventListener("click", () => {
    const mv = parseInt(document.getElementById("dac1-mv").value || "0", 10);
    publishDacMvSet(1, mv);
  });
  document.getElementById("dac2-set").addEventListener("click", () => {
    const mv = parseInt(document.getElementById("dac2-mv").value || "0", 10);
    publishDacMvSet(2, mv);
  });

  document.getElementById("btn-relays-all-on").addEventListener("click", () => {
    for (let i = 1; i <= NUM_RELAYS; i++) publishRelaySet(i, 1);
  });
  document.getElementById("btn-relays-all-off").addEventListener("click", () => {
    for (let i = 1; i <= NUM_RELAYS; i++) publishRelaySet(i, 0);
  });

  document.getElementById("btn-buz-beep").addEventListener("click", () => {
    const freq = parseInt(document.getElementById("buz-freq").value || "2000", 10);
    const ms = parseInt(document.getElementById("buz-on").value || "200", 10);
    publishBuzzerBeep(freq, ms);
  });
  document.getElementById("btn-buz-start").addEventListener("click", () => {
    const freq = parseInt(document.getElementById("buz-freq").value || "2000", 10);
    const onMs = parseInt(document.getElementById("buz-on").value || "200", 10);
    const offMs = parseInt(document.getElementById("buz-off").value || "200", 10);
    const rep = parseInt(document.getElementById("buz-rep").value || "5", 10);
    publishBuzzerPattern(freq, onMs, offMs, rep);
  });
  document.getElementById("btn-buz-stop").addEventListener("click", () => publishBuzzerStop());

  document.getElementById("btn-req-snapshot").addEventListener("click", () => {
    if (!mqttInfo) return;
    publish(`${mqttInfo.fullBase}/cmd/request/full`, "1");
  });
}

// ---- Publish helpers aligned to MQTT_Manager.cpp ----
function publish(topic, payload) {
  if (!mqttClient || !mqttClient.connected) return;
  const p = (typeof payload === "string") ? payload : JSON.stringify(payload);
  mqttClient.publish(topic, p, { qos: 0, retain: false });
}

function publishRelaySet(relayNum, state01) {
  if (!mqttInfo) return;
  publish(`${mqttInfo.fullBase}/cmd/rel/${relayNum}/set`, String(state01 ? 1 : 0));
}

function publishDacMvSet(ch, mv) {
  if (!mqttInfo) return;
  publish(`${mqttInfo.fullBase}/cmd/dac/${ch}/mv_set`, String(mv));
}

function publishBuzzerBeep(freq, ms) {
  if (!mqttInfo) return;
  publish(`${mqttInfo.fullBase}/cmd/buzzer/beep`, JSON.stringify({ freq, ms }));
}

function publishBuzzerPattern(freq, onMs, offMs, repeats) {
  if (!mqttInfo) return;
  publish(`${mqttInfo.fullBase}/cmd/buzzer/pattern`, JSON.stringify({ freq, on: onMs, off: offMs, rep: repeats }));
}

function publishBuzzerStop() {
  if (!mqttInfo) return;
  publish(`${mqttInfo.fullBase}/cmd/buzzer/stop`, "1");
}

// ---- UI updates ----
function setRelayUi(relayNum, on) {
  const sw = document.getElementById(`relay-sw-${relayNum}`);
  const badge = document.getElementById(`relay-badge-${relayNum}`);
  if (!sw || !badge) return;
  sw.checked = !!on;
  badge.textContent = on ? "ON" : "OFF";
  badge.classList.toggle("on", !!on);
  badge.classList.toggle("off", !on);
}

function setDiUi(diNum, on) {
  const dot = document.getElementById(`di-dot-${diNum}`);
  const val = document.getElementById(`di-val-${diNum}`);
  if (!dot || !val) return;
  dot.classList.toggle("on", !!on);
  val.textContent = on ? "1" : "0";
}

function handleSnapshotJson(obj) {
  if (obj?.rtc?.iso) ui.rtc.textContent = `RTC: ${obj.rtc.iso}`;

  if (obj?.di) for (let i = 1; i <= NUM_DIS; i++) setDiUi(i, !!obj.di[String(i)]);
  if (obj?.rel) for (let i = 1; i <= NUM_RELAYS; i++) setRelayUi(i, !!obj.rel[String(i)]);

  if (obj?.analog) {
    if (obj.analog.ch1_mv != null) document.getElementById("ai-v1").textContent = `${obj.analog.ch1_mv} mV`;
    if (obj.analog.ch2_mv != null) document.getElementById("ai-v2").textContent = `${obj.analog.ch2_mv} mV`;
  }
  if (obj?.current) {
    if (obj.current.ch1_mA != null) document.getElementById("ai-i1").textContent = `${obj.current.ch1_mA} mA`;
    if (obj.current.ch2_mA != null) document.getElementById("ai-i2").textContent = `${obj.current.ch2_mA} mA`;
  }
  if (obj?.dac) {
    if (obj.dac.ch1_mv != null) document.getElementById("dac1-val").textContent = `${obj.dac.ch1_mv} mV`;
    if (obj.dac.ch2_mv != null) document.getElementById("dac2-val").textContent = `${obj.dac.ch2_mv} mV`;
  }
}

function handleMqttMessage(topic, payloadStr) {
  if (!mqttInfo) return;

  if (topic === `${mqttInfo.fullBase}/birth/online`) {
    ui.online.textContent = (payloadStr === "1") ? "Online" : "Offline";
    return;
  }
  for (let i = 1; i <= NUM_DIS; i++) {
    if (topic === `${mqttInfo.fullBase}/di/${i}`) { setDiUi(i, payloadStr === "1"); return; }
  }
  for (let i = 1; i <= NUM_RELAYS; i++) {
    if (topic === `${mqttInfo.fullBase}/rel/${i}` || topic === `${mqttInfo.fullBase}/rel/${i}/state`) {
      setRelayUi(i, payloadStr === "1");
      return;
    }
  }
  if (topic === `${mqttInfo.fullBase}/snapshot/json`) {
    const obj = safeJsonParse(payloadStr);
    if (obj) handleSnapshotJson(obj);
    return;
  }

  // Device Info topics
  if (topic === `${mqttInfo.fullBase}/info/board`)   { ui.board.textContent = payloadStr || "-"; return; }
  if (topic === `${mqttInfo.fullBase}/info/serial`)  { ui.serial.textContent = payloadStr || "-"; return; }
  if (topic === `${mqttInfo.fullBase}/info/fw`)      { ui.fw.textContent = payloadStr || "-"; return; }
  if (topic === `${mqttInfo.fullBase}/info/hw`)      { ui.hw.textContent = payloadStr || "-"; return; }
  if (topic === `${mqttInfo.fullBase}/info/mfg`)     { ui.mfg.textContent = payloadStr || "-"; return; }
  if (topic === `${mqttInfo.fullBase}/info/year`)    { ui.year.textContent = payloadStr || "-"; return; }
  if (topic === `${mqttInfo.fullBase}/info/mac/eth`) { ui.macEth.textContent = payloadStr || "-"; updateMacSummary(); return; }
  if (topic === `${mqttInfo.fullBase}/info/mac/sta`) { ui.macSta.textContent = payloadStr || "-"; updateMacSummary(); return; }
  if (topic === `${mqttInfo.fullBase}/info/mac/ap`)  { ui.macAp.textContent  = payloadStr || "-"; updateMacSummary(); return; }
  if (topic === `${mqttInfo.fullBase}/info/mac/efuse`) { ui.macEfuse.textContent = payloadStr || "-"; updateMacSummary(); return; }
  if (topic === `${mqttInfo.fullBase}/info/json`) {
    const obj = safeJsonParse(payloadStr);
    if (obj) {
      // Accept either flat keys or nested mac{}
      if (obj.board != null) ui.board.textContent = obj.board;
      if (obj.serial != null) ui.serial.textContent = obj.serial;
      if (obj.fw != null) ui.fw.textContent = obj.fw;
      if (obj.hw != null) ui.hw.textContent = obj.hw;
      if (obj.mfg != null) ui.mfg.textContent = obj.mfg;
      if (obj.year != null) ui.year.textContent = obj.year;
      const mac = obj.mac || {};
      if (mac.eth != null) ui.macEth.textContent = mac.eth;
      if (mac.sta != null) ui.macSta.textContent = mac.sta;
      if (mac.ap  != null) ui.macAp.textContent  = mac.ap;
      if (mac.efuse != null) ui.macEfuse.textContent = mac.efuse;
      updateMacSummary();
    }
    return;
  }

}

// ---- init ----
async function loadMqttInfo() {
  const r = await fetch("/api/mqttinfo", { cache: "no-store" });
  if (!r.ok) throw new Error(`/api/mqttinfo http ${r.status}`);
  return await r.json();
}

function connectMqtt() {
  const proto = (location.protocol === "https:") ? "wss" : "ws";
  const path = (mqttInfo.brokerWsPath && mqttInfo.brokerWsPath.length) ? mqttInfo.brokerWsPath : "/";
  const url = `${proto}://${mqttInfo.brokerWsHost}:${mqttInfo.brokerWsPort}${path.startsWith("/") ? path : `/${path}`}`;

  ui.debug.textContent = `WS: ${url} ; sub=${mqttInfo.fullBase}/#`;
  clearError();

  const opts = {
    clientId: `webdash_${Math.random().toString(16).slice(2)}`,
    clean: true,
    reconnectPeriod: 2000,
    connectTimeout: 7000,

    // NEW: pass auth if provided
    username: (mqttInfo.brokerWsUser && mqttInfo.brokerWsUser.length) ? mqttInfo.brokerWsUser : undefined,
    password: (mqttInfo.brokerWsPass && mqttInfo.brokerWsPass.length) ? mqttInfo.brokerWsPass : undefined,
  };

  mqttClient = mqtt.connect(url, opts);

  mqttClient.on("connect", () => {
    setMqttUi(true, "MQTT: Connected");
    clearError();
    const sub = `${mqttInfo.fullBase}/#`;
    mqttClient.subscribe(sub, { qos: 0 }, (err) => {
      if (err) {
        showError(`Subscribe error: ${err.message}`);
        ui.debug.textContent = `Subscribe error: ${err.message}`;
      } else {
        ui.debug.textContent = `Subscribed: ${sub}`;
        publish(`${mqttInfo.fullBase}/cmd/request/full`, "1");
      }
    });
  });

  mqttClient.on("reconnect", () => setMqttUi(false, "MQTT: Reconnecting…"));
  mqttClient.on("close", () => setMqttUi(false, "MQTT: Disconnected"));

  mqttClient.on("error", (e) => {
    setMqttUi(false, "MQTT: Error");
    showError(`MQTT error: ${e.message}`);
    ui.debug.textContent = `MQTT error: ${e.message}`;
  });

  mqttClient.on("message", (topic, payload) => handleMqttMessage(topic, payload.toString()));
}

async function boot() {
  buildStaticUi();

  ui.apSsid.textContent = "AP: A8RM-SETUP";
  ui.apIp.textContent = `ESP IP: ${location.hostname}`;

  try {
    mqttInfo = await loadMqttInfo();

    ui.clientId.textContent = mqttInfo.clientId || "-";
    ui.fullBase.textContent = mqttInfo.fullBase || "-";

    // FIX: never show “Not configured” when we DO have mqttinfo
    setMqttUi(false, "MQTT: Connecting…");
    connectMqtt();
  } catch (e) {
    ui.debug.textContent = `Failed to start: ${e.message}`;
    showError(`Failed to start: ${e.message}`);
    setMqttUi(false, "MQTT: Not configured");
  }
}

document.addEventListener("DOMContentLoaded", boot);

// ======================================================================
// ===== Settings Tab (Network + RTC) =====
// ======================================================================
function qs(id){ return document.getElementById(id); }

function setTab(name){
  document.querySelectorAll('.tabbtn').forEach(b=>b.classList.toggle('active', b.dataset.tab===name));
  qs('tab-dashboard').classList.toggle('active', name==='dashboard');
  qs('tab-settings').classList.toggle('active', name==='settings');
}

document.querySelectorAll('.tabbtn').forEach(btn=>{
  btn.addEventListener('click', ()=> setTab(btn.dataset.tab));
});
// default tab
setTab('dashboard');

async function apiGet(url){
  const r = await fetch(url, { cache:'no-store' });
  if(!r.ok) throw new Error('HTTP '+r.status);
  return await r.json();
}
async function apiPost(url, formObj){
  const body = new URLSearchParams(formObj);
  const r = await fetch(url, {
    method:'POST',
    headers:{ 'Content-Type':'application/x-www-form-urlencoded' },
    body
  });
  if(!r.ok) throw new Error('HTTP '+r.status);
  return await r.json();
}

function setNetStatus(msg, ok=true){
  const el = qs('net-status');
  if(!el) return;
  el.textContent = msg;
  el.classList.toggle('ok', ok);
}

async function loadNetCfg(){
  try{
    setNetStatus('Loading…', true);
    const j = await apiGet('/api/netcfg/get');

    qs('cfg-ap-ssid').value = j.ap?.ssid ?? '';
    qs('cfg-ap-pass').value = j.ap?.pass ?? '';
    qs('cfg-ap-ip').textContent = j.ap?.ip ?? '—';

    qs('cfg-sta-en').checked = !!j.sta?.enabled;
    qs('cfg-sta-ssid').value = j.sta?.ssid ?? '';
    qs('cfg-sta-pass').value = j.sta?.pass ?? '';
    qs('cfg-sta-dhcp').checked = !!j.sta?.dhcp;
    qs('cfg-sta-ip').value = j.sta?.ip ?? '';
    qs('cfg-sta-mask').value = j.sta?.mask ?? '';
    qs('cfg-sta-gw').value = j.sta?.gw ?? '';
    qs('cfg-sta-dns').value = j.sta?.dns ?? '';
    qs('cfg-sta-conn').textContent = (j.sta?.connected ? 'YES' : 'NO');
    qs('cfg-sta-ip-now').textContent = j.sta?.ip_now ?? '—';

    qs('cfg-eth-dhcp').checked = !!j.eth?.dhcp;
    qs('cfg-eth-mac').value = j.eth?.mac ?? '';
    qs('cfg-eth-ip').value = j.eth?.ip ?? '';
    qs('cfg-eth-mask').value = j.eth?.mask ?? '';
    qs('cfg-eth-gw').value = j.eth?.gw ?? '';
    qs('cfg-eth-dns').value = j.eth?.dns ?? '';
    qs('cfg-eth-ip-now').textContent = j.eth?.ip_now ?? '—';

    qs('cfg-tcp-port').value = j.tcp?.port ?? 5000;

    setNetStatus('Loaded', true);
  }catch(e){
    setNetStatus('Load failed: '+e.message, false);
  }
}

async function saveNetCfg(mode){
  // mode: 'apply' or 'reboot'
  try{
    setNetStatus('Saving…', true);
    const form = {
      ap_ssid: qs('cfg-ap-ssid').value.trim(),
      ap_pass: qs('cfg-ap-pass').value,

      sta_en: qs('cfg-sta-en').checked ? '1' : '0',
      sta_ssid: qs('cfg-sta-ssid').value.trim(),
      sta_pass: qs('cfg-sta-pass').value,
      sta_dhcp: qs('cfg-sta-dhcp').checked ? '1' : '0',
      sta_ip: qs('cfg-sta-ip').value.trim(),
      sta_mask: qs('cfg-sta-mask').value.trim(),
      sta_gw: qs('cfg-sta-gw').value.trim(),
      sta_dns: qs('cfg-sta-dns').value.trim(),

      eth_dhcp: qs('cfg-eth-dhcp').checked ? '1' : '0',
      eth_mac: qs('cfg-eth-mac').value.trim(),
      eth_ip: qs('cfg-eth-ip').value.trim(),
      eth_mask: qs('cfg-eth-mask').value.trim(),
      eth_gw: qs('cfg-eth-gw').value.trim(),
      eth_dns: qs('cfg-eth-dns').value.trim(),

      tcp_port: qs('cfg-tcp-port').value.trim(),
      apply: (mode==='apply') ? '1' : '0',
      reboot: (mode==='reboot') ? '1' : '0',
    };
    const j = await apiPost('/api/netcfg/set', form);
    setNetStatus(j.ok ? (mode==='reboot' ? 'Saved. Rebooting…' : 'Saved + Applied') : 'Save failed', !!j.ok);
    if(mode==='apply') setTimeout(loadNetCfg, 800);
  }catch(e){
    setNetStatus('Save failed: '+e.message, false);
  }
}

function setRtcStatus(msg, ok=true){
  const el = qs('rtc-status');
  if(!el) return;
  el.textContent = msg;
  el.classList.toggle('ok', ok);
}

async function loadRtc(){
  try{
    setRtcStatus('Loading…', true);
    const j = await apiGet('/api/rtc/get');
    qs('rtc-now').textContent = j.rtc ?? '—';
    qs('rtc-unix').textContent = (j.unix ?? '—');
    setRtcStatus('OK', true);
  }catch(e){
    setRtcStatus('Load failed: '+e.message, false);
  }
}

function fmt2(n){ return String(n).padStart(2,'0'); }
function browserTs(){
  return String(Math.floor(Date.now()/1000));
}

async function setRtcToBrowser(){
  try{
    setRtcStatus('Setting…', true);
    const ts = browserTs();
    const j = await apiPost('/api/rtc/set', { ts });
    setRtcStatus(j.ok ? 'Set OK' : 'Set failed', !!j.ok);
    setTimeout(loadRtc, 700);
  }catch(e){
    setRtcStatus('Set failed: '+e.message, false);
  }
}

window.addEventListener('DOMContentLoaded', ()=>{
  qs('btn-net-reload')?.addEventListener('click', loadNetCfg);
  qs('btn-net-apply')?.addEventListener('click', ()=>saveNetCfg('apply'));
  qs('btn-net-reboot')?.addEventListener('click', ()=>saveNetCfg('reboot'));

  qs('btn-rtc-refresh')?.addEventListener('click', loadRtc);
  qs('btn-rtc-set-browser')?.addEventListener('click', setRtcToBrowser);

  // Load on boot
  loadNetCfg();
  loadRtc();
});
