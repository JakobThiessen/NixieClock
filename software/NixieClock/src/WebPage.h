/*


  HTML + CSS styling + javascript all in and undebuggable environment

  get all your HTML code (from html to /html) and past it into this test site
  muck with the HTML and CSS code until it's what you want
  https://www.w3schools.com/html/tryit.asp?filename=tryhtml_intro

  No clue how to debug javascrip other that write, compile, upload, refresh, guess, repeat

  I'm using class designators to set styles and id's for data updating
  for example:
  the CSS class .tabledata defines with the cell will look like
  <td><div class="tabledata" id = "switch"></div></td>

  the XML code will update the data where id = "switch"
  java script then uses getElementById
  document.getElementById("switch").innerHTML="Switch is OFF";


  .. now you can have the class define the look AND the class update the content, but you will then need
  a class for every data field that must be updated, here's what that will look like
  <td><div class="switch"></div></td>

  the XML code will update the data where class = "switch"
  java script then uses getElementsByClassName
  document.getElementsByClassName("switch")[0].style.color=text_color;


  the main general sections of a web page are the following and used here

  <html>
    <style>
    // dump CSS style stuff in here
    </style>
    <body>
      <header>
      // put header code for cute banners here
      </header>
      <main>
      // the buld of your web page contents
      </main>
      <footer>
      // put cute footer (c) 2021 xyz inc type thing
      </footer>
    </body>
    <script>
    // you java code between these tags
    </script>
  </html>


*/







const char PAGE_MAIN[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nixie Clock</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#0d1117;--card:#161b22;--bd:#30363d;--ac:#58a6ff;--ah:#388bfd;--tx:#c9d1d9;--mu:#8b949e;--ok:#3fb950;--er:#f85149;--r:10px;--f:"Segoe UI",system-ui,sans-serif}
body{background:var(--bg);color:var(--tx);font-family:var(--f);min-height:100vh}
header{background:var(--card);border-bottom:1px solid var(--bd);padding:12px 20px;display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:100}
.logo{font-size:1.15rem;font-weight:700;color:var(--ac);letter-spacing:2px}
.hi{font-size:.75rem;color:var(--mu);text-align:right;line-height:1.6}
.tabs{display:flex;background:var(--card);border-bottom:1px solid var(--bd);padding:0 16px;overflow-x:auto}
.tb{background:none;border:none;color:var(--mu);padding:10px 16px;font-size:.88rem;font-family:var(--f);cursor:pointer;border-bottom:2px solid transparent;transition:.2s;white-space:nowrap}
.tb.on{color:var(--ac);border-bottom-color:var(--ac)}
.tb:hover:not(.on){color:var(--tx)}
.wrap{padding:16px;max-width:820px;margin:0 auto}
.pn{display:none}.pn.on{display:block}
.card{background:var(--card);border:1px solid var(--bd);border-radius:var(--r);padding:16px;margin-bottom:12px}
.ct{font-size:.78rem;font-weight:600;color:var(--mu);text-transform:uppercase;letter-spacing:1px;margin-bottom:12px;padding-bottom:7px;border-bottom:1px solid var(--bd)}
.sg{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px;margin-bottom:12px}
.st{background:var(--bg);border:1px solid var(--bd);border-radius:8px;padding:12px;text-align:center}
.sv{font-size:1.4rem;font-weight:700;color:var(--ac)}
.sl{font-size:.7rem;color:var(--mu);margin-top:3px}
.fr{display:flex;align-items:center;gap:9px;margin-bottom:10px;flex-wrap:wrap}
.fl{min-width:115px;font-size:.85rem;color:var(--mu);flex-shrink:0}
input[type=text],input[type=number],select{background:var(--bg);border:1px solid var(--bd);border-radius:6px;color:var(--tx);padding:6px 10px;font-size:.85rem;font-family:var(--f);flex:1;min-width:80px}
input:focus,select:focus{outline:none;border-color:var(--ac)}
input[type=range]{-webkit-appearance:none;flex:1;height:5px;background:var(--bd);border-radius:3px;outline:none;min-width:80px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;background:var(--ac);border-radius:50%;cursor:pointer}
input[type=color]{width:44px;height:32px;border:1px solid var(--bd);border-radius:6px;background:var(--bg);cursor:pointer;padding:2px;flex-shrink:0}
.vd{min-width:44px;font-size:.85rem;color:var(--ac);text-align:right;flex-shrink:0}
.btn{background:var(--ac);color:#0d1117;border:none;border-radius:6px;padding:7px 16px;font-size:.85rem;font-weight:600;font-family:var(--f);cursor:pointer;transition:.2s}
.btn:hover{background:var(--ah)}
.mg{display:flex;gap:6px;flex-wrap:wrap}
.mb{background:var(--bg);border:1px solid var(--bd);color:var(--mu);border-radius:6px;padding:6px 12px;font-size:.82rem;font-family:var(--f);cursor:pointer;transition:.2s}
.mb.on{background:var(--ac);color:#0d1117;border-color:var(--ac);font-weight:600}
.mb:hover:not(.on){border-color:var(--ac);color:var(--ac)}
.ir{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid var(--bd);font-size:.85rem}
.ir:last-child{border-bottom:none}
.ik{color:var(--mu)}.iv{color:var(--tx)}
.bk{display:inline-block;padding:2px 8px;border-radius:12px;font-size:.75rem;font-weight:600}
.ok{background:#1a3a2a;color:var(--ok)}.er{background:#3a1a1a;color:var(--er)}
#toast{position:fixed;bottom:16px;right:16px;background:var(--card);border:1px solid var(--bd);border-radius:8px;padding:9px 16px;font-size:.85rem;opacity:0;transition:opacity .3s;pointer-events:none;z-index:999}
#toast.on{opacity:1}
.tgl{position:relative;display:inline-block;width:42px;height:22px;flex-shrink:0}
.tgl input{opacity:0;width:0;height:0}
.tgl-sl{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:var(--bd);border-radius:22px;transition:.3s}
.tgl-sl:before{position:absolute;content:"";height:16px;width:16px;left:3px;bottom:3px;background:white;border-radius:50%;transition:.3s}
input:checked+.tgl-sl{background:var(--ac)}
input:checked+.tgl-sl:before{transform:translateX(20px)}
</style>
</head>
<body>
<header>
<div class="logo">&#9203; NIXIE CLOCK</div>
<div class="hi"><span id="hd-ip">-</span><br><span id="hd-up">-</span></div>
</header>
<div class="tabs">
<button class="tb on" onclick="tab('sys',this)">System</button>
<button class="tb" onclick="tab('rgb',this)">NeoPixel</button>
<button class="tb" onclick="tab('dsp',this)">Display</button>
<button class="tb" onclick="tab('sdc',this)">SD-Karte</button>
</div>
<div class="wrap">
<div id="sys" class="pn on">
<div class="sg">
<div class="st"><div class="sv" id="s-hp">-</div><div class="sl">Freier Heap</div></div>
<div class="st"><div class="sv" id="s-rs">-</div><div class="sl">WiFi RSSI</div></div>
<div class="st"><div class="sv" id="s-up">-</div><div class="sl">Uptime</div></div>
<div class="st"><div class="sv" id="s-sd">-</div><div class="sl">SD-Karte</div></div>
</div>
<div class="card">
<div class="ct">Firmware &amp; System</div>
<div class="ir"><span class="ik">Version</span><span class="iv" id="fi-vr">-</span></div>
<div class="ir"><span class="ik">Build</span><span class="iv" id="fi-bd">-</span></div>
<div class="ir"><span class="ik">IP-Adresse</span><span class="iv" id="fi-ip">-</span></div>
<div class="ir"><span class="ik">CPU-Takt</span><span class="iv" id="fi-cp">-</span></div>
<div class="ir"><span class="ik">Chip</span><span class="iv" id="fi-ch">-</span></div>
</div>
<div class="card">
<div class="ct">NTP-Konfiguration</div>
<div class="fr"><span class="fl">NTP Server</span><input type="text" id="ntp-s" placeholder="de.pool.ntp.org" maxlength="63"></div>
<div class="fr"><span class="fl">UTC-Offset</span>
<select id="ntp-u">
<option value="-12">UTC-12</option><option value="-11">UTC-11</option><option value="-10">UTC-10</option>
<option value="-9">UTC-9</option><option value="-8">UTC-8</option><option value="-7">UTC-7</option>
<option value="-6">UTC-6</option><option value="-5">UTC-5</option><option value="-4">UTC-4</option>
<option value="-3">UTC-3</option><option value="-2">UTC-2</option><option value="-1">UTC-1</option>
<option value="0">UTC+0</option><option value="1" selected>UTC+1 (DE)</option>
<option value="2">UTC+2</option><option value="3">UTC+3</option><option value="4">UTC+4</option>
<option value="5">UTC+5</option><option value="6">UTC+6</option><option value="7">UTC+7</option>
<option value="8">UTC+8</option><option value="9">UTC+9</option><option value="10">UTC+10</option>
<option value="11">UTC+11</option><option value="12">UTC+12</option>
</select>
</div>
<div class="fr"><span class="fl"></span><button class="btn" onclick="saveNTP()">Speichern</button></div>
</div>
<div class="card">
<div class="ct">Wetter</div>
<div class="fr">
<span class="fl">Wetter anzeigen</span>
<label class="tgl"><input type="checkbox" id="w-en" checked><span class="tgl-sl"></span></label>
</div>
<div class="fr">
<span class="fl">Ort</span>
<input type="text" id="w-city" value="Berlin" style="width:120px;padding:4px 8px;border-radius:6px;border:1px solid var(--bd);background:var(--bg);color:var(--fg)">
</div>
<div class="fr">
<span class="fl">Breitengrad</span>
<input type="number" id="w-lat" value="52.52" step="0.0001" style="width:100px;padding:4px 8px;border-radius:6px;border:1px solid var(--bd);background:var(--bg);color:var(--fg)">
</div>
<div class="fr">
<span class="fl">Laengengrad</span>
<input type="number" id="w-lon" value="13.405" step="0.0001" style="width:100px;padding:4px 8px;border-radius:6px;border:1px solid var(--bd);background:var(--bg);color:var(--fg)">
</div>
<div class="fr"><span class="fl"></span><button class="btn" onclick="saveWeather()">Speichern</button></div>
<div style="margin-top:8px;font-size:.75rem;color:#888">Datenquelle: <a href="https://open-meteo.com" target="_blank" style="color:#6af">Open-Meteo.com</a> (kostenlos, kein API-Key)</div>
</div>
</div>
<div id="rgb" class="pn">
<div class="card">
<div class="ct">Modus</div>
<div class="mg">
<button class="mb on" onclick="setMode(0,this)">Aus</button>
<button class="mb" onclick="setMode(1,this)">Statisch</button>
<button class="mb" onclick="setMode(2,this)">Regenbogen</button>
<button class="mb" onclick="setMode(3,this)">Atmen</button>
<button class="mb" onclick="setMode(4,this)">Farbwechsel</button>
</div>
</div>
<div class="card">
<div class="ct">Farbe &amp; Helligkeit</div>
<div class="fr">
<span class="fl">Farbe</span>
<input type="color" id="rgb-c" value="#ff6600" oninput="document.getElementById('rgb-p').style.background=this.value">
<div id="rgb-p" style="width:36px;height:32px;border-radius:6px;border:1px solid var(--bd);background:#ff6600;flex-shrink:0"></div>
</div>
<div class="fr">
<span class="fl">Helligkeit</span>
<input type="range" id="rgb-b" min="0" max="255" value="128" oninput="document.getElementById('rgb-bv').textContent=this.value">
<span class="vd" id="rgb-bv">128</span>
</div>
<div class="fr"><span class="fl"></span><button class="btn" onclick="sendRGB()">Ubernehmen</button></div>
</div>
</div>
<div id="dsp" class="pn">
<div class="card">
<div class="ct">Uhrzeit</div>
<div style="display:flex;gap:20px;align-items:flex-start;flex-wrap:wrap">
<div style="flex:1;min-width:200px">
<div class="fr">
<span class="fl">Ziffernfarbe</span>
<input type="color" id="cl-fg" value="#f800f8" oninput="updSeg()">
</div>
<div class="fr">
<span class="fl">Segmenthintergrund</span>
<input type="color" id="cl-bg" value="#181818" oninput="updSeg()">
</div>
<div class="fr"><span class="fl"></span><button class="btn" onclick="saveClk()">Ubernehmen</button></div>
</div>
<div style="flex-shrink:0;text-align:center">
<svg viewBox="0 0 60 100" width="60" height="100">
<rect width="60" height="100" rx="6" id="seg-bg" fill="#181818"/>
<rect x="12" y="5" width="36" height="6" rx="2" class="seg-fg" fill="#f800f8"/>
<rect x="12" y="47" width="36" height="6" rx="2" class="seg-fg" fill="#f800f8"/>
<rect x="12" y="89" width="36" height="6" rx="2" class="seg-fg" fill="#f800f8"/>
<rect x="5" y="12" width="6" height="34" rx="2" class="seg-fg" fill="#f800f8"/>
<rect x="49" y="12" width="6" height="34" rx="2" class="seg-fg" fill="#f800f8"/>
<rect x="5" y="54" width="6" height="34" rx="2" class="seg-fg" fill="#f800f8"/>
<rect x="49" y="54" width="6" height="34" rx="2" class="seg-fg" fill="#f800f8"/>
</svg>
<div style="font-size:.7rem;color:var(--mu);margin-top:4px">Vorschau</div>
</div>
</div>
</div>
<div class="card">
<div class="ct">Kalender</div>
<div class="fr">
<span class="fl">Kalender anzeigen</span>
<label class="tgl"><input type="checkbox" id="dp-cal" checked><span class="tgl-sl"></span></label>
</div>
<div class="fr">
<span class="fl">Hintergrundbild</span>
<label class="tgl"><input type="checkbox" id="dp-bg"><span class="tgl-sl"></span></label>
</div>
</div>
<div class="card">
<div class="ct">Display</div>
<div class="fr">
<span class="fl">Helligkeit</span>
<input type="range" id="dp-b" min="0" max="4095" value="4095" oninput="document.getElementById('dp-bv').textContent=Math.round(this.value/40.95)+'%'">
<span class="vd" id="dp-bv">100%</span>
</div>
<div class="fr">
<span class="fl">Bilderwechsel</span>
<label class="tgl"><input type="checkbox" id="dp-slide"><span class="tgl-sl"></span></label>
</div>
<div class="fr">
<span class="fl">Dauer je Bild</span>
<input type="range" id="dp-i" min="1" max="60" value="5" oninput="document.getElementById('dp-iv').textContent=this.value+' s'">
<span class="vd" id="dp-iv">5 s</span>
</div>
<div class="fr"><span class="fl"></span><button class="btn" onclick="saveDisp()">Speichern</button></div>
</div>
</div>
<div id="sdc" class="pn">
<div class="card">
<div class="ct">SD-Karte Dateibrowser</div>
<div style="display:flex;gap:12px;flex-wrap:wrap">
<div id="sd-tree" style="flex:1;min-width:200px;max-height:400px;overflow:auto;font-size:.85rem;border:1px solid var(--bd);border-radius:8px;padding:8px;background:var(--bg)"></div>
<div id="sd-preview" style="flex:1;min-width:200px;text-align:center;border:1px solid var(--bd);border-radius:8px;padding:8px;background:var(--bg)">
<div style="color:var(--mu);padding:40px 0">Datei auswaehlen</div>
</div>
</div>
</div>
</div>
<div id="toast"></div>
<script>
var cMode=0,rgbInitDone=false,dispInitDone=false,clockInitDone=false,weatherInitDone=false;
function tab(id,b){document.querySelectorAll('.pn').forEach(p=>p.classList.remove('on'));document.querySelectorAll('.tb').forEach(x=>x.classList.remove('on'));document.getElementById(id).classList.add('on');b.classList.add('on');}
function toast(m,ok){var t=document.getElementById('toast');t.textContent=m;t.style.borderColor=ok?'var(--ok)':'var(--er)';t.classList.add('on');setTimeout(()=>t.classList.remove('on'),2500);}
function gx(x,t){var m=x.match('<'+t+'>([\\s\\S]*?)<\\/'+t+'>');return m?m[1]:'';}
function fup(s){s=parseInt(s)||0;return Math.floor(s/3600)+'h '+Math.floor(s%3600/60)+'m '+s%60+'s';}
function poll(){
fetch('/xml').then(r=>r.text()).then(function(x){
var h=parseInt(gx(x,'HEAP'));document.getElementById('s-hp').textContent=h>0?Math.round(h/1024)+'KB':'?';
document.getElementById('s-rs').textContent=gx(x,'RSSI')+'dBm';
var up=fup(gx(x,'UPTIME'));document.getElementById('s-up').textContent=up;document.getElementById('hd-up').textContent=up;
var sd=gx(x,'SD');document.getElementById('s-sd').innerHTML=sd=='1'?'<span class="bk ok">OK</span>':'<span class="bk er">Fehlt</span>';
var ip=gx(x,'IP');document.getElementById('hd-ip').textContent=ip;document.getElementById('fi-ip').textContent=ip;
document.getElementById('fi-vr').textContent=gx(x,'VER');
document.getElementById('fi-bd').textContent=gx(x,'BUILD');
document.getElementById('fi-cp').textContent=gx(x,'CPU')+' MHz';
document.getElementById('fi-ch').textContent=gx(x,'CHIP');
var ns=gx(x,'NTP');if(ns&&document.getElementById('ntp-s').value==='')document.getElementById('ntp-s').value=ns;
var utc=gx(x,'UTC');if(utc!='')document.getElementById('ntp-u').value=utc;
if(!rgbInitDone){cMode=parseInt(gx(x,'RGBM'))||0;document.querySelectorAll('.mb').forEach(function(b,i){b.classList.toggle('on',i===cMode);});var rr=parseInt(gx(x,'RGBR'))||0,rg=parseInt(gx(x,'RGBG'))||0,rb=parseInt(gx(x,'RGBB'))||0;var rhex='#'+('0'+rr.toString(16)).slice(-2)+('0'+rg.toString(16)).slice(-2)+('0'+rb.toString(16)).slice(-2);document.getElementById('rgb-c').value=rhex;document.getElementById('rgb-p').style.background=rhex;var rbr=parseInt(gx(x,'RGBBR'))||128;document.getElementById('rgb-b').value=rbr;document.getElementById('rgb-bv').textContent=rbr;rgbInitDone=true;}
if(!dispInitDone){var di=parseInt(gx(x,'DPINT'))||5;document.getElementById('dp-i').value=di;document.getElementById('dp-iv').textContent=di+' s';var db=parseInt(gx(x,'DPBR'))||4095;document.getElementById('dp-b').value=db;document.getElementById('dp-bv').textContent=Math.round(db/40.95)+'%';document.getElementById('dp-slide').checked=gx(x,'SLIDE')==='1';document.getElementById('dp-cal').checked=gx(x,'CAL')!=='0';document.getElementById('dp-bg').checked=gx(x,'BG')==='1';dispInitDone=true;}
if(!clockInitDone){var cfr=parseInt(gx(x,'CLFR'))||248,cfg2=parseInt(gx(x,'CLFG'))||0,cfb=parseInt(gx(x,'CLFB'))||248;var cbr=parseInt(gx(x,'CLBR'))||24,cbg=parseInt(gx(x,'CLBG'))||24,cbb=parseInt(gx(x,'CLBB'))||24;document.getElementById('cl-fg').value='#'+('0'+cfr.toString(16)).slice(-2)+('0'+cfg2.toString(16)).slice(-2)+('0'+cfb.toString(16)).slice(-2);document.getElementById('cl-bg').value='#'+('0'+cbr.toString(16)).slice(-2)+('0'+cbg.toString(16)).slice(-2)+('0'+cbb.toString(16)).slice(-2);updSeg();clockInitDone=true;}
if(!weatherInitDone){document.getElementById('w-en').checked=gx(x,'WEN')!=='0';document.getElementById('w-city').value=gx(x,'WCITY')||'Berlin';var wla=gx(x,'WLAT');if(wla)document.getElementById('w-lat').value=wla;var wlo=gx(x,'WLON');if(wlo)document.getElementById('w-lon').value=wlo;weatherInitDone=true;}
}).catch(function(){});
}
setInterval(poll,2000);poll();
function saveNTP(){var s=document.getElementById('ntp-s').value.trim();var u=document.getElementById('ntp-u').value;if(!s){toast('NTP Server eingeben!',false);return;}fetch('/SET_NTP?SERVER='+encodeURIComponent(s)+'&UTC='+u).then(function(r){toast(r.ok?'NTP gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function setMode(m,b){cMode=m;document.querySelectorAll('.mb').forEach(function(x){x.classList.remove('on');});b.classList.add('on');}
function sendRGB(){var h=document.getElementById('rgb-c').value;var r=parseInt(h.slice(1,3),16),g=parseInt(h.slice(3,5),16),b=parseInt(h.slice(5,7),16);var br=document.getElementById('rgb-b').value;fetch('/SET_RGB?MODE='+cMode+'&R='+r+'&G='+g+'&B='+b+'&BRIGHT='+br).then(function(r){toast(r.ok?'NeoPixel aktualisiert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function saveDisp(){var i=document.getElementById('dp-i').value;var b=document.getElementById('dp-b').value;var sl=document.getElementById('dp-slide').checked?1:0;var ca=document.getElementById('dp-cal').checked?1:0;var bg=document.getElementById('dp-bg').checked?1:0;fetch('/SET_DISPLAY?INTERVAL='+i+'&BRIGHT='+b+'&SLIDE='+sl+'&CAL='+ca+'&BG='+bg).then(function(r){toast(r.ok?'Display gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function updSeg(){var fg=document.getElementById('cl-fg').value;var bg=document.getElementById('cl-bg').value;document.getElementById('seg-bg').setAttribute('fill',bg);document.querySelectorAll('.seg-fg').forEach(function(s){s.setAttribute('fill',fg);});}
function saveClk(){var fh=document.getElementById('cl-fg').value;var bh=document.getElementById('cl-bg').value;var fr=parseInt(fh.slice(1,3),16),fg=parseInt(fh.slice(3,5),16),fb=parseInt(fh.slice(5,7),16);var br=parseInt(bh.slice(1,3),16),bg=parseInt(bh.slice(3,5),16),bb=parseInt(bh.slice(5,7),16);fetch('/SET_DISPLAY?CFR='+fr+'&CFG='+fg+'&CFB='+fb+'&CBR='+br+'&CBG='+bg+'&CBB='+bb).then(function(r){toast(r.ok?'Farben gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function saveWeather(){var en=document.getElementById('w-en').checked?1:0;var city=document.getElementById('w-city').value.trim();var lat=document.getElementById('w-lat').value;var lon=document.getElementById('w-lon').value;if(!city){toast('Ort eingeben!',false);return;}fetch('/SET_WEATHER?EN='+en+'&CITY='+encodeURIComponent(city)+'&LAT='+lat+'&LON='+lon).then(function(r){toast(r.ok?'Wetter gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function sdLoad(p){fetch('/SD_LIST?path='+encodeURIComponent(p)).then(r=>r.json()).then(function(d){var h='';d.sort(function(a,b){if(a.dir&&!b.dir)return -1;if(!a.dir&&b.dir)return 1;return a.name.localeCompare(b.name);});if(p!=='/'){h+='<div style="cursor:pointer;padding:2px 0" onclick="sdLoad(\''+p.replace(/\/[^/]*\/?$/,'/')+'\')">&#128281; ..</div>';}d.forEach(function(e){if(e.dir){h+='<div style="cursor:pointer;padding:2px 0" onclick="sdLoad(\''+e.path+'\')">&#128193; '+e.name+'</div>';}else{h+='<div style="cursor:pointer;padding:2px 0" onclick="sdFile(\''+e.path+'\',\''+e.name+'\')">&#128196; '+e.name+' <small style=\"color:var(--mu)\">('+sdSize(e.size)+')</small></div>';}});document.getElementById('sd-tree').innerHTML=h;}).catch(function(){document.getElementById('sd-tree').innerHTML='<div style=\"color:var(--er)\">Fehler beim Laden</div>';});}
function sdSize(b){if(b<1024)return b+'B';if(b<1048576)return(b/1024).toFixed(1)+'KB';return(b/1048576).toFixed(1)+'MB';}
function sdFile(p,n){var pv=document.getElementById('sd-preview');var ext=n.toLowerCase().split('.').pop();if(['jpg','jpeg','png','bmp','gif'].indexOf(ext)>=0){pv.innerHTML='<div style=\"margin-bottom:8px;font-weight:600\">'+n+'</div><img src=\"/SD_FILE?path='+encodeURIComponent(p)+'\" style=\"max-width:100%;max-height:300px;border-radius:6px\">';}else{pv.innerHTML='<div style=\"margin-bottom:8px;font-weight:600\">'+n+'</div><div style=\"color:var(--mu);padding:20px\">Keine Vorschau verfuegbar</div>';}}
sdLoad('/');
</script>
</body>
</html>
)=====";