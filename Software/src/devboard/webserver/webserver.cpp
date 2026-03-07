#ifdef HW_WAVESHARE7B_DISPLAY_ONLY
#include "webserver.h"
#include <WiFi.h>
#include "../../datalayer/datalayer.h"
#include "../utils/events.h"
#include "../utils/timer.h"
#include "../display/mqtt_display_bridge.h"

bool ota_active = false;
static AsyncWebServer server(80);
static MyTimer ota_timeout_timer(600000);

void onOTAStart() {
  ota_active = true;
  ota_timeout_timer.reset();
  Serial.printf("[%lus] OTA update started\n", millis()/1000);
}

void onOTAProgress(size_t current, size_t final_size) {
  static int last_pct = -1;
  int pct = (current * 100) / final_size;
  if (pct != last_pct && pct % 10 == 0) {
    Serial.printf("[%lus] OTA: %d%%\n", millis()/1000, pct);
    last_pct = pct;
  }
}

void onOTAEnd(bool success) {
  ota_active = false;
  Serial.printf("[%lus] OTA %s\n", millis()/1000, success ? "SUCCESS - rebooting" : "FAILED");
}

String processor(const String& var) { return String(); }
String get_firmware_info_processor(const String& var) { return String(); }
void init_ElegantOTA() {}

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><title>Tesla Battery Monitor</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;padding:12px}
.hdr{display:flex;align-items:center;gap:12px;margin-bottom:12px}
.logo{color:#e82127;font-weight:900;font-size:28px;letter-spacing:6px}
.sub{color:#8b949e;font-size:16px}
.tabs{display:flex;gap:6px;margin-bottom:12px}
.tab{background:#161b22;border:1px solid #30363d;color:#8b949e;padding:8px 18px;border-radius:8px 8px 0 0;cursor:pointer;font-size:14px}
.tab.active{background:#0d1117;border-bottom-color:#0d1117;color:#58a6ff}
.page{display:none}.page.active{display:block}
.soc-wrap{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:14px 18px;margin-bottom:12px}
.soc-top{display:flex;align-items:center;gap:16px;margin-bottom:8px}
.soc-pct{font-size:32px;font-weight:700;min-width:90px}
.soc-lbl{color:#8b949e;font-size:12px;text-transform:uppercase;letter-spacing:1px}
.bar-bg{background:#1a1a2e;border-radius:6px;height:28px;flex:1;overflow:hidden}
.bar-fg{height:100%;border-radius:6px;transition:width .5s}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:12px}
.card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:12px}
.card-title{color:#8b949e;font-size:11px;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
.card-val{font-size:22px;font-weight:600}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}
.sys-split{display:grid;grid-template-columns:1fr 1px 1fr;gap:12px}
.sys-div{background:#30363d}
.sys-line{font-size:13px;margin:3px 0}
.sys-lbl{color:#8b949e}
.grn{color:#7ee787}.red{color:#ff7b72}.yel{color:#ffa657}.blu{color:#58a6ff}.pur{color:#a371f7}.cyn{color:#79c0ff}
.cell-grid{display:grid;grid-template-columns:repeat(9,1fr);gap:2px;font-size:11px}
.cell{background:#161b22;border:1px solid #21262d;border-radius:4px;padding:4px;text-align:center}
.cell.outlier{border-color:#ff4444;color:#ff7b72}
.cell .cn{color:#8b949e;font-size:9px}.cell .cv{font-size:12px;font-weight:700}
.bar-chart{display:flex;align-items:flex-end;gap:1px;height:100px;margin-top:8px;background:#0d1117;padding:4px;border-radius:4px}
.bar-chart .bar{flex:1;min-width:2px;background:#3b82f6;border-radius:1px 1px 0 0}
.bar-chart .bar.out{background:#ff4444}
.alert-item{padding:4px 0;border-bottom:1px solid #21262d}
.solar-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.solar-split{display:flex;gap:16px}
.solar-half{flex:1}
.solar-section-title{color:#58a6ff;font-size:16px;font-weight:700;margin-bottom:6px}
.solar-cards{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.solar-card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:14px}
.solar-title{color:#58a6ff;font-size:14px;font-weight:600;margin-bottom:10px;border-bottom:1px solid #30363d;padding-bottom:6px}
.solar-row{display:flex;justify-content:space-between;padding:3px 0;font-size:13px}
.solar-lbl{color:#8b949e}
.ota-btn{display:inline-block;background:#238636;color:#fff;padding:10px 20px;border-radius:6px;text-decoration:none;font-size:14px;margin-top:8px}
@media(max-width:700px){.grid{grid-template-columns:repeat(2,1fr)}.cell-grid{grid-template-columns:repeat(4,1fr)}.solar-split{flex-direction:column}.solar-cards{grid-template-columns:repeat(2,1fr)}.bar-chart{height:60px}}
</style></head><body>
<div class="hdr"><span class="logo">TESLA</span><span class="sub">Battery Monitor</span></div>
<div class="tabs">
<div class="tab active" onclick="showTab(3)">Solar</div>
<div class="tab" onclick="showTab(0)">Tesla</div>
<div class="tab" onclick="showTab(1)">Cells</div>
<div class="tab" onclick="showTab(2)">Alerts</div>
</div>

<div id="p0" class="page">
<div class="soc-wrap">
<div class="soc-lbl">STATE OF CHARGE</div>
<div class="soc-top"><span id="soc" class="soc-pct grn">--%</span>
<div class="bar-bg"><div id="socbar" class="bar-fg" style="width:0%;background:linear-gradient(90deg,#22cc44,#00ff88)"></div></div>
</div></div>
<div class="grid">
<div class="card"><div class="card-title">Voltage</div><div id="v" class="card-val blu">---.- V</div></div>
<div class="card"><div class="card-title">Current</div><div id="a" class="card-val" style="color:#f0883e">---.- A</div></div>
<div class="card"><div class="card-title">Power</div><div id="w" class="card-val pur">---- W</div></div>
<div class="card"><div class="card-title">Capacity</div><div id="kwh" class="card-val grn">-- kWh</div></div>
</div>
<div class="grid">
<div class="card"><div class="card-title">Cell Min</div><div id="cmin" class="card-val red">-.--- V</div></div>
<div class="card"><div class="card-title">Cell Max</div><div id="cmax" class="card-val grn">-.--- V</div></div>
<div class="card"><div class="card-title">Cell Delta</div><div id="cdelta" class="card-val yel">--- mV</div></div>
<div class="card"><div class="card-title">Temperature</div><div id="temp" class="card-val cyn">--.- C</div></div>
</div>
<div class="grid">
<div class="card"><div class="card-title">Temp Min</div><div id="tmin" class="card-val cyn">--.- C</div></div>
<div class="card"><div class="card-title">Temp Max</div><div id="tmax" class="card-val red">--.- C</div></div>
<div class="card"><div class="card-title">Contactor</div><div id="cont" class="card-val red">OPEN</div></div>
<div class="card"><div class="card-title">Network</div><div id="net" class="card-val" style="font-size:14px">--</div></div>
</div>
<div class="grid2">
<div class="card"><div class="card-title">Battery Info</div>
<div id="binfo" style="font-size:14px">Total: -- kWh<br>Remaining: -- kWh<br>SOH: -- %</div></div>
<div class="card"><div class="card-title">System / CAN Status</div>
<div class="sys-split"><div id="sys" style="font-size:13px">BMS: --<br>MQTT: --<br>Uptime: --<br>Heap: --</div>
<div class="sys-div"></div><div id="can" style="font-size:13px">CAN Bus<br>BATT: --<br>INV: --</div></div></div>
</div>
<a class="ota-btn" href="/update">Firmware Update (OTA)</a>
</div>

<div id="p1" class="page">
<div style="margin-bottom:10px"><span id="ctitle" class="card-title" style="color:#00d4ff;font-size:16px">CELL VOLTAGES (0 cells)</span></div>
<div style="display:flex;gap:20px;margin-bottom:10px;font-size:13px;color:#8b949e">
<span id="mmv">Pack V: Min -- / Max --</span>
<span id="mmt">Temp: Min -- / Max --</span>
<span id="mmc">Cell V: Min -- / Max --</span>
</div>
<div id="cells" class="cell-grid"></div>
<div id="cell_bars" class="bar-chart"></div>
</div>

<div id="p2" class="page">
<div id="alerts" style="margin-bottom:16px"><div class="card-title" style="color:#00d4ff;font-size:16px;margin-bottom:10px">ACTIVE ALERTS</div><div class="grn">No active alerts</div></div>
<div><div class="card-title" style="color:#00d4ff;font-size:16px;margin-bottom:10px">EVENT LOG</div><div id="events" style="font-size:13px;color:#8b949e">(no events)</div></div>
</div>

<div id="p3" class="page active">
<div style="color:#f0a500;font-size:18px;font-weight:700;margin-bottom:20px;text-align:center">STREETSRIDGE MICRO GRID</div>
<div class="soc-wrap" style="padding:10px 14px;margin-bottom:12px">
<div style="display:flex;align-items:center;gap:10px">
<span id="ms_soc_pct" style="font-size:24px;font-weight:700;min-width:55px" class="grn">-- %</span>
<div class="bar-bg" style="flex:1;height:36px"><div id="ms_soc_bar" class="bar-fg" style="width:0%;height:100%;background:linear-gradient(90deg,#22cc44,#00ff88)"></div></div>
</div></div>
<div class="solar-split">
<div class="solar-half">
<div class="solar-section-title"><img src="data:image/png;base64,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" style="height:26px"> <span style="color:#8b949e;font-size:12px;margin-left:6px">12K-2P</span></div>
<div class="soc-wrap" style="padding:10px 14px;margin-bottom:8px">
<div style="display:flex;align-items:center;gap:10px">
<span id="sk_soc_pct" style="font-size:24px;font-weight:700;min-width:55px" class="grn">-- %</span>
<div class="bar-bg" style="flex:1;height:44px"><div id="sk_soc_bar" class="bar-fg" style="width:0%;height:100%;background:linear-gradient(90deg,#22cc44,#00ff88)"></div></div>
</div></div>
<div class="solar-cards"><div class="solar-card"><div class="solar-title">PV Power</div><div id="sk_pv">--</div></div>
<div class="solar-card"><div class="solar-title">Load</div><div id="sk_load">--</div></div>
<div class="solar-card"><div class="solar-title">Grid</div><div id="sk_grid">--</div></div>
<div class="solar-card"><div class="solar-title">Battery</div><div id="sk_batt">--</div></div>
<div class="solar-card"><div class="solar-title">SOC</div><div id="sk_soc">--</div></div>
<div class="solar-card"><div class="solar-title">Today</div><div id="sk_day">--</div></div></div>
</div>
<div class="solar-half">
<div class="solar-section-title"><img src="data:image/png;base64,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" style="height:26px"> <span style="color:#8b949e;font-size:12px;margin-left:6px">S6-EH1P(11.4)K-H-US</span></div>
<div class="soc-wrap" style="padding:10px 14px;margin-bottom:8px">
<div style="display:flex;align-items:center;gap:10px">
<span id="sl_soc_pct" style="font-size:24px;font-weight:700;min-width:55px" class="grn">-- %</span>
<div class="bar-bg" style="flex:1;height:44px"><div id="sl_soc_bar" class="bar-fg" style="width:0%;height:100%;background:linear-gradient(90deg,#22cc44,#00ff88)"></div></div>
</div></div>
<div class="solar-cards"><div class="solar-card"><div class="solar-title">PV Power</div><div id="sl_pv">--</div></div>
<div class="solar-card"><div class="solar-title">Load</div><div id="sl_load">--</div></div>
<div class="solar-card"><div class="solar-title">Grid</div><div id="sl_grid">--</div></div>
<div class="solar-card"><div class="solar-title">Battery</div><div id="sl_batt">--</div></div>
<div class="solar-card"><div class="solar-title">SOC</div><div id="sl_soc">--</div></div>
<div class="solar-card"><div class="solar-title">Today</div><div id="sl_day">--</div></div></div>
</div>
</div>
<div style="display:flex;gap:10px;margin-top:12px;flex-wrap:wrap">
<div class="solar-card" style="flex:1;min-width:120px"><div class="solar-title">Total Live</div><div id="env_total_live">--</div></div>
<div class="solar-card" style="flex:1;min-width:120px"><div class="solar-title">Total Today</div><div id="env_total_today">--</div></div>
<div class="solar-card" style="flex:1;min-width:120px"><div class="solar-title">House Today</div><div id="env_house">--</div></div>
<div class="solar-card" style="flex:1;min-width:120px"><div class="solar-title">Shed Today</div><div id="env_shed">--</div></div>
<div class="solar-card" style="flex:1;min-width:120px"><div class="solar-title">Trailer Today</div><div id="env_trailer">--</div></div>
</div>
</div>

<script>
var tabs=document.querySelectorAll('.tab');
function showTab(n){tabs.forEach(function(t,i){t.classList.toggle('active',i==n)});
for(var i=0;i<4;i++)document.getElementById('p'+i).classList.toggle('active',i==n)}

function fmt(w){return Math.abs(w)>=1000?(w/1000).toFixed(2)+' kW':w.toFixed(0)+' W'}
function srow(l,v,c){return '<div class="solar-row"><span class="solar-lbl">'+l+'</span><span style="color:'+(c||'#c9d1d9')+'">'+v+'</span></div>'}

function upd(){fetch('/api/data').then(function(r){return r.json()}).then(function(d){
var s=d.soc;document.getElementById('soc').textContent=s+'%';
document.getElementById('soc').className='soc-pct '+(s<20?'red':s<50?'yel':'grn');
var bg=s<20?'linear-gradient(90deg,#ff4444,#ff8844)':s<50?'linear-gradient(90deg,#ff6622,#ffcc00)':'linear-gradient(90deg,#22cc44,#00ff88)';
var b=document.getElementById('socbar');b.style.width=s+'%';b.style.background=bg;
document.getElementById('v').textContent=d.voltage.toFixed(1)+' V';
document.getElementById('a').textContent=d.current.toFixed(1)+' A';
var p=d.voltage*d.current;document.getElementById('w').textContent=Math.abs(p)>=1000?(p/1000).toFixed(1)+' kW':p.toFixed(0)+' W';
document.getElementById('kwh').textContent=(d.remaining_kwh).toFixed(1)+' kWh';
document.getElementById('cmin').textContent=(d.cell_min/1000).toFixed(3)+' V';
document.getElementById('cmax').textContent=(d.cell_max/1000).toFixed(3)+' V';
var dl=d.cell_max-d.cell_min;document.getElementById('cdelta').textContent=dl+' mV';
document.getElementById('cdelta').className='card-val '+(dl<50?'grn':dl<100?'yel':'red');
document.getElementById('temp').textContent=((d.temp_min+d.temp_max)/20).toFixed(1)+' C';
document.getElementById('tmin').textContent=(d.temp_min/10).toFixed(1)+' C';
document.getElementById('tmax').textContent=(d.temp_max/10).toFixed(1)+' C';
document.getElementById('cont').textContent=d.contactors?'CLOSED':'OPEN';
document.getElementById('cont').className='card-val '+(d.contactors?'grn':'red');
document.getElementById('net').innerHTML=d.wifi;document.getElementById('net').style.color=d.wifi_ok?'#7ee787':'#8b949e';
document.getElementById('binfo').innerHTML='Total: '+(d.total_kwh).toFixed(1)+' kWh<br>Remaining: '+(d.remaining_kwh).toFixed(1)+' kWh<br>SOH: '+d.soh+' %';
var sc=d.bms==1?'#7ee787':d.bms==2?'#ff7b72':'#ffa657';var st=d.bms==1?'ACTIVE':d.bms==2?'FAULT':'STANDBY';
document.getElementById('sys').innerHTML='<span style="color:'+sc+'">BMS: '+st+'</span><br>'+'<span style="color:'+(d.mqtt_ok?'#7ee787':'#ff7b72')+'">MQTT: '+(d.mqtt_ok?'OK':'OFF')+'</span><br>Uptime: '+d.uptime+'<br>Heap: '+d.heap+' KB';
document.getElementById('can').innerHTML='<span class="sys-lbl">CAN Bus</span><br>'+'<span style="color:'+(d.can_batt?'#7ee787':'#8b949e')+'">BATT: '+(d.can_batt?'OK':'--')+'</span><br>'+'<span style="color:'+(d.can_inv?'#7ee787':'#8b949e')+'">INV: '+(d.can_inv?'OK':'--')+'</span>';
if(d.cells){var cg=document.getElementById('cells');var bg=document.getElementById('cell_bars');
var nc=d.cells.length;if(cg.children.length!=nc){cg.innerHTML='';bg.innerHTML='';
for(var i=0;i<nc;i++){var ce=document.createElement('div');ce.className='cell';ce.id='c'+i;ce.innerHTML='<div class="cn">Cell '+(i+1)+'</div><div class="cv">--</div>';cg.appendChild(ce);
var br=document.createElement('div');br.className='bar';br.id='b'+i;bg.appendChild(br)}}
var vmin=99999,vmax=0;for(var i=0;i<nc;i++){var mv=d.cells[i];if(mv>0&&mv<vmin)vmin=mv;if(mv>vmax)vmax=mv}
var vfloor=vmin>100?vmin-100:0;var vspan=vmax-vfloor;if(vspan<1)vspan=1;var vrange=vmax-vmin;
for(var i=0;i<nc;i++){var mv=d.cells[i];var el=document.getElementById('c'+i);var br=document.getElementById('b'+i);
var out=vrange>50&&(mv==vmin||mv==vmax);el.querySelector('.cv').textContent=mv>0?mv+' mV':'--';
el.className=out?'cell outlier':'cell';el.querySelector('.cv').style.color=out?'#ff7b72':'#c9d1d9';
var pct=mv>0?((mv-vfloor)/vspan*100):0;if(pct<2)pct=2;br.style.height=pct+'%';br.className=out?'bar out':'bar'}
document.getElementById('ctitle').textContent='CELL VOLTAGES ('+d.num_cells+' cells)'}
document.getElementById('mmv').textContent='Pack: '+d.v_min_ever.toFixed(1)+' / '+d.v_max_ever.toFixed(1)+' V';
document.getElementById('mmt').textContent='Temp: '+d.t_min_ever.toFixed(0)+' / '+d.t_max_ever.toFixed(0)+' C';
document.getElementById('mmc').textContent='Cell: '+d.c_min_ever.toFixed(3)+' / '+d.c_max_ever.toFixed(3)+' V';
if(d.alert_count>0){var ah='<div class="card-title" style="color:#ff7b72;font-size:16px;margin-bottom:10px">ACTIVE ALERTS ('+d.alert_count+')</div>';
d.alerts.forEach(function(a){ah+='<div class="alert-item red">! '+a+'</div>'});document.getElementById('alerts').innerHTML=ah}
else{document.getElementById('alerts').innerHTML='<div class="card-title" style="color:#00d4ff;font-size:16px;margin-bottom:10px">ACTIVE ALERTS</div><div class="grn">No active alerts</div>'}
if(d.events)document.getElementById('events').innerHTML=d.events.join('<br>')||'(no events)';
var sk=d.solar;if(sk){
function usb(id,soc){var b=document.getElementById(id+'_soc_bar'),p=document.getElementById(id+'_soc_pct');if(!b||!p)return;
var v=Math.max(0,Math.min(100,Math.round(soc)));b.style.width=v+'%';p.textContent=v+' %';
var bg=v<20?'linear-gradient(90deg,#ff4444,#ff8844)':v<50?'linear-gradient(90deg,#ff6622,#ffcc00)':'linear-gradient(90deg,#22cc44,#00ff88)';
b.style.background=bg;p.className=v<20?'red':v<50?'yel':'grn'}
function sval(id,v){var e=document.getElementById(id);if(e)e.innerHTML=v}
if(sk.solark_ts>0){var sg=sk.solark_grid>=0?'+'+sk.solark_grid.toFixed(0):sk.solark_grid.toFixed(0);var sb=-sk.solark_batt;
usb('sk',sk.solark_soc);
sval('sk_pv','<span style="color:#ffa657;font-size:18px;font-weight:600">'+fmt(sk.solark_pv)+'</span>');
sval('sk_load','<span style="font-size:18px;font-weight:600">'+fmt(sk.solark_load)+'</span>');
sval('sk_grid','<span style="color:'+(sk.solark_grid<0?'#7ee787':'#ff7b72')+';font-size:18px;font-weight:600">'+sg+' W</span>');
sval('sk_batt','<span style="color:'+(sb>=0?'#58a6ff':'#ffa657')+';font-size:18px;font-weight:600">'+(sb>=0?'+':'')+sb.toFixed(0)+' W</span>');
sval('sk_soc','<span style="font-size:18px;font-weight:600">'+sk.solark_soc.toFixed(1)+' %</span>');
sval('sk_day','<span style="color:#ffa657;font-size:18px;font-weight:600">'+sk.solark_day.toFixed(2)+' kWh</span>')}
if(sk.solis_ts>0){var sg2=sk.solis_grid>=0?'+'+sk.solis_grid.toFixed(0):sk.solis_grid.toFixed(0);var sb2=-sk.solis_batt;
usb('sl',sk.solis_soc);
sval('sl_pv','<span style="color:#ffa657;font-size:18px;font-weight:600">'+fmt(sk.solis_pv)+'</span>');
sval('sl_load','<span style="font-size:18px;font-weight:600">'+fmt(sk.solis_load)+'</span>');
sval('sl_grid','<span style="color:'+(sk.solis_grid<0?'#7ee787':'#ff7b72')+';font-size:18px;font-weight:600">'+sg2+' W</span>');
sval('sl_batt','<span style="color:'+(sb2>=0?'#58a6ff':'#ffa657')+';font-size:18px;font-weight:600">'+(sb2>=0?'+':'')+sb2.toFixed(0)+' W</span>');
sval('sl_soc','<span style="font-size:18px;font-weight:600">'+sk.solis_soc.toFixed(1)+' %</span>');
sval('sl_day','<span style="color:#ffa657;font-size:18px;font-weight:600">'+sk.solis_day.toFixed(2)+' kWh</span>')}
usb('ms',((sk.solark_ts>0?sk.solark_soc:0)+(sk.solis_ts>0?sk.solis_soc:0))/((sk.solark_ts>0?1:0)+(sk.solis_ts>0?1:0)||1));
if(sk.env_ts>0){sval('env_total_live','<span style="color:#7ee787;font-size:18px;font-weight:600">'+fmt(sk.env_total_live)+'</span>');
sval('env_total_today','<span style="color:#ffa657;font-size:18px;font-weight:600">'+sk.env_total_today.toFixed(1)+' kWh</span>');
sval('env_house','<span style="color:#58a6ff;font-size:18px;font-weight:600">'+sk.env_house.toFixed(1)+' kWh</span>');
sval('env_shed','<span style="color:#58a6ff;font-size:18px;font-weight:600">'+sk.env_shed.toFixed(1)+' kWh</span>');
sval('env_trailer','<span style="color:#58a6ff;font-size:18px;font-weight:600">'+sk.env_trailer.toFixed(1)+' kWh</span>')}
}}).catch(function(){})}
setInterval(upd,2000);upd();
</script></body></html>)rawliteral";

static const uint8_t WEB_MAX_EVENTS = 20;
static char web_event_log[WEB_MAX_EVENTS][80];
static uint8_t web_event_count = 0;

void web_add_event(const char* msg) {
  for (int i = WEB_MAX_EVENTS - 1; i > 0; i--)
    strncpy(web_event_log[i], web_event_log[i-1], 79);
  unsigned long secs = millis() / 1000;
  snprintf(web_event_log[0], 79, "[%lus] %s", secs, msg);
  if (web_event_count < WEB_MAX_EVENTS) web_event_count++;
}

void init_webserver() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    unsigned long up = millis() / 1000;
    uint8_t soc = datalayer.battery.status.reported_soc / 100;
    float voltage = datalayer.battery.status.voltage_dV / 10.0f;
    float current = datalayer.battery.status.current_dA / 10.0f;
    float remaining_kwh = datalayer.battery.status.remaining_capacity_Wh / 1000.0f;
    float total_kwh = datalayer.battery.info.total_capacity_Wh / 1000.0f;
    uint16_t cell_min = datalayer.battery.status.cell_min_voltage_mV;
    uint16_t cell_max = datalayer.battery.status.cell_max_voltage_mV;
    int16_t temp_min = datalayer.battery.status.temperature_min_dC;
    int16_t temp_max = datalayer.battery.status.temperature_max_dC;
    bool contactors = datalayer.system.status.contactors_engaged;
    bool can_batt = datalayer.battery.status.CAN_battery_still_alive > 0;
    bool can_inv = datalayer.system.status.CAN_inverter_still_alive > 0;
    bool mqtt_ok = (get_event_pointer(EVENT_MQTT_CONNECT)->state == EVENT_STATE_ACTIVE);
    uint8_t bms = (datalayer.battery.status.bms_status == ACTIVE) ? 1 :
                  (datalayer.battery.status.bms_status == FAULT) ? 2 : 0;
    int soh = datalayer.battery.status.soh_pptt / 100;
    uint16_t num_cells = datalayer.battery.info.number_of_cells;

    static float v_min_ever = 9999.0f, v_max_ever = 0.0f;
    static float t_min_ever = 999.0f, t_max_ever = -999.0f;
    static float c_min_ever = 9.999f, c_max_ever = 0.0f;
    if (voltage > 0 && voltage < v_min_ever) v_min_ever = voltage;
    if (voltage > v_max_ever) v_max_ever = voltage;
    float t_min_f = temp_min / 10.0f, t_max_f = temp_max / 10.0f;
    if (t_min_f > -40 && t_min_f < t_min_ever) t_min_ever = t_min_f;
    if (t_max_f > t_max_ever) t_max_ever = t_max_f;
    float c_min_f = cell_min / 1000.0f, c_max_f = cell_max / 1000.0f;
    if (c_min_f > 0 && c_min_f < c_min_ever) c_min_ever = c_min_f;
    if (c_max_f > c_max_ever) c_max_ever = c_max_f;

    String wifi_info;
    bool wifi_ok = WiFi.status() == WL_CONNECTED;
    if (wifi_ok) {
      wifi_info = String(WiFi.SSID()) + "<br>" + WiFi.localIP().toString() + "<br>RSSI: " + String(WiFi.RSSI()) + " dBm";
    } else {
      wifi_info = "Disconnected";
    }

    char uptime_buf[16];
    snprintf(uptime_buf, sizeof(uptime_buf), "%luh %lum", up / 3600, (up % 3600) / 60);

    String json = "{\"soc\":" + String(soc) +
      ",\"voltage\":" + String(voltage, 1) +
      ",\"current\":" + String(current, 1) +
      ",\"remaining_kwh\":" + String(remaining_kwh, 1) +
      ",\"total_kwh\":" + String(total_kwh, 1) +
      ",\"cell_min\":" + String(cell_min) +
      ",\"cell_max\":" + String(cell_max) +
      ",\"temp_min\":" + String(temp_min) +
      ",\"temp_max\":" + String(temp_max) +
      ",\"contactors\":" + String(contactors ? "true" : "false") +
      ",\"can_batt\":" + String(can_batt ? "true" : "false") +
      ",\"can_inv\":" + String(can_inv ? "true" : "false") +
      ",\"mqtt_ok\":" + String(mqtt_ok ? "true" : "false") +
      ",\"bms\":" + String(bms) +
      ",\"soh\":" + String(soh) +
      ",\"heap\":" + String(ESP.getFreeHeap() / 1024) +
      ",\"uptime\":\"" + String(uptime_buf) + "\"" +
      ",\"wifi\":\"" + wifi_info + "\"" +
      ",\"wifi_ok\":" + String(wifi_ok ? "true" : "false") +
      ",\"num_cells\":" + String(num_cells) +
      ",\"v_min_ever\":" + String(v_min_ever, 1) +
      ",\"v_max_ever\":" + String(v_max_ever, 1) +
      ",\"t_min_ever\":" + String(t_min_ever, 1) +
      ",\"t_max_ever\":" + String(t_max_ever, 1) +
      ",\"c_min_ever\":" + String(c_min_ever, 3) +
      ",\"c_max_ever\":" + String(c_max_ever, 3);

    json += ",\"cells\":[";
    for (uint16_t i = 0; i < num_cells && i < 108; i++) {
      if (i > 0) json += ",";
      json += String(datalayer.battery.status.cell_voltages_mV[i]);
    }
    json += "]";

    uint8_t alert_count = 0;
    String alerts = ",\"alerts\":[";
    uint16_t cell_delta = (cell_min > 0) ? (cell_max - cell_min) : 0;
    if (soc < 10 && soc > 0) { if (alert_count) alerts += ","; alerts += "\"LOW SOC (<10%)\""; alert_count++; }
    if (t_max_f > 45.0f) { if (alert_count) alerts += ","; alerts += "\"HIGH TEMP (>45C)\""; alert_count++; }
    if (t_min_f < 0.0f && t_min_f > -40.0f) { if (alert_count) alerts += ","; alerts += "\"LOW TEMP (<0C)\""; alert_count++; }
    if (cell_delta > 100 && cell_min > 0) { if (alert_count) alerts += ","; alerts += "\"CELL IMBALANCE (>100mV)\""; alert_count++; }
    if (c_min_f < 2.8f && c_min_f > 0.1f) { if (alert_count) alerts += ","; alerts += "\"CELL UNDERVOLTAGE\""; alert_count++; }
    if (c_max_f > 4.25f) { if (alert_count) alerts += ","; alerts += "\"CELL OVERVOLTAGE\""; alert_count++; }
    if (!can_batt) { if (alert_count) alerts += ","; alerts += "\"NO BATTERY CAN\""; alert_count++; }
    alerts += "],\"alert_count\":" + String(alert_count);
    json += alerts;

    json += ",\"events\":[";
    for (int i = 0; i < web_event_count && i < WEB_MAX_EVENTS; i++) {
      if (i > 0) json += ",";
      String ev = String(web_event_log[i]);
      ev.replace("\"", "\\\"");
      json += "\"" + ev + "\"";
    }
    json += "]";

    const SolarData& sol = mqtt_display_bridge::get_solar_data();
    json += ",\"solar\":{";
    json += "\"solark_pv\":" + String(sol.solark_pv_power_W, 0);
    json += ",\"solark_load\":" + String(sol.solark_load_power_W, 0);
    json += ",\"solark_grid\":" + String(sol.solark_grid_power_W, 0);
    json += ",\"solark_batt\":" + String(sol.solark_battery_power_W, 0);
    json += ",\"solark_soc\":" + String(sol.solark_battery_soc_pct, 1);
    json += ",\"solark_day\":" + String(sol.solark_day_pv_energy_kWh, 2);
    json += ",\"solark_ts\":" + String(sol.solark_last_update_ms);
    json += ",\"solis_pv\":" + String(sol.solis_pv_power_W, 0);
    json += ",\"solis_load\":" + String(sol.solis_load_power_W, 0);
    json += ",\"solis_grid\":" + String(sol.solis_grid_power_W, 0);
    json += ",\"solis_batt\":" + String(sol.solis_battery_power_W, 0);
    json += ",\"solis_soc\":" + String(sol.solis_battery_soc_pct, 1);
    json += ",\"solis_day\":" + String(sol.solis_day_pv_energy_kWh, 2);
    json += ",\"solis_ts\":" + String(sol.solis_last_update_ms);
    json += ",\"env_total_live\":" + String(sol.envoy_total_live_W, 0);
    json += ",\"env_total_today\":" + String(sol.envoy_total_today_kWh, 1);
    json += ",\"env_house\":" + String(sol.envoy_house_today_kWh, 1);
    json += ",\"env_shed\":" + String(sol.envoy_shed_today_kWh, 1);
    json += ",\"env_trailer\":" + String(sol.envoy_trailer_today_kWh, 1);
    json += ",\"env_ts\":" + String(sol.envoy_last_update_ms);
    json += "}}";

    request->send(200, "application/json", json);
  });

  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.printf("[%lus] Webserver started on port 80\n", millis()/1000);
  web_add_event("Webserver started");
}

void ota_monitor() {
  if (ota_active && ota_timeout_timer.elapsed()) {
    ota_active = false;
    Serial.printf("[%lus] OTA timeout\n", millis()/1000);
    web_add_event("OTA timeout");
  }

  static bool prev_wifi = false;
  static bool prev_mqtt = false;
  static uint8_t prev_bms = 255;
  static unsigned long last_check = 0;
  if (millis() - last_check < 2000) return;
  last_check = millis();

  bool wifi_ok = WiFi.status() == WL_CONNECTED;
  if (wifi_ok != prev_wifi) {
    prev_wifi = wifi_ok;
    web_add_event(wifi_ok ? "WiFi connected" : "WiFi disconnected");
  }
  bool mqtt_ok = (get_event_pointer(EVENT_MQTT_CONNECT)->state == EVENT_STATE_ACTIVE);
  if (mqtt_ok != prev_mqtt) {
    prev_mqtt = mqtt_ok;
    web_add_event(mqtt_ok ? "MQTT connected" : "MQTT disconnected");
  }
  uint8_t bms = datalayer.battery.status.bms_status;
  if (bms != prev_bms) {
    prev_bms = bms;
    const char* names[] = {"STANDBY","INACTIVE","DARKSTART","ACTIVE","FAULT","UPDATING"};
    char buf[40];
    snprintf(buf, sizeof(buf), "BMS: %s", bms < 6 ? names[bms] : "UNKNOWN");
    web_add_event(buf);
  }
}
#else
#include "webserver.h"
#include <Preferences.h>
#include <ctime>
#include <vector>
#include "../../battery/BATTERIES.h"
#include "../../battery/Battery.h"
#include "../../battery/Shunt.h"
#include "../../charger/CHARGERS.h"
#include "../../communication/can/comm_can.h"
#include "../../communication/contactorcontrol/comm_contactorcontrol.h"
#include "../../communication/equipmentstopbutton/comm_equipmentstopbutton.h"
#include "../../communication/nvm/comm_nvm.h"
#include "../../datalayer/datalayer.h"
#include "../../datalayer/datalayer_extended.h"
#include "../../devboard/safety/safety.h"
#include "../../inverter/INVERTERS.h"
#include "../../lib/bblanchon-ArduinoJson/ArduinoJson.h"
#include "../sdcard/sdcard.h"
#include "../utils/events.h"
#include "../utils/led_handler.h"
#include "../utils/timer.h"
#include "esp_task_wdt.h"
#include "html_escape.h"

#include <string>
extern std::string http_username;
extern std::string http_password;

bool webserver_auth = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Measure OTA progress
unsigned long ota_progress_millis = 0;

#include "advanced_battery_html.h"
#include "can_logging_html.h"
#include "can_replay_html.h"
#include "cellmonitor_html.h"
#include "debug_logging_html.h"
#include "events_html.h"
#include "index_html.h"
#include "settings_html.h"

MyTimer ota_timeout_timer = MyTimer(15000);
bool ota_active = false;

const char get_firmware_info_html[] = R"rawliteral(%X%)rawliteral";

String importedLogs = "";      // Store the uploaded logfile contents in RAM
bool isReplayRunning = false;  // Global flag to track replay state

// True when user has updated settings that need a reboot to be effective.
bool settingsUpdated = false;

CAN_frame currentFrame = {.FD = true, .ext_ID = false, .DLC = 64, .ID = 0x12F, .data = {0}};

void handleFileUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len,
                      bool final) {
  if (!index) {
    importedLogs = "";  // Clear previous logs
    logging.printf("Receiving file: %s\n", filename.c_str());
  }

  // Append received data to the string (RAM storage)
  importedLogs += String((char*)data).substring(0, len);

  if (final) {
    logging.println("Upload Complete!");
    request->send(200, "text/plain", "File uploaded successfully");
  }
}

void canReplayTask(void* param) {
  std::vector<String> messages;
  messages.reserve(1000);  // Pre-allocate memory to reduce fragmentation

  if (!importedLogs.isEmpty()) {
    int lastIndex = 0;

    while (true) {
      int nextIndex = importedLogs.indexOf("\n", lastIndex);
      if (nextIndex == -1) {
        messages.push_back(importedLogs.substring(lastIndex));
        break;
      }
      messages.push_back(importedLogs.substring(lastIndex, nextIndex));
      lastIndex = nextIndex + 1;
    }

    do {
      float firstTimestamp = -1.0f;
      float lastTimestamp = 0.0f;
      bool firstMessageSent = false;  // Track first message

      for (size_t i = 0; i < messages.size(); i++) {
        String line = messages[i];
        line.trim();
        if (line.length() == 0)
          continue;

        int timeStart = line.indexOf("(") + 1;
        int timeEnd = line.indexOf(")");
        if (timeStart == 0 || timeEnd == -1)
          continue;

        float currentTimestamp = line.substring(timeStart, timeEnd).toFloat();

        if (firstTimestamp < 0) {
          firstTimestamp = currentTimestamp;
        }

        // Send first message immediately
        if (!firstMessageSent) {
          firstMessageSent = true;
          firstTimestamp = currentTimestamp;  // Adjust reference time
        } else {
          // Delay only if this isn't the first message
          float deltaT = (currentTimestamp - lastTimestamp) * 1000;
          vTaskDelay((int)deltaT / portTICK_PERIOD_MS);
        }

        lastTimestamp = currentTimestamp;

        int interfaceStart = timeEnd + 2;
        int interfaceEnd = line.indexOf(" ", interfaceStart);
        if (interfaceEnd == -1)
          continue;

        int idStart = interfaceEnd + 1;
        int idEnd = line.indexOf(" [", idStart);
        if (idStart == -1 || idEnd == -1)
          continue;

        String messageID = line.substring(idStart, idEnd);
        int dlcStart = idEnd + 2;
        int dlcEnd = line.indexOf("]", dlcStart);
        if (dlcEnd == -1)
          continue;

        String dlc = line.substring(dlcStart, dlcEnd);
        int dataStart = dlcEnd + 2;
        String dataBytes = line.substring(dataStart);

        currentFrame.ID = strtol(messageID.c_str(), NULL, 16);
        currentFrame.DLC = dlc.toInt();

        int byteIndex = 0;
        char* token = strtok((char*)dataBytes.c_str(), " ");
        while (token != NULL && byteIndex < currentFrame.DLC) {
          currentFrame.data.u8[byteIndex++] = strtol(token, NULL, 16);
          token = strtok(NULL, " ");
        }

        currentFrame.FD = (datalayer.system.info.can_replay_interface == CANFD_NATIVE) ||
                          (datalayer.system.info.can_replay_interface == CANFD_ADDON_MCP2518);
        currentFrame.ext_ID = (currentFrame.ID > 0x7F0);

        transmit_can_frame_to_interface(&currentFrame, (CAN_Interface)datalayer.system.info.can_replay_interface);
      }
    } while (datalayer.system.info.loop_playback);

    messages.clear();          // Free vector memory
    messages.shrink_to_fit();  // Release excess memory
  }

  isReplayRunning = false;  // Mark replay as stopped
  vTaskDelete(NULL);
}

void def_route_with_auth(const char* uri, AsyncWebServer& serv, WebRequestMethodComposite method,
                         std::function<void(AsyncWebServerRequest*)> handler) {
  serv.on(uri, method, [handler](AsyncWebServerRequest* request) {
    if (webserver_auth && !request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    handler(request);
  });
}

void init_webserver() {

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(401); });

  // Route for firmware info from ota update page
  def_route_with_auth("/GetFirmwareInfo", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", get_firmware_info_html, get_firmware_info_processor);
  });

  // Route for root / web page
  def_route_with_auth("/", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    // Clear OTA active flag as a safeguard in case onOTAEnd() wasn't called
    ota_active = false;
    request->send(200, "text/html", index_html, processor);
  });

  // Route for going to settings web page
  def_route_with_auth("/settings", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    // Using make_shared to ensure lifetime for the settings object during send() lambda execution
    auto settings = std::make_shared<BatteryEmulatorSettingsStore>(true);

    request->send(200, "text/html", settings_html,
                  [settings](const String& content) { return settings_processor(content, *settings); });
  });

  // Route for going to advanced battery info web page
  def_route_with_auth("/advanced", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html, advanced_battery_processor);
  });

  // Route for going to CAN logging web page
  def_route_with_auth("/canlog", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(request->beginResponse(200, "text/html", can_logger_processor()));
  });

  // Route for going to CAN replay web page
  def_route_with_auth("/canreplay", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(request->beginResponse(200, "text/html", can_replay_processor()));
  });

  def_route_with_auth("/startReplay", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    // Prevent multiple replay tasks from being created
    if (isReplayRunning) {
      request->send(400, "text/plain", "Replay already running!");
      return;
    }

    datalayer.system.info.loop_playback = request->hasParam("loop") && request->getParam("loop")->value().toInt() == 1;
    isReplayRunning = true;  // Set flag before starting task

    xTaskCreatePinnedToCore(canReplayTask, "CAN_Replay", 8192, NULL, 1, NULL, 1);

    request->send(200, "text/plain", "CAN replay started!");
  });

  // Route for stopping the CAN replay
  def_route_with_auth("/stopReplay", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    datalayer.system.info.loop_playback = false;

    request->send(200, "text/plain", "CAN replay stopped!");
  });

  // Route to handle setting the CAN interface for CAN replay
  def_route_with_auth("/setCANInterface", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->hasParam("interface")) {
      String canInterface = request->getParam("interface")->value();

      // Convert the received value to an integer
      int interfaceValue = canInterface.toInt();

      // Update the datalayer with the selected interface
      datalayer.system.info.can_replay_interface = interfaceValue;

      // Respond with success message
      request->send(200, "text/plain", "New interface selected");
    } else {
      request->send(400, "text/plain", "Error: updating interface failed");
    }
  });

  if (datalayer.system.info.web_logging_active || datalayer.system.info.SD_logging_active) {
    // Route for going to debug logging web page
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(200, "text/html", debug_logger_processor());
      request->send(response);
    });
  }

  // Define the handler to stop can logging
  server.on("/stop_can_logging", HTTP_GET, [](AsyncWebServerRequest* request) {
    datalayer.system.info.can_logging_active = false;
    request->send(200, "text/plain", "Logging stopped");
  });

  // Define the handler to import can log
  server.on(
      "/import_can_log", HTTP_POST,
      [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Ready to receive file.");  // Response when request is made
      },
      handleFileUpload);

  if (datalayer.system.info.CAN_SD_logging_active) {
    // Define the handler to export can log
    server.on("/export_can_log", HTTP_GET, [](AsyncWebServerRequest* request) {
      pause_can_writing();
      request->send(SD_MMC, CAN_LOG_FILE, String(), true);
      resume_can_writing();
    });

    // Define the handler to delete can log
    server.on("/delete_can_log", HTTP_GET, [](AsyncWebServerRequest* request) {
      delete_can_log();
      request->send(200, "text/plain", "Log file deleted");
    });
  } else {
    // Define the handler to export can log
    server.on("/export_can_log", HTTP_GET, [](AsyncWebServerRequest* request) {
      String logs = String(datalayer.system.info.logged_can_messages);
      if (logs.length() == 0) {
        logs = "No logs available.";
      }

      // Get the current time
      time_t now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);

      // Ensure time retrieval was successful
      char filename[32];
      if (strftime(filename, sizeof(filename), "canlog_%H-%M-%S.txt", &timeinfo)) {
        // Valid filename created
      } else {
        // Fallback filename if automatic timestamping failed
        strcpy(filename, "battery_emulator_can_log.txt");
      }

      // Use request->send with dynamic headers
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", logs);
      response->addHeader("Content-Disposition", String("attachment; filename=\"") + String(filename) + "\"");
      request->send(response);
    });
  }

  if (datalayer.system.info.SD_logging_active) {
    // Define the handler to delete log file
    server.on("/delete_log", HTTP_GET, [](AsyncWebServerRequest* request) {
      delete_log();
      request->send(200, "text/plain", "Log file deleted");
    });

    // Define the handler to export debug log
    server.on("/export_log", HTTP_GET, [](AsyncWebServerRequest* request) {
      pause_log_writing();
      request->send(SD_MMC, LOG_FILE, String(), true);
      resume_log_writing();
    });
  } else {
    // Define the handler to export debug log
    server.on("/export_log", HTTP_GET, [](AsyncWebServerRequest* request) {
      String logs = String(datalayer.system.info.logged_can_messages);
      if (logs.length() == 0) {
        logs = "No logs available.";
      }

      // Get the current time
      time_t now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);

      // Ensure time retrieval was successful
      char filename[32];
      if (strftime(filename, sizeof(filename), "log_%H-%M-%S.txt", &timeinfo)) {
        // Valid filename created
      } else {
        // Fallback filename if automatic timestamping failed
        strcpy(filename, "battery_emulator_log.txt");
      }

      // Use request->send with dynamic headers
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", logs);
      response->addHeader("Content-Disposition", String("attachment; filename=\"") + String(filename) + "\"");
      request->send(response);
    });
  }

  // Route for going to cellmonitor web page
  def_route_with_auth("/cellmonitor", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html, cellmonitor_processor);
  });

  // Route for going to event log web page
  def_route_with_auth("/events", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html, events_processor);
  });

  // Route for clearing all events
  def_route_with_auth("/clearevents", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    reset_all_events();
    // Send back a response that includes an instant redirect to /events
    String response = "<html><body>";
    response += "<script>window.location.href = '/events';</script>";  // Instant redirect
    response += "</body></html>";
    request->send(200, "text/html", response);
  });

  def_route_with_auth("/factoryReset", server, HTTP_POST, [](AsyncWebServerRequest* request) {
    // Reset all settings to factory defaults
    BatteryEmulatorSettingsStore settings;
    settings.clearAll();

    request->send(200, "text/html", "OK");
  });

  const char* boolSettingNames[] = {
      "DBLBTR",        "CNTCTRL",      "CNTCTRLDBL",  "PWMCNTCTRL",   "PERBMSRESET",  "SDLOGENABLED", "STATICIP",
      "REMBMSRESET",   "EXTPRECHARGE", "USBENABLED",  "CANLOGUSB",    "WEBENABLED",   "CANFDASCAN",   "CANLOGSD",
      "WIFIAPENABLED", "MQTTENABLED",  "NOINVDISC",   "HADISC",       "MQTTTOPICS",   "MQTTCELLV",    "INVICNT",
      "GTWRHD",        "DIGITALHVIL",  "PERFPROFILE", "INTERLOCKREQ", "SOCESTIMATED", "PYLONOFFSET",  "PYLONORDER",
      "DEYEBYD",       "NCCONTACTOR",  "TRIBTR",      "CNTCTRLTRI",
  };

  const char* uintSettingNames[] = {
      "BATTCVMAX", "BATTCVMIN",  "MAXPRETIME", "MAXPREFREQ", "WIFICHANNEL", "DCHGPOWER", "CHGPOWER",
      "LOCALIP1",  "LOCALIP2",   "LOCALIP3",   "LOCALIP4",   "GATEWAY1",    "GATEWAY2",  "GATEWAY3",
      "GATEWAY4",  "SUBNET1",    "SUBNET2",    "SUBNET3",    "SUBNET4",     "MQTTPORT",  "MQTTTIMEOUT",
      "SOFAR_ID",  "PYLONSEND",  "INVCELLS",   "INVMODULES", "INVCELLSPER", "INVVLEVEL", "INVCAPACITY",
      "INVBTYPE",  "CANFREQ",    "CANFDFREQ",  "PRECHGMS",   "PWMFREQ",     "PWMHOLD",   "GTWCOUNTRY",
      "GTWMAPREG", "GTWCHASSIS", "GTWPACK",    "LEDMODE",    "GPIOOPT1",    "GPIOOPT2",  "GPIOOPT3",
  };

  const char* stringSettingNames[] = {"APNAME",       "APPASSWORD", "HOSTNAME",        "MQTTSERVER",     "MQTTUSER",
                                      "MQTTPASSWORD", "MQTTTOPIC",  "MQTTOBJIDPREFIX", "MQTTDEVICENAME", "HADEVICEID"};

  // Handles the form POST from UI to save settings of the common image
  server.on("/saveSettings", HTTP_POST,
            [boolSettingNames, stringSettingNames, uintSettingNames](AsyncWebServerRequest* request) {
              BatteryEmulatorSettingsStore settings;

              int numParams = request->params();
              for (int i = 0; i < numParams; i++) {
                auto p = request->getParam(i);
                if (p->name() == "inverter") {
                  auto type = static_cast<InverterProtocolType>(atoi(p->value().c_str()));
                  settings.saveUInt("INVTYPE", (int)type);
                } else if (p->name() == "INVCOMM") {
                  auto type = static_cast<comm_interface>(atoi(p->value().c_str()));
                  settings.saveUInt("INVCOMM", (int)type);
                } else if (p->name() == "battery") {
                  auto type = static_cast<BatteryType>(atoi(p->value().c_str()));
                  settings.saveUInt("BATTTYPE", (int)type);
                } else if (p->name() == "BATTCHEM") {
                  auto type = static_cast<battery_chemistry_enum>(atoi(p->value().c_str()));
                  settings.saveUInt("BATTCHEM", (int)type);
                } else if (p->name() == "BATTCOMM") {
                  auto type = static_cast<comm_interface>(atoi(p->value().c_str()));
                  settings.saveUInt("BATTCOMM", (int)type);
                } else if (p->name() == "BATTPVMAX") {
                  auto type = p->value().toFloat() * 10.0f;
                  settings.saveUInt("BATTPVMAX", (int)type);
                } else if (p->name() == "BATTPVMIN") {
                  auto type = p->value().toFloat() * 10.0f;
                  settings.saveUInt("BATTPVMIN", (int)type);
                } else if (p->name() == "charger") {
                  auto type = static_cast<ChargerType>(atoi(p->value().c_str()));
                  settings.saveUInt("CHGTYPE", (int)type);
                } else if (p->name() == "CHGCOMM") {
                  auto type = static_cast<comm_interface>(atoi(p->value().c_str()));
                  settings.saveUInt("CHGCOMM", (int)type);
                } else if (p->name() == "EQSTOP") {
                  auto type = static_cast<STOP_BUTTON_BEHAVIOR>(atoi(p->value().c_str()));
                  settings.saveUInt("EQSTOP", (int)type);
                } else if (p->name() == "BATT2COMM") {
                  auto type = static_cast<comm_interface>(atoi(p->value().c_str()));
                  settings.saveUInt("BATT2COMM", (int)type);
                } else if (p->name() == "BATT3COMM") {
                  auto type = static_cast<comm_interface>(atoi(p->value().c_str()));
                  settings.saveUInt("BATT3COMM", (int)type);
                } else if (p->name() == "shunt") {
                  auto type = static_cast<ShuntType>(atoi(p->value().c_str()));
                  settings.saveUInt("SHUNTTYPE", (int)type);
                } else if (p->name() == "SHUNTCOMM") {
                  auto type = static_cast<comm_interface>(atoi(p->value().c_str()));
                  settings.saveUInt("SHUNTCOMM", (int)type);
                } else if (p->name() == "SSID") {
                  settings.saveString("SSID", p->value().c_str());
                  ssid = settings.getString("SSID", "").c_str();
                } else if (p->name() == "PASSWORD") {
                  settings.saveString("PASSWORD", p->value().c_str());
                  password = settings.getString("PASSWORD", "").c_str();
                } else if (p->name() == "MQTTPUBLISHMS") {
                  auto interval = atoi(p->value().c_str()) * 1000;  // Convert seconds to milliseconds
                  settings.saveUInt("MQTTPUBLISHMS", interval);
                }

                for (auto& uintSetting : uintSettingNames) {
                  if (p->name() == uintSetting) {
                    auto value = atoi(p->value().c_str());
                    if (settings.getUInt(uintSetting, 0) != value) {
                      settings.saveUInt(uintSetting, value);
                    }
                  }
                }

                for (auto& stringSetting : stringSettingNames) {
                  if (p->name() == stringSetting) {
                    if (settings.getString(stringSetting) != p->value()) {
                      settings.saveString(stringSetting, p->value().c_str());
                    }
                  }
                }
              }

              for (auto& boolSetting : boolSettingNames) {
                auto p = request->getParam(boolSetting, true);
                const bool default_value = (std::string(boolSetting) == std::string("WIFIAPENABLED"));
                const bool value = p != nullptr && p->value() == "on";
                if (settings.getBool(boolSetting, default_value) != value) {
                  settings.saveBool(boolSetting, value);
                }
              }

              settingsUpdated = settings.were_settings_updated();
              request->redirect("/settings");
            });

  auto update_string = [](const char* route, std::function<void(String)> setter,
                          std::function<bool(String)> validator = nullptr) {
    def_route_with_auth(route, server, HTTP_GET, [=](AsyncWebServerRequest* request) {
      if (request->hasParam("value")) {
        String value = request->getParam("value")->value();

        if (validator && !validator(value)) {
          request->send(400, "text/plain", "Invalid value");
          return;
        }

        setter(value);
        request->send(200, "text/plain", "Updated successfully");
      } else {
        request->send(400, "text/plain", "Bad Request");
      }
    });
  };

  auto update_string_setting = [=](const char* route, std::function<void(String)> setter,
                                   std::function<bool(String)> validator = nullptr) {
    update_string(
        route,
        [setter](String value) {
          setter(value);
          store_settings();
        },
        validator);
  };

  auto update_int_setting = [=](const char* route, std::function<void(int)> setter) {
    update_string_setting(route, [setter](String value) { setter(value.toInt()); });
  };

  // Route for editing Wh
  update_int_setting("/updateBatterySize", [](int value) { datalayer.battery.info.total_capacity_Wh = value; });

  // Route for editing USE_SCALED_SOC
  update_int_setting("/updateUseScaledSOC", [](int value) { datalayer.battery.settings.soc_scaling_active = value; });

  // Route for enabling recovery mode charging
  update_int_setting("/enableRecoveryMode",
                     [](int value) { datalayer.battery.settings.user_requests_forced_charging_recovery_mode = value; });

  // Route for editing SOCMax
  update_string_setting("/updateSocMax", [](String value) {
    datalayer.battery.settings.max_percentage = static_cast<uint16_t>(value.toFloat() * 100);
  });

  // Route for editing CAN ID cutoff filter
  update_int_setting("/set_can_id_cutoff", [](int value) { user_selected_CAN_ID_cutoff_filter = value; });

  // Route for pause/resume Battery emulator
  update_string("/pause", [](String value) { setBatteryPause(value == "true" || value == "1", false); });

  // Route for equipment stop/resume
  update_string("/equipmentStop", [](String value) {
    if (value == "true" || value == "1") {
      setBatteryPause(true, false, true);  //Pause battery, do not pause CAN, equipment stop on (store to flash)
    } else {
      setBatteryPause(false, false, false);
    }
  });

  // Route for editing SOCMin
  update_string_setting("/updateSocMin", [](String value) {
    datalayer.battery.settings.min_percentage = static_cast<uint16_t>(value.toFloat() * 100);
  });

  // Route for editing MaxChargeA
  update_string_setting("/updateMaxChargeA", [](String value) {
    datalayer.battery.settings.max_user_set_charge_dA = static_cast<uint16_t>(value.toFloat() * 10);
  });

  // Route for editing MaxDischargeA
  update_string_setting("/updateMaxDischargeA", [](String value) {
    datalayer.battery.settings.max_user_set_discharge_dA = static_cast<uint16_t>(value.toFloat() * 10);
  });

  for (const auto& cmd : battery_commands) {
    auto route = String("/") + cmd.identifier;
    server.on(
        route.c_str(), HTTP_PUT,
        [cmd](AsyncWebServerRequest* request) {
          if (webserver_auth && !request->authenticate(http_username.c_str(), http_password.c_str())) {
            return request->requestAuthentication();
          }
        },
        nullptr,
        [cmd](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
          String battIndex = "";
          if (len > 0) {
            battIndex += (char)data[0];
          }
          Battery* batt = battery;
          if (battIndex == "1") {
            batt = battery2;
          }
          if (battIndex == "2") {
            batt = battery3;
          }
          if (batt) {
            cmd.action(batt);
          }
          request->send(200, "text/plain", "Command performed.");
        });
  }

  // Route for editing BATTERY_USE_VOLTAGE_LIMITS
  update_int_setting("/updateUseVoltageLimit",
                     [](int value) { datalayer.battery.settings.user_set_voltage_limits_active = value; });

  // Route for editing MaxChargeVoltage
  update_string_setting("/updateMaxChargeVoltage", [](String value) {
    datalayer.battery.settings.max_user_set_charge_voltage_dV = static_cast<uint16_t>(value.toFloat() * 10);
  });

  // Route for editing MaxDischargeVoltage
  update_string_setting("/updateMaxDischargeVoltage", [](String value) {
    datalayer.battery.settings.max_user_set_discharge_voltage_dV = static_cast<uint16_t>(value.toFloat() * 10);
  });

  // Route for editing BMSresetDuration
  update_string_setting("/updateBMSresetDuration", [](String value) {
    datalayer.battery.settings.user_set_bms_reset_duration_ms = static_cast<uint16_t>(value.toFloat() * 1000);
  });

  // Route for editing FakeBatteryVoltage
  update_string_setting("/updateFakeBatteryVoltage", [](String value) { battery->set_fake_voltage(value.toFloat()); });

  // Route for editing balancing enabled
  update_int_setting("/TeslaBalAct", [](int value) { datalayer.battery.settings.user_requests_balancing = value; });

  // Route for editing balancing max time
  update_string_setting("/BalTime", [](String value) {
    datalayer.battery.settings.balancing_max_time_ms = static_cast<uint32_t>(value.toFloat() * 60000);
  });

  // Route for editing balancing max power
  update_string_setting("/BalFloatPower", [](String value) {
    datalayer.battery.settings.balancing_float_power_W = static_cast<uint16_t>(value.toFloat());
  });

  // Route for editing balancing max pack voltage
  update_string_setting("/BalMaxPackV", [](String value) {
    datalayer.battery.settings.balancing_max_pack_voltage_dV = static_cast<uint16_t>(value.toFloat() * 10);
  });

  // Route for editing balancing max cell voltage
  update_string_setting("/BalMaxCellV", [](String value) {
    datalayer.battery.settings.balancing_max_cell_voltage_mV = static_cast<uint16_t>(value.toFloat());
  });

  // Route for editing balancing max cell voltage deviation
  update_string_setting("/BalMaxDevCellV", [](String value) {
    datalayer.battery.settings.balancing_max_deviation_cell_voltage_mV = static_cast<uint16_t>(value.toFloat());
  });

  if (charger) {
    // Route for editing ChargerTargetV
    update_string_setting(
        "/updateChargeSetpointV", [](String value) { datalayer.charger.charger_setpoint_HV_VDC = value.toFloat(); },
        [](String value) {
          float val = value.toFloat();
          return (val <= CHARGER_MAX_HV && val >= CHARGER_MIN_HV) &&
                 (val * datalayer.charger.charger_setpoint_HV_IDC <= CHARGER_MAX_POWER);
        });

    // Route for editing ChargerTargetA
    update_string_setting(
        "/updateChargeSetpointA", [](String value) { datalayer.charger.charger_setpoint_HV_IDC = value.toFloat(); },
        [](String value) {
          float val = value.toFloat();
          return (val <= CHARGER_MAX_A) && (val <= datalayer.battery.settings.max_user_set_charge_dA) &&
                 (val * datalayer.charger.charger_setpoint_HV_VDC <= CHARGER_MAX_POWER);
        });

    // Route for editing ChargerEndA
    update_string_setting("/updateChargeEndA",
                          [](String value) { datalayer.charger.charger_setpoint_HV_IDC_END = value.toFloat(); });

    // Route for enabling/disabling HV charger
    update_int_setting("/updateChargerHvEnabled",
                       [](int value) { datalayer.charger.charger_HV_enabled = (bool)value; });

    // Route for enabling/disabling aux12v charger
    update_int_setting("/updateChargerAux12vEnabled",
                       [](int value) { datalayer.charger.charger_aux12V_enabled = (bool)value; });
  }

  // Send a GET request to <ESP_IP>/update
  def_route_with_auth("/debug", server, HTTP_GET,
                      [](AsyncWebServerRequest* request) { request->send(200, "text/plain", "Debug: all OK."); });

  // Route to handle reboot command
  def_route_with_auth("/reboot", server, HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Rebooting server...");

    //Equipment STOP without persisting the equipment state before restart
    // Max Charge/Discharge = 0; CAN = stop; contactors = open
    setBatteryPause(true, true, true, false);
    delay(1000);
    ESP.restart();
  });

  // Initialize ElegantOTA
  init_ElegantOTA();

  // Start server
  server.begin();
}

String getConnectResultString(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED:
      return "Connected";
    case WL_NO_SHIELD:
      return "No shield";
    case WL_IDLE_STATUS:
      return "Idle status";
    case WL_NO_SSID_AVAIL:
      return "No SSID available";
    case WL_SCAN_COMPLETED:
      return "Scan completed";
    case WL_CONNECT_FAILED:
      return "Connect failed";
    case WL_CONNECTION_LOST:
      return "Connection lost";
    case WL_DISCONNECTED:
      return "Disconnected";
    default:
      return "Unknown";
  }
}

void ota_monitor() {
  if (ota_active && ota_timeout_timer.elapsed()) {
    // OTA timeout, try to restore can and clear the update event
    set_event(EVENT_OTA_UPDATE_TIMEOUT, 0);
    onOTAEnd(false);
  }
}

// Function to initialize ElegantOTA
void init_ElegantOTA() {
  ElegantOTA.begin(&server);  // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
}

String get_firmware_info_processor(const String& var) {
  if (var == "X") {
    String content = "";
    static JsonDocument doc;

    doc["hardware"] = esp32hal->name();
    doc["firmware"] = String(version_number);
    serializeJson(doc, content);
    return content;
  }
  return String();
}

String get_uptime() {
  uint64_t milliseconds;
  uint32_t remaining_seconds_in_day;
  uint32_t remaining_seconds;
  uint32_t remaining_minutes;
  uint32_t remaining_hours;
  uint16_t total_days;

  milliseconds = millis64();

  //convert passed millis to days, hours, minutes, seconds
  total_days = milliseconds / (1000 * 60 * 60 * 24);
  remaining_seconds_in_day = (milliseconds / 1000) % (60 * 60 * 24);
  remaining_hours = remaining_seconds_in_day / (60 * 60);
  remaining_minutes = (remaining_seconds_in_day % (60 * 60)) / 60;
  remaining_seconds = remaining_seconds_in_day % 60;

  return (String)total_days + " days, " + (String)remaining_hours + " hours, " + (String)remaining_minutes +
         " minutes, " + (String)remaining_seconds + " seconds";
}

String processor(const String& var) {
  if (var == "X") {
    String content = "";
    content += "<style>";
    content += "body { background-color: black; color: white; }";
    content +=
        "button { background-color: #505E67; color: white; border: none; padding: 10px 20px; margin-bottom: 20px; "
        "cursor: pointer; border-radius: 10px; }";
    content += "button:hover { background-color: #3A4A52; }";
    content += "h2 { font-size: 1.2em; margin: 0.3em 0 0.5em 0; }";
    content += "h4 { margin: 0.6em 0; line-height: 1.2; }";
    //content += ".tooltip { position: relative; display: inline-block; }";
    content += ".tooltip .tooltiptext {";
    content += "  visibility: hidden;";
    content += "  width: 200px;";
    content += "  background-color: #3A4A52;";  // Matching your button hover color
    content += "  color: white;";
    content += "  text-align: center;";
    content += "  border-radius: 6px;";
    content += "  padding: 8px;";
    content += "  position: absolute;";
    content += "  z-index: 1;";
    content += "  margin-left: -100px;";
    content += "  opacity: 0;";
    content += "  transition: opacity 0.3s;";
    content += "  font-size: 0.9em;";
    content += "  font-weight: normal;";
    content += "  line-height: 1.4;";
    content += "}";
    content += ".tooltip:hover .tooltiptext { visibility: visible; opacity: 1; }";
    content += ".tooltip-icon { color: #505E67; cursor: help; }";  // Matching your button color
    content += "</style>";

    // Compact header
    content += "<h2>Battery Emulator</h2>";

    // Start content block
    content += "<div style='background-color: #303E47; padding: 10px; margin-bottom: 10px; border-radius: 50px'>";
    content += "<h4>Software: " + String(version_number);

// Show hardware used:
#ifdef HW_LILYGO
    content += " Hardware: LilyGo T-CAN485";
#endif  // HW_LILYGO
#ifdef HW_LILYGO2CAN
    content += " Hardware: LilyGo T_2CAN";
#endif  // HW_LILYGO2CAN
#ifdef HW_BECOM
    content += " Hardware: BECom";
#endif  // HW_BECOM
#ifdef HW_STARK
    content += " Hardware: Stark CMR Module";
#endif  // HW_STARK
    content += " @ " + String(datalayer.system.info.CPU_temperature, 1) + " &deg;C</h4>";
    content += "<h4>Uptime: " + get_uptime() + "</h4>";
    if (datalayer.system.info.performance_measurement_active) {
      // Load information
      content += "<h4>Core task max load: " + String(datalayer.system.status.core_task_max_us) + " us</h4>";
      content +=
          "<h4>Core task max load last 10 s: " + String(datalayer.system.status.core_task_10s_max_us) + " us</h4>";
      content +=
          "<h4>MQTT function (MQTT task) max load last 10 s: " + String(datalayer.system.status.mqtt_task_10s_max_us) +
          " us</h4>";
      content +=
          "<h4>WIFI function (MQTT task) max load last 10 s: " + String(datalayer.system.status.wifi_task_10s_max_us) +
          " us</h4>";
      content += "<h4>Max load @ worst case execution of core task:</h4>";
      content += "<h4>10ms function timing: " + String(datalayer.system.status.time_snap_10ms_us) + " us</h4>";
      content += "<h4>Values function timing: " + String(datalayer.system.status.time_snap_values_us) + " us</h4>";
      content += "<h4>CAN/serial RX function timing: " + String(datalayer.system.status.time_snap_comm_us) + " us</h4>";
      content += "<h4>CAN TX function timing: " + String(datalayer.system.status.time_snap_cantx_us) + " us</h4>";
      content += "<h4>OTA function timing: " + String(datalayer.system.status.time_snap_ota_us) + " us</h4>";
    }

    wl_status_t status = WiFi.status();
    // Display ssid of network connected to and, if connected to the WiFi, its own IP
    content += "<h4>SSID: " + html_escape(ssid.c_str());
    if (status == WL_CONNECTED) {
      // Get and display the signal strength (RSSI) and channel
      content += " RSSI:" + String(WiFi.RSSI()) + " dBm Ch: " + String(WiFi.channel());
    }
    content += "</h4>";
    if (status == WL_CONNECTED) {
      content += "<h4>Hostname: " + html_escape(WiFi.getHostname()) + "</h4>";
      content += "<h4>IP: " + WiFi.localIP().toString() + "</h4>";
    } else {
      content += "<h4>Wifi state: " + getConnectResultString(status) + "</h4>";
    }
    // Close the block
    content += "</div>";

    if (inverter || battery || charger || user_selected_shunt_type != ShuntType::None) {
      // Start a new block with a specific background color
      content += "<div style='background-color: #333; padding: 10px; margin-bottom: 10px; border-radius: 50px'>";

      // Display which components are used
      if (inverter) {
        content += "<h4 style='color: white;'>Inverter protocol: ";
        content += inverter->name();
        content += " ";
        content += datalayer.system.info.inverter_brand;
        content += "</h4>";
      }

      if (battery) {
        content += "<h4 style='color: white;'>Battery protocol: ";
        content += datalayer.system.info.battery_protocol;
        if (battery3) {
          content += " (Triple battery)";
        } else if (battery2) {
          content += " (Double battery)";
        }
        if (datalayer.battery.info.chemistry == battery_chemistry_enum::LFP) {
          content += " (LFP)";
        }
        content += "</h4>";
      }

      if (user_selected_shunt_type != ShuntType::None) {
        content += "<h4 style='color: white;'>Shunt protocol: ";
        content += datalayer.system.info.shunt_protocol;
        content += "</h4>";
      }

      if (charger) {
        content += "<h4 style='color: white;'>Charger protocol: ";
        content += charger->name();
        content += "</h4>";
      }

      // Close the block
      content += "</div>";
    }

    if (battery) {
      if (battery2) {
        // Start a new block with a specific background color. Color changes depending on BMS status
        content += "<div style='display: flex; width: 100%;'>";
        content += "<div style='flex: 1; background-color: ";
      } else {
        // Start a new block with a specific background color. Color changes depending on system status
        content += "<div style='background-color: ";
      }

      switch (get_emulator_status()) {
        case EMULATOR_STATUS::STATUS_OK:
          content += "#2D3F2F;";
          break;
        case EMULATOR_STATUS::STATUS_WARNING:
          content += "#F5CC00;";
          break;
        case EMULATOR_STATUS::STATUS_ERROR:
          content += "#A70107;";
          break;
        case EMULATOR_STATUS::STATUS_UPDATING:
          content += "#2B35AF;";  // Blue in test mode
          break;
      }

      // Add the common style properties
      content += "padding: 10px; margin-bottom: 10px; border-radius: 50px;'>";

      // Display battery statistics within this block
      float socRealFloat =
          static_cast<float>(datalayer.battery.status.real_soc) / 100.0f;  // Convert to float and divide by 100
      float socScaledFloat =
          static_cast<float>(datalayer.battery.status.reported_soc) / 100.0f;  // Convert to float and divide by 100
      float sohFloat =
          static_cast<float>(datalayer.battery.status.soh_pptt) / 100.0f;  // Convert to float and divide by 100
      float voltageFloat =
          static_cast<float>(datalayer.battery.status.voltage_dV) / 10.0f;  // Convert to float and divide by 10
      float currentFloat =
          static_cast<float>(datalayer.battery.status.current_dA) / 10.0f;  // Convert to float and divide by 10
      float powerFloat = static_cast<float>(datalayer.battery.status.active_power_W);                // Convert to float
      float tempMaxFloat = static_cast<float>(datalayer.battery.status.temperature_max_dC) / 10.0f;  // Convert to float
      float tempMinFloat = static_cast<float>(datalayer.battery.status.temperature_min_dC) / 10.0f;  // Convert to float
      float maxCurrentChargeFloat =
          static_cast<float>(datalayer.battery.status.max_charge_current_dA) / 10.0f;  // Convert to float
      float maxCurrentDischargeFloat =
          static_cast<float>(datalayer.battery.status.max_discharge_current_dA) / 10.0f;  // Convert to float
      uint16_t cell_delta_mv =
          datalayer.battery.status.cell_max_voltage_mV - datalayer.battery.status.cell_min_voltage_mV;

      if (datalayer.battery.settings.soc_scaling_active)
        content += "<h4 style='color: white;'>Scaled SOC: " + String(socScaledFloat, 2) +
                   "&percnt; (real: " + String(socRealFloat, 2) + "&percnt;)</h4>";
      else
        content += "<h4 style='color: white;'>SOC: " + String(socRealFloat, 2) + "&percnt;</h4>";

      content += "<h4 style='color: white;'>SOH: " + String(sohFloat, 2) + "&percnt;</h4>";
      content += "<h4 style='color: white;'>Voltage: " + String(voltageFloat, 1) +
                 " V &nbsp; Current: " + String(currentFloat, 1) + " A</h4>";
      content += formatPowerValue("Power", powerFloat, "", 1);

      if (datalayer.battery.settings.soc_scaling_active)
        content += "<h4 style='color: white;'>Scaled total capacity: " +
                   formatPowerValue(datalayer.battery.info.reported_total_capacity_Wh, "h", 1) +
                   " (real: " + formatPowerValue(datalayer.battery.info.total_capacity_Wh, "h", 1) + ")</h4>";
      else
        content += formatPowerValue("Total capacity", datalayer.battery.info.total_capacity_Wh, "h", 1);

      if (datalayer.battery.settings.soc_scaling_active)
        content += "<h4 style='color: white;'>Scaled remaining capacity: " +
                   formatPowerValue(datalayer.battery.status.reported_remaining_capacity_Wh, "h", 1) +
                   " (real: " + formatPowerValue(datalayer.battery.status.remaining_capacity_Wh, "h", 1) + ")</h4>";
      else
        content += formatPowerValue("Remaining capacity", datalayer.battery.status.remaining_capacity_Wh, "h", 1);

      if (datalayer.system.info.equipment_stop_active) {
        content +=
            formatPowerValue("Max discharge power", datalayer.battery.status.max_discharge_power_W, "", 1, "red");
        content += formatPowerValue("Max charge power", datalayer.battery.status.max_charge_power_W, "", 1, "red");
        content += "<h4 style='color: red;'>Max discharge current: " + String(maxCurrentDischargeFloat, 1) + " A</h4>";
        content += "<h4 style='color: red;'>Max charge current: " + String(maxCurrentChargeFloat, 1) + " A</h4>";
      } else {
        content += formatPowerValue("Max discharge power", datalayer.battery.status.max_discharge_power_W, "", 1);
        content += formatPowerValue("Max charge power", datalayer.battery.status.max_charge_power_W, "", 1);
        content += "<h4 style='color: white;'>Max discharge current: " + String(maxCurrentDischargeFloat, 1) + " A";
        if (datalayer.battery.settings.remote_settings_limit_discharge) {
          content += " (Remote)</h4>";
        } else if (datalayer.battery.settings.user_settings_limit_discharge) {
          content += " (Manual)</h4>";
        } else {
          content += " (BMS)</h4>";
        }
        content += "<h4 style='color: white;'>Max charge current: " + String(maxCurrentChargeFloat, 1) + " A";
        if (datalayer.battery.settings.remote_settings_limit_charge) {
          content += " (Remote)</h4>";
        } else if (datalayer.battery.settings.user_settings_limit_charge) {
          content += " (Manual)</h4>";
        } else {
          content += " (BMS)</h4>";
        }
      }

      content += "<h4>Cell min/max: " + String(datalayer.battery.status.cell_min_voltage_mV) + " mV / " +
                 String(datalayer.battery.status.cell_max_voltage_mV) + " mV</h4>";
      if (cell_delta_mv > datalayer.battery.info.max_cell_voltage_deviation_mV) {
        content += "<h4 style='color: red;'>Cell delta: " + String(cell_delta_mv) + " mV</h4>";
      } else {
        content += "<h4>Cell delta: " + String(cell_delta_mv) + " mV</h4>";
      }
      content += "<h4>Temperature min/max: " + String(tempMinFloat, 1) + " &deg;C / " + String(tempMaxFloat, 1) +
                 " &deg;C</h4>";

      content += "<h4>System status: ";
      switch (datalayer.battery.status.bms_status) {
        case ACTIVE:
          content += String("OK");
          break;
        case UPDATING:
          content += String("UPDATING");
          break;
        case FAULT:
          content += String("FAULT");
          break;
        case INACTIVE:
          content += String("INACTIVE");
          break;
        case STANDBY:
          content += String("STANDBY");
          break;
        default:
          content += String("??");
          break;
      }
      content += "</h4>";

      if (battery && battery->supports_real_BMS_status()) {
        content += "<h4>Battery BMS status: ";
        switch (datalayer.battery.status.real_bms_status) {
          case BMS_ACTIVE:
            content += String("OK");
            break;
          case BMS_FAULT:
            content += String("FAULT");
            break;
          case BMS_DISCONNECTED:
            content += String("DISCONNECTED");
            break;
          case BMS_STANDBY:
            content += String("STANDBY");
            break;
          default:
            content += String("??");
            break;
        }
        content += "</h4>";
      }

      if (datalayer.battery.status.current_dA == 0) {
        content += "<h4>Battery idle</h4>";
      } else if (datalayer.battery.status.current_dA < 0) {
        content += "<h4>Battery discharging!";
        if (datalayer.battery.settings.inverter_limits_discharge) {
          content += " (Inverter limiting)</h4>";
        } else {
          if (datalayer.battery.settings.user_settings_limit_discharge) {
            content += " (Settings limiting)</h4>";
          } else {
            content += " (Battery limiting)</h4>";
          }
        }
        content += "</h4>";
      } else {  // > 0 , positive current
        content += "<h4>Battery charging!";
        if (datalayer.battery.settings.inverter_limits_charge) {
          content += " (Inverter limiting)</h4>";
        } else {
          if (datalayer.battery.settings.user_settings_limit_charge) {
            content += " (Settings limiting)</h4>";
          } else {
            content += " (Battery limiting)</h4>";
          }
        }
      }

      // Close the block
      content += "</div>";

      if (battery2) {
        content += "<div style='flex: 1; background-color: ";
        switch (datalayer.battery.status.bms_status) {
          case ACTIVE:
            content += "#2D3F2F;";
            break;
          case FAULT:
            content += "#A70107;";
            break;
          default:
            content += "#2D3F2F;";
            break;
        }
        // Add the common style properties
        content += "padding: 10px; margin-bottom: 10px; border-radius: 50px;'>";

        // Display battery statistics within this block
        socRealFloat =
            static_cast<float>(datalayer.battery2.status.real_soc) / 100.0f;  // Convert to float and divide by 100
        //socScaledFloat; // Same value used for bat2
        sohFloat =
            static_cast<float>(datalayer.battery2.status.soh_pptt) / 100.0f;  // Convert to float and divide by 100
        voltageFloat =
            static_cast<float>(datalayer.battery2.status.voltage_dV) / 10.0f;  // Convert to float and divide by 10
        currentFloat =
            static_cast<float>(datalayer.battery2.status.current_dA) / 10.0f;       // Convert to float and divide by 10
        powerFloat = static_cast<float>(datalayer.battery2.status.active_power_W);  // Convert to float
        tempMaxFloat = static_cast<float>(datalayer.battery2.status.temperature_max_dC) / 10.0f;  // Convert to float
        tempMinFloat = static_cast<float>(datalayer.battery2.status.temperature_min_dC) / 10.0f;  // Convert to float
        cell_delta_mv = datalayer.battery2.status.cell_max_voltage_mV - datalayer.battery2.status.cell_min_voltage_mV;

        if (datalayer.battery.settings.soc_scaling_active)
          content += "<h4 style='color: white;'>Scaled SOC: " + String(socScaledFloat, 2) +
                     "&percnt; (real: " + String(socRealFloat, 2) + "&percnt;)</h4>";
        else
          content += "<h4 style='color: white;'>SOC: " + String(socRealFloat, 2) + "&percnt;</h4>";

        content += "<h4 style='color: white;'>SOH: " + String(sohFloat, 2) + "&percnt;</h4>";
        content += "<h4 style='color: white;'>Voltage: " + String(voltageFloat, 1) +
                   " V &nbsp; Current: " + String(currentFloat, 1) + " A</h4>";
        content += formatPowerValue("Power", powerFloat, "", 1);

        if (datalayer.battery.settings.soc_scaling_active)
          content += "<h4 style='color: white;'>Scaled total capacity: " +
                     formatPowerValue(datalayer.battery2.info.reported_total_capacity_Wh, "h", 1) +
                     " (real: " + formatPowerValue(datalayer.battery2.info.total_capacity_Wh, "h", 1) + ")</h4>";
        else
          content += formatPowerValue("Total capacity", datalayer.battery2.info.total_capacity_Wh, "h", 1);

        if (datalayer.battery.settings.soc_scaling_active)
          content += "<h4 style='color: white;'>Scaled remaining capacity: " +
                     formatPowerValue(datalayer.battery2.status.reported_remaining_capacity_Wh, "h", 1) +
                     " (real: " + formatPowerValue(datalayer.battery2.status.remaining_capacity_Wh, "h", 1) + ")</h4>";
        else
          content += formatPowerValue("Remaining capacity", datalayer.battery2.status.remaining_capacity_Wh, "h", 1);

        if (datalayer.system.info.equipment_stop_active) {
          content +=
              formatPowerValue("Max discharge power", datalayer.battery2.status.max_discharge_power_W, "", 1, "red");
          content += formatPowerValue("Max charge power", datalayer.battery2.status.max_charge_power_W, "", 1, "red");
          content +=
              "<h4 style='color: red;'>Max discharge current: " + String(maxCurrentDischargeFloat, 1) + " A</h4>";
          content += "<h4 style='color: red;'>Max charge current: " + String(maxCurrentChargeFloat, 1) + " A</h4>";
        } else {
          content += formatPowerValue("Max discharge power", datalayer.battery2.status.max_discharge_power_W, "", 1);
          content += formatPowerValue("Max charge power", datalayer.battery2.status.max_charge_power_W, "", 1);
          content +=
              "<h4 style='color: white;'>Max discharge current: " + String(maxCurrentDischargeFloat, 1) + " A</h4>";
          content += "<h4 style='color: white;'>Max charge current: " + String(maxCurrentChargeFloat, 1) + " A</h4>";
        }

        content += "<h4>Cell min/max: " + String(datalayer.battery2.status.cell_min_voltage_mV) + " mV / " +
                   String(datalayer.battery2.status.cell_max_voltage_mV) + " mV</h4>";
        if (cell_delta_mv > datalayer.battery2.info.max_cell_voltage_deviation_mV) {
          content += "<h4 style='color: red;'>Cell delta: " + String(cell_delta_mv) + " mV</h4>";
        } else {
          content += "<h4>Cell delta: " + String(cell_delta_mv) + " mV</h4>";
        }
        content += "<h4>Temperature min/max: " + String(tempMinFloat, 1) + " &deg;C / " + String(tempMaxFloat, 1) +
                   " &deg;C</h4>";
        if (datalayer.battery.status.bms_status == ACTIVE) {
          content += "<h4>System status: OK </h4>";
        } else if (datalayer.battery.status.bms_status == UPDATING) {
          content += "<h4>System status: UPDATING </h4>";
        } else {
          content += "<h4>System status: FAULT </h4>";
        }
        if (datalayer.battery2.status.current_dA == 0) {
          content += "<h4>Battery idle</h4>";
        } else if (datalayer.battery2.status.current_dA < 0) {
          content += "<h4>Battery discharging!</h4>";
        } else {  // > 0
          content += "<h4>Battery charging!</h4>";
        }
        content += "</div>";
        if (battery3) {
          content += "<div style='flex: 1; background-color: ";
          switch (datalayer.battery.status.bms_status) {
            case ACTIVE:
              content += "#2D3F2F;";
              break;
            case FAULT:
              content += "#A70107;";
              break;
            default:
              content += "#2D3F2F;";
              break;
          }
          // Add the common style properties
          content += "padding: 10px; margin-bottom: 10px; border-radius: 50px;'>";

          // Display battery statistics within this block
          socRealFloat =
              static_cast<float>(datalayer.battery3.status.real_soc) / 100.0f;  // Convert to float and divide by 100
          //socScaledFloat; // Same value used for bat2
          sohFloat =
              static_cast<float>(datalayer.battery3.status.soh_pptt) / 100.0f;  // Convert to float and divide by 100
          voltageFloat =
              static_cast<float>(datalayer.battery3.status.voltage_dV) / 10.0f;  // Convert to float and divide by 10
          currentFloat =
              static_cast<float>(datalayer.battery3.status.current_dA) / 10.0f;  // Convert to float and divide by 10
          powerFloat = static_cast<float>(datalayer.battery3.status.active_power_W);                // Convert to float
          tempMaxFloat = static_cast<float>(datalayer.battery3.status.temperature_max_dC) / 10.0f;  // Convert to float
          tempMinFloat = static_cast<float>(datalayer.battery3.status.temperature_min_dC) / 10.0f;  // Convert to float
          cell_delta_mv = datalayer.battery3.status.cell_max_voltage_mV - datalayer.battery3.status.cell_min_voltage_mV;

          if (datalayer.battery.settings.soc_scaling_active)
            content += "<h4 style='color: white;'>Scaled SOC: " + String(socScaledFloat, 2) +
                       "&percnt; (real: " + String(socRealFloat, 2) + "&percnt;)</h4>";
          else
            content += "<h4 style='color: white;'>SOC: " + String(socRealFloat, 2) + "&percnt;</h4>";

          content += "<h4 style='color: white;'>SOH: " + String(sohFloat, 2) + "&percnt;</h4>";
          content += "<h4 style='color: white;'>Voltage: " + String(voltageFloat, 1) +
                     " V &nbsp; Current: " + String(currentFloat, 1) + " A</h4>";
          content += formatPowerValue("Power", powerFloat, "", 1);

          if (datalayer.battery.settings.soc_scaling_active)
            content += "<h4 style='color: white;'>Scaled total capacity: " +
                       formatPowerValue(datalayer.battery3.info.reported_total_capacity_Wh, "h", 1) +
                       " (real: " + formatPowerValue(datalayer.battery3.info.total_capacity_Wh, "h", 1) + ")</h4>";
          else
            content += formatPowerValue("Total capacity", datalayer.battery3.info.total_capacity_Wh, "h", 1);

          if (datalayer.battery.settings.soc_scaling_active)
            content += "<h4 style='color: white;'>Scaled remaining capacity: " +
                       formatPowerValue(datalayer.battery3.status.reported_remaining_capacity_Wh, "h", 1) +
                       " (real: " + formatPowerValue(datalayer.battery3.status.remaining_capacity_Wh, "h", 1) +
                       ")</h4>";
          else
            content += formatPowerValue("Remaining capacity", datalayer.battery3.status.remaining_capacity_Wh, "h", 1);

          if (datalayer.system.info.equipment_stop_active) {
            content +=
                formatPowerValue("Max discharge power", datalayer.battery3.status.max_discharge_power_W, "", 1, "red");
            content += formatPowerValue("Max charge power", datalayer.battery3.status.max_charge_power_W, "", 1, "red");
            content +=
                "<h4 style='color: red;'>Max discharge current: " + String(maxCurrentDischargeFloat, 1) + " A</h4>";
            content += "<h4 style='color: red;'>Max charge current: " + String(maxCurrentChargeFloat, 1) + " A</h4>";
          } else {
            content += formatPowerValue("Max discharge power", datalayer.battery3.status.max_discharge_power_W, "", 1);
            content += formatPowerValue("Max charge power", datalayer.battery3.status.max_charge_power_W, "", 1);
            content +=
                "<h4 style='color: white;'>Max discharge current: " + String(maxCurrentDischargeFloat, 1) + " A</h4>";
            content += "<h4 style='color: white;'>Max charge current: " + String(maxCurrentChargeFloat, 1) + " A</h4>";
          }

          content += "<h4>Cell min/max: " + String(datalayer.battery3.status.cell_min_voltage_mV) + " mV / " +
                     String(datalayer.battery3.status.cell_max_voltage_mV) + " mV</h4>";
          if (cell_delta_mv > datalayer.battery3.info.max_cell_voltage_deviation_mV) {
            content += "<h4 style='color: red;'>Cell delta: " + String(cell_delta_mv) + " mV</h4>";
          } else {
            content += "<h4>Cell delta: " + String(cell_delta_mv) + " mV</h4>";
          }
          content += "<h4>Temperature min/max: " + String(tempMinFloat, 1) + " &deg;C / " + String(tempMaxFloat, 1) +
                     " &deg;C</h4>";
          if (datalayer.battery.status.bms_status == ACTIVE) {
            content += "<h4>System status: OK </h4>";
          } else if (datalayer.battery.status.bms_status == UPDATING) {
            content += "<h4>System status: UPDATING </h4>";
          } else {
            content += "<h4>System status: FAULT </h4>";
          }
          if (datalayer.battery3.status.current_dA == 0) {
            content += "<h4>Battery idle</h4>";
          } else if (datalayer.battery3.status.current_dA < 0) {
            content += "<h4>Battery discharging!</h4>";
          } else {  // > 0
            content += "<h4>Battery charging!</h4>";
          }
          content += "</div>";
          content += "</div>";
        }
        content += "</div>";
      }
    }
    // Block for Contactor status and component request status
    // Start a new block with gray background color
    content += "<div style='background-color: #333; padding: 10px; margin-bottom: 10px;border-radius: 50px'>";

    if (emulator_pause_status == NORMAL) {
      content += "<h4>Power status: " + String(get_emulator_pause_status().c_str()) + " </h4>";
    } else {
      content += "<h4 style='color: red;'>Power status: " + String(get_emulator_pause_status().c_str()) + " </h4>";
    }

    content += "<h4>Emulator allows contactor closing: ";
    if (datalayer.battery.status.bms_status == FAULT) {
      content += "<span style='color: red;'>&#10005;</span>";
    } else {
      content += "<span>&#10003;</span>";
    }
    content += " Inverter allows contactor closing: ";
    if (datalayer.system.status.inverter_allows_contactor_closing == true) {
      content += "<span>&#10003;</span></h4>";
    } else {
      content += "<span style='color: red;'>&#10005;</span></h4>";
    }
    if (battery2) {
      content += "<h4>Secondary battery allowed to join ";
      if (datalayer.system.status.battery2_allowed_contactor_closing == true) {
        content += "<span>&#10003;</span>";
      } else {
        content += "<span style='color: red;'>&#10005; (voltage mismatch)</span>";
      }
    }

    if (!contactor_control_enabled) {
      content += "<div class=\"tooltip\">";
      content += "<h4>Contactors not fully controlled via emulator <span style=\"color:orange\">[?]</span></h4>";
      content +=
          "<span class=\"tooltiptext\">This means you are either running CAN controlled contactors OR manually "
          "powering the contactors. Battery-Emulator will have limited amount of control over the contactors!</span>";
      content += "</div>";
    } else {  //contactor_control_enabled TRUE
      content += "<div class=\"tooltip\"><h4>Contactors controlled by emulator, state: ";
      if (datalayer.system.status.contactors_engaged == 0) {
        content += "<span style='color: red;'>OFF (DISCONNECTED)</span>";
      } else if (datalayer.system.status.contactors_engaged == 1) {
        content += "<span style='color: green;'>ON</span>";
      } else if (datalayer.system.status.contactors_engaged == 2) {
        content += "<span style='color: red;'>OFF (FAULT)</span>";
        content += "<span class=\"tooltip-icon\"> [!]</span>";
        content +=
            "<span class=\"tooltiptext\">Emulator spent too much time in critical FAULT event. Investigate event "
            "causing this via Events page. Reboot required to resume operation!</span>";
      } else if (datalayer.system.status.contactors_engaged == 3) {
        content += "<span style='color: orange;'>PRECHARGE</span>";
      }
      content += "</h4></div>";
      if (contactor_control_enabled_double_battery && battery2) {
        content += "<h4>Secondary battery contactor, state: ";
        if (pwm_contactor_control) {
          if (datalayer.system.status.contactors_battery2_engaged) {
            content += "<span style='color: green;'>Economized</span>";
          } else {
            content += "<span style='color: red;'>OFF</span>";
          }
        } else if (
            esp32hal->SECOND_BATTERY_CONTACTORS_PIN() !=
            GPIO_NUM_NC) {  // No PWM_CONTACTOR_CONTROL , we can read the pin and see feedback. Helpful if channel overloaded
          if (digitalRead(esp32hal->SECOND_BATTERY_CONTACTORS_PIN()) == HIGH) {
            content += "<span style='color: green;'>ON</span>";
          } else {
            content += "<span style='color: red;'>OFF</span>";
          }
        }  //no PWM_CONTACTOR_CONTROL
        content += "</h4>";
      }
    }

    // Close the block
    content += "</div>";

    if (charger) {
      // Start a new block with orange background color
      content += "<div style='background-color: #FF6E00; padding: 10px; margin-bottom: 10px;border-radius: 50px'>";

      content += "<h4>Charger HV Enabled: ";
      if (datalayer.charger.charger_HV_enabled) {
        content += "<span>&#10003;</span>";
      } else {
        content += "<span style='color: red;'>&#10005;</span>";
      }
      content += "</h4>";

      content += "<h4>Charger Aux12v Enabled: ";
      if (datalayer.charger.charger_aux12V_enabled) {
        content += "<span>&#10003;</span>";
      } else {
        content += "<span style='color: red;'>&#10005;</span>";
      }
      content += "</h4>";

      auto chgPwrDC = charger->outputPowerDC();
      auto chgEff = charger->efficiency();

      content += formatPowerValue("Charger Output Power", chgPwrDC, "", 1);
      if (charger->efficiencySupported()) {
        content += "<h4 style='color: white;'>Charger Efficiency: " + String(chgEff) + "%</h4>";
      }

      float HVvol = charger->HVDC_output_voltage();
      float HVcur = charger->HVDC_output_current();
      float LVvol = charger->LVDC_output_voltage();
      float LVcur = charger->LVDC_output_current();

      content += "<h4 style='color: white;'>Charger HVDC Output V: " + String(HVvol, 2) + " V</h4>";
      content += "<h4 style='color: white;'>Charger HVDC Output I: " + String(HVcur, 2) + " A</h4>";
      content += "<h4 style='color: white;'>Charger LVDC Output I: " + String(LVcur, 2) + "</h4>";
      content += "<h4 style='color: white;'>Charger LVDC Output V: " + String(LVvol, 2) + "</h4>";

      float ACcur = charger->AC_input_current();
      float ACvol = charger->AC_input_voltage();

      content += "<h4 style='color: white;'>Charger AC Input V: " + String(ACvol, 2) + " VAC</h4>";
      content += "<h4 style='color: white;'>Charger AC Input I: " + String(ACcur, 2) + " A</h4>";

      content += "</div>";
    }

    if (emulator_pause_request_ON)
      content += "<button onclick='PauseBattery(false)'>Resume charge/discharge</button> ";
    else
      content +=
          "<button onclick=\"if(confirm('Are you sure you want to pause charging and discharging? This will set the "
          "maximum charge and discharge values to zero, preventing any further power flow.')) { PauseBattery(true); "
          "}\">Pause charge/discharge</button> ";

    content += "<button onclick='OTA()'>Perform OTA update</button> ";
    content += "<button onclick='Settings()'>Change Settings</button> ";
    content += "<button onclick='Advanced()'>More Battery Info</button> ";
    content += "<button onclick='CANlog()'>CAN logger</button> ";
    content += "<button onclick='CANreplay()'>CAN replay</button> ";
    if (datalayer.system.info.web_logging_active || datalayer.system.info.SD_logging_active) {
      content += "<button onclick='Log()'>Log</button> ";
    }
    content += "<button onclick='Cellmon()'>Cellmonitor</button> ";
    content += "<button onclick='Events()'>Events</button> ";
    content += "<button onclick='askReboot()'>Reboot Emulator</button>";
    if (webserver_auth)
      content += "<button onclick='logout()'>Logout</button>";
    if (!datalayer.system.info.equipment_stop_active)
      content +=
          "<br/><button style=\"background:red;color:white;cursor:pointer;\""
          " onclick=\""
          "if(confirm('This action will attempt to open contactors on the battery. Are you "
          "sure?')) { estop(true); }\""
          ">Open Contactors</button><br/>";
    else
      content +=
          "<br/><button style=\"background:green;color:white;cursor:pointer;\""
          "20px;font-size:16px;font-weight:bold;cursor:pointer;border-radius:5px; margin:10px;"
          " onclick=\""
          "if(confirm('This action will attempt to close contactors and enable power transfer. Are you sure?')) { "
          "estop(false); }\""
          ">Close Contactors</button><br/>";
    content += "<script>";
    content += "function OTA() { window.location.href = '/update'; }";
    content += "function Cellmon() { window.location.href = '/cellmonitor'; }";
    content += "function Settings() { window.location.href = '/settings'; }";
    content += "function Advanced() { window.location.href = '/advanced'; }";
    content += "function CANlog() { window.location.href = '/canlog'; }";
    content += "function CANreplay() { window.location.href = '/canreplay'; }";
    content += "function Log() { window.location.href = '/log'; }";
    content += "function Events() { window.location.href = '/events'; }";
    if (webserver_auth) {
      content += "function logout() {";
      content += "  var xhr = new XMLHttpRequest();";
      content += "  xhr.open('GET', '/logout', true);";
      content += "  xhr.send();";
      content += "  setTimeout(function(){ window.open(\"/\",\"_self\"); }, 1000);";
      content += "}";
    }
    content += "function PauseBattery(pause){";
    content +=
        "var xhr=new "
        "XMLHttpRequest();xhr.onload=function() { "
        "window.location.reload();};xhr.open('GET','/pause?value='+pause,true);xhr.send();";
    content += "}";
    content += "function estop(stop){";
    content +=
        "var xhr=new "
        "XMLHttpRequest();xhr.onload=function() { "
        "window.location.reload();};xhr.open('GET','/equipmentStop?value='+stop,true);xhr.send();";
    content += "}";
    content += "</script>";

    //Script for refreshing page
    content += "<script>";
    content += "setTimeout(function(){ location.reload(true); }, 15000);";
    content += "</script>";

    return content;
  }
  return String();
}

void onOTAStart() {
  //try to Pause the battery
  setBatteryPause(true, true);

  // Log when OTA has started
  set_event(EVENT_OTA_UPDATE, 0);

  // If already set, make a new attempt
  clear_event(EVENT_OTA_UPDATE_TIMEOUT);
  ota_active = true;

  ota_timeout_timer.reset();
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    logging.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
    // Reset the "watchdog"
    ota_timeout_timer.reset();
  }
}

void onOTAEnd(bool success) {

  ota_active = false;
  clear_event(EVENT_OTA_UPDATE);

  // Log when OTA has finished
  if (success) {
    //Equipment STOP without persisting the equipment state before restart
    // Max Charge/Discharge = 0; CAN = stop; contactors = open
    setBatteryPause(true, true, true, false);
    // a reboot will be done by the OTA library. no need to do anything here
    logging.println("OTA update finished successfully!");
  } else {
    logging.println("There was an error during OTA update!");
    //try to Resume the battery pause and CAN communication
    setBatteryPause(false, false);
  }
}

template <typename T>  // This function makes power values appear as W when under 1000, and kW when over
String formatPowerValue(String label, T value, String unit, int precision, String color) {
  String result = "<h4 style='color: " + color + ";'>" + label + ": ";
  result += formatPowerValue(value, unit, precision);
  result += "</h4>";
  return result;
}
template <typename T>  // This function makes power values appear as W when under 1000, and kW when over
String formatPowerValue(T value, String unit, int precision) {
  String result = "";

  if (std::is_same<T, float>::value || std::is_same<T, uint16_t>::value || std::is_same<T, uint32_t>::value) {
    float convertedValue = static_cast<float>(value);

    if (convertedValue >= 1000.0f || convertedValue <= -1000.0f) {
      result += String(convertedValue / 1000.0f, precision) + " kW";
    } else {
      result += String(convertedValue, 0) + " W";
    }
  }

  result += unit;
  return result;
}
#endif  // !HW_WAVESHARE7B_DISPLAY_ONLY
