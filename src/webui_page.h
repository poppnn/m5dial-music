// Page de la Web UI (servie telle quelle par webui.cpp). Image de démarrage : même format "M5BA" que le web flasher
// (src/bootanim.h), fabriqué ici dans le navigateur du téléphone (JPEG 240 x 240, GIF animé si le navigateur sait le
// décoder, sinon sa première image).
#pragma once
#include <pgmspace.h>

static const char WEBUI_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>M5Dial</title>
<style>
:root{--bg:#111;--card:#1c1c1e;--fg:#f2f2f2;--mute:#9a9a9f;--acc:#ff8c1a;--line:#333}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,-apple-system,sans-serif}
main{max-width:560px;margin:0 auto;padding:16px}h1{font-size:22px;margin:4px 0 2px}h2{font-size:17px;margin:0 0 10px}
section{background:var(--card);border-radius:14px;padding:14px 16px;margin:14px 0}
label{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:7px 0;border-bottom:1px solid var(--line)}
label:last-child{border-bottom:0}select,input{font:inherit;color:var(--fg);background:#2c2c2e;border:1px solid #444;border-radius:8px;padding:7px 9px;min-height:40px;max-width:60%}
input[type=range]{padding:0;background:none;border:0;width:55%}button{font:inherit;font-weight:600;border:0;border-radius:10px;padding:10px 14px;min-height:44px;background:var(--acc);color:#111;cursor:pointer}
button.ghost{background:#2c2c2e;color:var(--fg)}.row{display:flex;flex-wrap:wrap;gap:10px;margin-top:10px}
.muted{color:var(--mute);font-size:14px}#msg{min-height:22px}.dev{display:flex;justify-content:space-between;align-items:center;padding:6px 0}
canvas{width:120px;height:120px;border-radius:50%;background:#000}progress{width:100%}
</style></head><body><main>
<h1>M5Dial</h1><p class="muted" id="fw"></p>
<p id="msg" role="status" aria-live="polite"></p>
<section><h2>Settings</h2><div id="set"></div><div class="row"><button id="save">Save</button></div></section>
<section><h2>LEDs</h2><div id="led"></div><div class="row"><button id="save2">Save</button></div></section>
<section><h2>Wi-Fi (hotspot for updates)</h2>
<label>Name<input id="ssid" maxlength="32" autocomplete="off"></label>
<label>Password<input id="pass" type="password" maxlength="63" placeholder="unchanged" autocomplete="off"></label>
<div class="row"><button id="save3">Save</button></div></section>
<section><h2>Paired devices</h2><div id="devs"></div><p class="muted">Forgotten when the M5Dial restarts (after "Done").</p></section>
<section><h2>Boot image or GIF</h2><input type="file" id="img" accept="image/*" aria-label="Boot image or GIF">
<div class="row"><canvas id="prev" width="240" height="240"></canvas><p class="muted" id="info">Square crop, 240 × 240.</p></div>
<progress id="prog" max="1" value="0" hidden></progress>
<div class="row"><button id="send" disabled>Send</button><button class="ghost" id="clear">Remove</button></div></section>
<section><h2>Config file</h2><div class="row"><button class="ghost" id="exp">Export</button>
<label style="border:0;padding:0"><input type="file" id="imp" accept=".json,application/json" hidden><button class="ghost" id="impb">Import</button></label></div></section>
<div class="row"><button id="done">Done: restart the M5Dial</button></div>
</main><script>
const $=id=>document.getElementById(id);let cfg={},frames=[];
const msg=t=>$("msg").textContent=t;
const opts={imuDir:[[1,"normal"],[-1,"reversed"]],encSteps:[[1,"1 pulse"],[2,"2 pulses"],[3,"3 pulses"],[4,"4 pulses"]],
dimAfterS:[[0,"never"],[10,"10 s"],[20,"20 s"],[30,"30 s"],[60,"60 s"],[120,"120 s"]],sleepAfterMin:[[0,"never"],[5,"5 min"],[10,"10 min"],[20,"20 min"],[30,"30 min"],[60,"60 min"]],
briDay:[10,20,30,40,50,60,70,80,90,100].map(v=>[v,v+" %"]),briNight:[5,10,15,20,30,40,50].map(v=>[v,v+" %"]),
briMode:[[0,"day"],[1,"night"],[2,"auto"]],nightFrom:[...Array(24).keys()].map(h=>[h,h+":00"]),nightTo:[...Array(24).keys()].map(h=>[h,h+":00"]),
notifMode:[[0,"off"],[1,"sender"],[2,"full"]],haptic:[[1,"on"],[0,"off"]]};
const names={imuDir:"IMU direction",encSteps:"Wheel",dimAfterS:"Dim after",sleepAfterMin:"Power off",briDay:"Day level",briNight:"Night level",
briMode:"Brightness",nightFrom:"Night from",nightTo:"Night until",notifMode:"Notifications",accent:"Accent",haptic:"Click"};
function sel(key,list){const s=document.createElement("select");s.id="f_"+key;
 if(!list.some(o=>o[0]==cfg[key]))list=[[cfg[key],String(cfg[key])],...list];
 for(const [v,t] of list){const o=new Option(t,v);o.selected=v==cfg[key];s.add(o)}return s}
function row(parent,text,el){const l=document.createElement("label");l.append(text,el);parent.append(l)}
function render(){
 $("fw").textContent="Firmware "+cfg.firmware;const set=$("set");set.textContent="";
 opts.accent=cfg.accents.map((n,i)=>[i,n]);
 for(const k in names)row(set,names[k],sel(k,opts[k]));
 const led=$("led");led.textContent="";
 row(led,"Colour",sel("ledColor",cfg.ledColors.map((n,i)=>[i,n])));
 const c=document.createElement("input");c.type="checkbox";c.id="f_ledCustom";c.checked=!!cfg.ledCustom;c.style.minHeight="auto";row(led,"Use hue instead",c);
 for(const [k,t,lo,hi] of [["ledHue","Hue",0,359],["ledBri","Brightness",5,100]]){const r=document.createElement("input");r.type="range";r.min=lo;r.max=hi;r.value=cfg[k];r.id="f_"+k;r.setAttribute("aria-label",t);row(led,t,r)}
 $("ssid").value=cfg.wifiSsid;const d=$("devs");d.textContent="";
 if(!cfg.devices.length)d.innerHTML='<p class="muted">None.</p>';
 for(const dev of cfg.devices){const r=document.createElement("div");r.className="dev";const b=document.createElement("button");b.className="ghost";b.textContent="Forget";
  b.onclick=()=>post("/forget?id="+encodeURIComponent(dev.id)).then(()=>msg("Will be forgotten at restart: "+(dev.name||dev.id)));
  r.append(dev.name||dev.id.split("/")[0],b);d.append(r)}
}
async function post(url,body,type){const r=await fetch(url,{method:"POST",body,headers:type?{"Content-Type":type}:{}});const j=await r.json();
 if(!r.ok)throw new Error(j.error||r.status);if(j.firmware){cfg=j;render()}return j}
function gather(){const o={};for(const k in names)o[k]=+$("f_"+k).value;o.ledColor=+$("f_ledColor").value;o.ledCustom=$("f_ledCustom").checked?1:0;
 o.ledHue=+$("f_ledHue").value;o.ledBri=+$("f_ledBri").value;return o}
async function save(extra){try{await post("/config",JSON.stringify({...gather(),...extra}),"application/json");msg("Saved.")}catch(e){msg("Refused: "+e.message)}}
$("save").onclick=$("save2").onclick=()=>save({});
$("save3").onclick=()=>{const w={wifiSsid:$("ssid").value};if($("pass").value)w.wifiPass=$("pass").value;save(w).then(()=>$("pass").value="")};
$("exp").onclick=()=>{const c={...cfg};for(const k of["accents","ledColors","devices","bootCapacity"])delete c[k];
 const a=document.createElement("a");a.href=URL.createObjectURL(new Blob([JSON.stringify(c,null,1)],{type:"application/json"}));a.download="m5dial-config.json";a.click()};
$("impb").onclick=()=>$("imp").click();
$("imp").onchange=async e=>{const f=e.target.files[0];if(!f)return;try{const c=JSON.parse(await f.text());delete c.firmware;
 await post("/config",JSON.stringify(c),"application/json");msg("Imported.")}catch(err){msg("Import refused: "+err.message)}};
// Image : recadrage carré 240 x 240, JPEG ; GIF animé image par image si le navigateur a ImageDecoder
$("img").onchange=async e=>{const f=e.target.files[0];if(!f)return;frames=[];$("send").disabled=true;msg("Reading the picture...");
 try{const cv=document.createElement("canvas");cv.width=cv.height=240;const g=cv.getContext("2d");
  const draw=(src,w,h)=>{g.fillStyle="#000";g.fillRect(0,0,240,240);const k=Math.max(240/w,240/h);g.drawImage(src,(240-w*k)/2,(240-h*k)/2,w*k,h*k)};
  const jpg=()=>new Promise(ok=>cv.toBlob(b=>b.arrayBuffer().then(a=>ok(new Uint8Array(a))),"image/jpeg",0.85));
  if("ImageDecoder" in window&&await ImageDecoder.isTypeSupported(f.type)){const dec=new ImageDecoder({data:f.stream(),type:f.type});await dec.completed;
   const n=Math.min(dec.tracks.selectedTrack.frameCount,300);for(let i=0;i<n;i++){const {image}=await dec.decode({frameIndex:i});draw(image,image.displayWidth,image.displayHeight);
   const ms=Math.round((image.duration||100000)/1000);image.close();frames.push({jpg:await jpg(),ms:Math.min(Math.max(ms||100,20),10000)})}dec.close()}
  else{const bmp=await createImageBitmap(f);draw(bmp,bmp.width,bmp.height);frames.push({jpg:await jpg(),ms:1000})}
  $("prev").getContext("2d").drawImage(cv,0,0);const kb=Math.round(pack().length/1024);
  $("info").textContent=frames.length+" frame"+(frames.length>1?"s":"")+", "+kb+" KB";$("send").disabled=false;msg("")}
 catch(err){msg("This picture could not be read: "+err.message)}};
function pack(){const head=12+frames.length*12,total=head+frames.reduce((s,f)=>s+f.jpg.length,0),out=new Uint8Array(total),dv=new DataView(out.buffer);
 out.set([77,53,66,65],0);dv.setUint16(4,frames.length,true);dv.setUint32(8,total,true);let off=head;
 frames.forEach((f,i)=>{dv.setUint32(12+i*12,off,true);dv.setUint32(16+i*12,f.jpg.length,true);dv.setUint16(20+i*12,f.ms,true);out.set(f.jpg,off);off+=f.jpg.length});return out}
$("send").onclick=()=>{const data=pack();if(data.length>cfg.bootCapacity){msg("Too big: "+Math.round(data.length/1024)+" KB for "+Math.round(cfg.bootCapacity/1024)+" KB.");return}
 const fd=new FormData();fd.append("f",new Blob([data]),"boot.m5ba");const x=new XMLHttpRequest();$("prog").hidden=false;$("send").disabled=true;
 x.upload.onprogress=e=>$("prog").value=e.loaded/e.total;x.onloadend=()=>{$("prog").hidden=true;$("send").disabled=false;
  msg(x.status==200?"Boot image saved.":"The M5Dial refused the image.")};x.open("POST","/boot?n="+data.length);x.send(fd)};
$("clear").onclick=()=>post("/boot/clear").then(()=>msg("Boot image removed."));
$("done").onclick=()=>post("/exit").finally(()=>msg("The M5Dial restarts: Bluetooth comes back in a few seconds."));
fetch("/config").then(r=>r.json()).then(j=>{cfg=j;render()}).catch(()=>msg("Could not read the settings."));
</script></body></html>)HTML";
