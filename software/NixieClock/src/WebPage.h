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
.sd-layout{display:flex;gap:12px;min-height:420px}
.sd-tree-card{flex:0 0 240px;overflow-y:auto;max-height:65vh;display:flex;flex-direction:column}
.sd-prev-card{flex:1;overflow:auto;display:flex;flex-direction:column;min-height:200px}
#sd-tree{flex:1;overflow-y:auto}
#sd-prev{flex:1;display:flex;align-items:center;justify-content:center;flex-direction:column;min-height:120px}
#sd-prev img{max-width:100%;max-height:420px;object-fit:contain;border-radius:6px}
.sd-item{display:flex;align-items:center;gap:6px;padding:4px 6px;border-radius:4px;cursor:pointer;font-size:.82rem;user-select:none;transition:.15s}
.sd-item:hover{background:var(--bg)}
.sd-item.sel{background:var(--ac)!important;color:#0d1117}
.sd-dir{color:var(--ac)}
.sd-file{color:var(--tx)}
.sd-caret{display:inline-block;width:10px;font-size:.65rem;transition:transform .2s;flex-shrink:0}
.sd-caret.open{transform:rotate(90deg)}
.sd-children{margin-left:14px}
.sd-sz{margin-left:auto;color:var(--mu);font-size:.72rem;flex-shrink:0}
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
<button class="tb" onclick="tab('sdc',this);initSd()">SD-Karte</button>
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
<div class="ct">Anzeigedauer</div>
<div class="fr">
<span class="fl">Dauer je Bild</span>
<input type="range" id="dp-i" min="1" max="60" value="5" oninput="document.getElementById('dp-iv').textContent=this.value+' s'">
<span class="vd" id="dp-iv">5 s</span>
</div>
</div>
<div class="card">
<div class="ct">Anzeige-Optionen</div>
<div class="fr">
<span class="fl">Bilderwechsel</span>
<label class="tgl"><input type="checkbox" id="dp-slide"><span class="tgl-sl"></span></label>
</div>
<div class="fr">
<span class="fl">Kalender</span>
<label class="tgl"><input type="checkbox" id="dp-cal" checked><span class="tgl-sl"></span></label>
</div>
<div class="fr">
<span class="fl">Hintergrundbild</span>
<label class="tgl"><input type="checkbox" id="dp-bg"><span class="tgl-sl"></span></label>
</div>
</div>
<div class="card">
<div class="ct">Helligkeit Displays</div>
<div class="fr">
<span class="fl">Helligkeit</span>
<input type="range" id="dp-b" min="0" max="4095" value="4095" oninput="document.getElementById('dp-bv').textContent=Math.round(this.value/40.95)+'%'">
<span class="vd" id="dp-bv">100%</span>
</div>
<div class="fr"><span class="fl"></span><button class="btn" onclick="saveDisp()">Speichern</button></div>
</div>
<div class="card">
<div class="ct">Boot-Animation</div>
<div class="mg" id="anim-mg">
<button class="mb on" onclick="setAnim(0,this)">Keine</button>
<button class="mb" onclick="setAnim(1,this)">Matrix</button>
<button class="mb" onclick="setAnim(2,this)">Nixie-Slot</button>
<button class="mb" onclick="setAnim(3,this)">Farbwellen</button>
</div>
<div class="fr" style="margin-top:10px"><span class="fl" style="font-size:.75rem;color:var(--mu)">Gilt ab dem n&auml;chsten Neustart</span><button class="btn" onclick="saveAnim()">Speichern</button></div>
</div>
</div>
</div>
<div id="sdc" class="pn">
<div class="sd-layout">
<div class="card sd-tree-card">
<div class="ct" style="display:flex;align-items:center">SD-Karte<button class="btn" style="margin-left:auto;padding:2px 8px;font-size:.75rem" onclick="reloadSd()">&#8635;</button></div>
<div id="sd-tree"><span style="color:var(--mu);font-size:.82rem">Tab anklicken zum Laden...</span></div>
</div>
<div class="card sd-prev-card">
<div class="ct" id="sd-fname">Vorschau</div>
<div id="sd-prev"><span style="color:var(--mu);font-size:.85rem">Datei ausw&#228;hlen...</span></div>
</div>
</div>
</div>
<div id="toast"></div>
<script>
var cMode=0,rgbInitDone=false,dispInitDone=false,animInitDone=false,curAnim=0;
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
if(!rgbInitDone){cMode=parseInt(gx(x,'RGBM'))||0;document.querySelectorAll('.mb').forEach(function(b,i){b.classList.toggle('on',i===cMode);});rgbInitDone=true;}
if(!dispInitDone){var di=parseInt(gx(x,'DPINT'))||5;document.getElementById('dp-i').value=di;document.getElementById('dp-iv').textContent=di+' s';var db=parseInt(gx(x,'DPBR'))||4095;document.getElementById('dp-b').value=db;document.getElementById('dp-bv').textContent=Math.round(db/40.95)+'%';document.getElementById('dp-slide').checked=gx(x,'SLIDE')==='1';document.getElementById('dp-cal').checked=gx(x,'CAL')!=='0';document.getElementById('dp-bg').checked=gx(x,'BG')==='1';dispInitDone=true;}
if(!animInitDone){var am=parseInt(gx(x,'ANIMM'))||0;curAnim=am;document.querySelectorAll('#anim-mg .mb').forEach(function(b,i){b.classList.toggle('on',i===am);});animInitDone=true;}
}).catch(function(){});
}
setInterval(poll,2000);poll();
function saveNTP(){var s=document.getElementById('ntp-s').value.trim();var u=document.getElementById('ntp-u').value;if(!s){toast('NTP Server eingeben!',false);return;}fetch('/SET_NTP?SERVER='+encodeURIComponent(s)+'&UTC='+u).then(function(r){toast(r.ok?'NTP gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function setMode(m,b){cMode=m;document.querySelectorAll('.mb').forEach(function(x){x.classList.remove('on');});b.classList.add('on');}
function sendRGB(){var h=document.getElementById('rgb-c').value;var r=parseInt(h.slice(1,3),16),g=parseInt(h.slice(3,5),16),b=parseInt(h.slice(5,7),16);var br=document.getElementById('rgb-b').value;fetch('/SET_RGB?MODE='+cMode+'&R='+r+'&G='+g+'&B='+b+'&BRIGHT='+br).then(function(r){toast(r.ok?'NeoPixel aktualisiert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);});}
function saveDisp(){var i=document.getElementById('dp-i').value;var b=document.getElementById('dp-b').value;var sl=document.getElementById('dp-slide').checked?1:0;var ca=document.getElementById('dp-cal').checked?1:0;var bg=document.getElementById('dp-bg').checked?1:0;fetch('/SET_DISPLAY?INTERVAL='+i+'&BRIGHT='+b+'&SLIDE='+sl+'&CAL='+ca+'&BG='+bg).then(function(r){toast(r.ok?'Display gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);})}  
function setAnim(m,b){curAnim=m;document.querySelectorAll('#anim-mg .mb').forEach(function(x){x.classList.remove('on');});b.classList.add('on');}
function saveAnim(){fetch('/SET_ANIM?MODE='+curAnim).then(function(r){toast(r.ok?'Animation gespeichert':'Fehler',r.ok);}).catch(function(){toast('Fehler',false);})}
var sdLoaded=false;
function initSd(){if(!sdLoaded){sdLoaded=true;loadDir('/',document.getElementById('sd-tree'));}}
function reloadSd(){sdLoaded=false;loadDir('/',document.getElementById('sd-tree'));sdLoaded=true;}
function isImg(n){return /\.(jpg|jpeg|png|bmp|gif)$/i.test(n);}
function fmtSz(s){if(s<1024)return s+' B';if(s<1048576)return (s/1024).toFixed(1)+' KB';return (s/1048576).toFixed(1)+' MB';}
function loadDir(path,container){
container.innerHTML='<span style="color:var(--mu);font-size:.82rem">Lade...</span>';
fetch('/SD_LIST?path='+encodeURIComponent(path))
.then(function(r){return r.json();})
.then(function(items){
container.innerHTML='';
if(!items||items.length===0){container.innerHTML='<span style="color:var(--mu);font-size:.8rem;padding:4px 6px">Leer</span>';return;}
items.sort(function(a,b){if(a.type===b.type)return a.name.localeCompare(b.name);return a.type==='dir'?-1:1;});
items.forEach(function(item){
var fp=(path==='/'?'':path)+'/'+item.name;
var row=document.createElement('div');
row.className='sd-item '+(item.type==='dir'?'sd-dir':'sd-file');
if(item.type==='dir'){
var car=document.createElement('span');car.className='sd-caret';car.textContent='\u25B6';
var ic=document.createElement('span');ic.textContent='\uD83D\uDCC1';
var lb=document.createElement('span');lb.textContent=item.name;
row.appendChild(car);row.appendChild(ic);row.appendChild(lb);
var ch=document.createElement('div');ch.className='sd-children';ch.style.display='none';
var isOpen=false;
row.onclick=function(e){e.stopPropagation();isOpen=!isOpen;car.classList.toggle('open',isOpen);ch.style.display=isOpen?'block':'none';if(isOpen&&ch.innerHTML==='')loadDir(fp,ch);};
container.appendChild(row);container.appendChild(ch);
}else{
var ic=document.createElement('span');ic.textContent=isImg(item.name)?'\uD83D\uDDBC\uFE0F':'\uD83D\uDCC4';
var lb=document.createElement('span');lb.textContent=item.name;
var sz=document.createElement('span');sz.className='sd-sz';sz.textContent=fmtSz(item.size||0);
row.appendChild(ic);row.appendChild(lb);row.appendChild(sz);
row.onclick=function(){
document.querySelectorAll('.sd-item').forEach(function(x){x.classList.remove('sel');});
row.classList.add('sel');showFile(fp,item.name,item.size||0);};
container.appendChild(row);
}
});
})
.catch(function(){container.innerHTML='<span style="color:var(--er);font-size:.82rem;padding:4px 6px">Fehler beim Laden</span>';});}
function showFile(path,name,size){
document.getElementById('sd-fname').textContent=name;
var pv=document.getElementById('sd-prev');
pv.innerHTML='';
if(isImg(name)){
var img=document.createElement('img');
img.src='/SD_FILE?path='+encodeURIComponent(path);
img.alt=name;
img.style.maxWidth='100%';
img.style.maxHeight='420px';
img.style.objectFit='contain';
img.style.borderRadius='6px';
img.onerror=function(){pv.innerHTML='<span style="color:var(--er)">Vorschau nicht m\u00F6glich</span>';};
pv.appendChild(img);
}else{
pv.innerHTML='<div style="text-align:center"><div style="font-size:2.5rem">&#128196;</div><div style="color:var(--mu);font-size:.85rem;margin-top:8px">'+name+'</div><div style="color:var(--mu);font-size:.8rem;margin-top:4px">'+fmtSz(size)+'</div></div>';
}
}
</script>
</body>
</html>
)=====";