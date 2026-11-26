// Page de la Web UI (servie telle quelle par webui.cpp). Image de démarrage : même format "M5BA" que le web flasher
// (src/bootanim.h), fabriqué ici dans le navigateur du téléphone : JPEG 240 x 240, dans l'anneau (rond) ou plein
// écran ; GIF animé décodé par la page elle-même (Safari n'a pas ImageDecoder), qualité puis nombre d'images réduits
// jusqu'à tenir dans la partition. Même code que docs/index.html (bootFrames).
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
button.ghost{background:#2c2c2e;color:var(--fg)}button:disabled{opacity:.35;cursor:default}.row{display:flex;flex-wrap:wrap;gap:10px;margin-top:10px}
.muted{color:var(--mute);font-size:14px}#msg{min-height:22px}.dev{display:flex;justify-content:space-between;align-items:center;padding:6px 0}
canvas{width:120px;height:120px;border-radius:50%;background:#000}progress{width:100%}
</style></head><body><main>
<h1>M5Dial</h1><p class="muted" id="fw"></p>
<p id="msg" role="status" aria-live="polite"></p>
<section><h2>Settings</h2><div id="set"></div><div class="row"><button id="save">Save</button></div></section>
<section><h2>Page order</h2><p class="muted">Swipe up = next page. The first one opens after the boot screen.</p><div id="pages"></div><div class="row"><button id="save4">Save</button></div></section>
<section><h2>LEDs</h2><div id="led"></div><div class="row"><button id="save2">Save</button></div></section>
<section><h2>Wi-Fi (hotspot for updates)</h2>
<label>Name<input id="ssid" maxlength="32" autocomplete="off"></label>
<label>Password<input id="pass" type="password" maxlength="63" placeholder="unchanged" autocomplete="off"></label>
<div class="row"><button id="save3">Save</button></div></section>
<section><h2>Paired devices</h2><div id="devs"></div><p class="muted">Forgotten when the M5Dial restarts (after "Done").</p></section>
<section><h2>Boot image or GIF</h2><input type="file" id="img" accept="image/*,.gif" aria-label="Boot image or GIF">
<label>Size<select id="fit"><option value="1">Inside the ring</option><option value="0">Full screen</option></select></label>
<div class="row"><canvas id="prev" width="240" height="240"></canvas><p class="muted" id="info">Square crop. Animated GIF: up to 300 frames.</p></div>
<progress id="prog" max="1" value="0" hidden></progress>
<div class="row"><button id="send" disabled>Send</button><button class="ghost" id="clear">Remove</button></div></section>
<section><h2>Config file</h2><div class="row"><button class="ghost" id="exp">Export</button>
<label style="border:0;padding:0"><input type="file" id="imp" accept=".json,application/json" hidden><button class="ghost" id="impb">Import</button></label></div></section>
<div class="row"><button id="done">Done: restart the M5Dial</button></div>
</main><script>
const $=id=>document.getElementById(id);let cfg={},frames=[],order=[];
const msg=t=>$("msg").textContent=t;
const opts={encSteps:[[1,"1 pulse"],[2,"2 pulses"],[3,"3 pulses"],[4,"4 pulses"]],
dimAfterS:[[0,"never"],[10,"10 s"],[20,"20 s"],[30,"30 s"],[60,"60 s"],[120,"120 s"]],sleepAfterMin:[[0,"never"],[5,"5 min"],[10,"10 min"],[20,"20 min"],[30,"30 min"],[60,"60 min"]],
briDay:[10,20,30,40,50,60,70,80,90,100].map(v=>[v,v+" %"]),briNight:[5,10,15,20,30,40,50].map(v=>[v,v+" %"]),
briMode:[[0,"day"],[1,"night"],[2,"auto"]],nightFrom:[...Array(24).keys()].map(h=>[h,h+":00"]),nightTo:[...Array(24).keys()].map(h=>[h,h+":00"]),
notifMode:[[0,"off"],[1,"sender"],[2,"full"]],haptic:[[1,"on"],[0,"off"]]};
const names={encSteps:"Wheel",dimAfterS:"Dim after",sleepAfterMin:"Power off",briDay:"Day level",briNight:"Night level",
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
 order=[...cfg.pageOrder].map(Number);pages();
 $("ssid").value=cfg.wifiSsid;const d=$("devs");d.textContent="";
 if(!cfg.devices.length)d.innerHTML='<p class="muted">None.</p>';
 for(const dev of cfg.devices){const r=document.createElement("div");r.className="dev";const b=document.createElement("button");b.className="ghost";b.textContent="Forget";
  b.onclick=()=>post("/forget?id="+encodeURIComponent(dev.id)).then(()=>msg("Will be forgotten at restart: "+(dev.name||dev.id)));
  r.append(dev.name||dev.id.split("/")[0],b);d.append(r)}
}
function pages(){const p=$("pages");p.textContent="";order.forEach((n,i)=>{const r=document.createElement("div");r.className="dev";
 const b=document.createElement("span");b.className="row";b.style.margin="0";
 for(const [t,d,l] of [["↑",-1,"up"],["↓",1,"down"]]){const k=document.createElement("button");k.className="ghost";k.textContent=t;k.setAttribute("aria-label",cfg.pages[n]+" "+l);
  k.disabled=order[i+d]===undefined;k.onclick=()=>{[order[i],order[i+d]]=[order[i+d],order[i]];pages()};b.append(k)}
 r.append((i+1)+". "+cfg.pages[n],b);p.append(r)})}
async function post(url,body,type){const r=await fetch(url,{method:"POST",body,headers:type?{"Content-Type":type}:{}});const j=await r.json();
 if(!r.ok)throw new Error(j.error||r.status);if(j.firmware){cfg=j;render()}return j}
function gather(){const o={};for(const k in names)o[k]=+$("f_"+k).value;o.ledColor=+$("f_ledColor").value;o.ledCustom=$("f_ledCustom").checked?1:0;
 o.ledHue=+$("f_ledHue").value;o.ledBri=+$("f_ledBri").value;o.pageOrder=order.join("");return o}
async function save(extra){try{await post("/config",JSON.stringify({...gather(),...extra}),"application/json");msg("Saved.")}catch(e){msg("Refused: "+e.message)}}
$("save").onclick=$("save2").onclick=$("save4").onclick=()=>save({});
$("save3").onclick=()=>{const w={wifiSsid:$("ssid").value};if($("pass").value)w.wifiPass=$("pass").value;save(w).then(()=>$("pass").value="")};
$("exp").onclick=()=>{const c={...cfg};for(const k of["accents","ledColors","devices","bootCapacity","pages"])delete c[k];
 const a=document.createElement("a");a.href=URL.createObjectURL(new Blob([JSON.stringify(c,null,1)],{type:"application/json"}));a.download="m5dial-config.json";a.click()};
$("impb").onclick=()=>$("imp").click();
$("imp").onchange=async e=>{const f=e.target.files[0];if(!f)return;try{const c=JSON.parse(await f.text());delete c.firmware;
 await post("/config",JSON.stringify(c),"application/json");msg("Imported.")}catch(err){msg("Import refused: "+err.message)}};
// Image de démarrage (bootFrames : même code que docs/index.html)
// Image / GIF de démarrage -> images JPEG 240 x 240 (format M5BA, src/bootanim.h). round : recadrée en rond dans
// l'anneau de l'écran d'attente (sinon plein écran, l'anneau par-dessus). GIF décodé ici (Safari et Firefox n'ont
// pas toujours ImageDecoder) ; autres formats animés : ImageDecoder s'il existe, sinon la première image. Qualité JPEG
// puis nombre d'images réduits jusqu'à tenir dans cap octets.
function lzw(d,min,n){const out=new Uint8Array(n),pre=new Uint16Array(4096),suf=new Uint8Array(4096),st=new Uint8Array(4097),clr=1<<min;
 for(let i=0;i<clr;i++)suf[i]=i;let size=min+1,mask=(1<<size)-1,avail=clr+2,old=-1,first=0,bits=0,acc=0,di=0,op=0;
 while(op<n){while(bits<size){if(di>=d.length)return out;acc|=d[di++]<<bits;bits+=8}
  let c=acc&mask;acc>>>=size;bits-=size;
  if(c===clr){size=min+1;mask=(1<<size)-1;avail=clr+2;old=-1;continue}
  if(c===clr+1)break;
  if(old<0){if(c>clr)break;out[op++]=first=c;old=c;continue}
  const cur=c;let sp=0;if(c>avail)break;
  if(c===avail){st[sp++]=first;c=old}
  while(c>=clr){st[sp++]=suf[c];c=pre[c]}
  first=c;st[sp++]=c;
  if(avail<4096){pre[avail]=old;suf[avail]=first;avail++;if(!(avail&mask)&&avail<4096){size++;mask+=avail}}
  old=cur;while(sp&&op<n)out[op++]=st[--sp]}
 return out}
// Images complètes du GIF (pixels RGBA W x H, partagés : à utiliser avant l'image suivante), durée en ms.
function* gifFrames(v){const sig=String.fromCharCode(...v.subarray(0,6));if(sig!=="GIF87a"&&sig!=="GIF89a")throw new Error("not a GIF");
 const W=v[6]|v[7]<<8,H=v[8]|v[9]<<8;if(!W||!H||W*H>16e6)throw new Error("GIF size");
 let p=13,gct=null,ms=100,tr=-1,disp=0;if(v[10]&128){gct=v.subarray(p,p+(3<<((v[10]&7)+1)));p+=gct.length}
 const pix=new Uint8ClampedArray(W*H*4),skip=()=>{while(p<v.length&&v[p])p+=v[p]+1;p++};
 while(p<v.length){const b=v[p++];if(b===0x3B)break;
  if(b===0x21){if(v[p]===0xF9&&v[p+1]>=4){const k=v[p+2];disp=k>>2&7;ms=(v[p+3]|v[p+4]<<8)*10;tr=k&1?v[p+5]:-1}p++;skip();continue}
  if(b!==0x2C)break;
  const x=v[p]|v[p+1]<<8,y=v[p+2]|v[p+3]<<8,w=v[p+4]|v[p+5]<<8,h=v[p+6]|v[p+7]<<8,f=v[p+8];p+=9;
  let ct=gct;if(f&128){ct=v.subarray(p,p+(3<<((f&7)+1)));p+=ct.length}
  const min=v[p++];if(min<1||min>11)throw new Error("bad GIF");
  const parts=[];let len=0;while(p<v.length&&v[p]){parts.push(v.subarray(p+1,p+1+v[p]));len+=v[p];p+=v[p]+1}p++;
  const data=new Uint8Array(len);len=0;for(const a of parts){data.set(a,len);len+=a.length}
  const idx=lzw(data,min,w*h),save=disp===3?pix.slice():null,rows=[];
  if(f&64){for(const [s,t] of [[0,8],[4,8],[2,4],[1,2]])for(let r=s;r<h;r+=t)rows.push(r)}else for(let r=0;r<h;r++)rows.push(r);
  if(ct)for(let i=0;i<h;i++){const yy=y+rows[i];if(yy>=H)continue;
   for(let j=0;j<w;j++){const k=idx[i*w+j],xx=x+j;if(k===tr||xx>=W||k*3>=ct.length)continue;const q=(yy*W+xx)*4;
    pix[q]=ct[k*3];pix[q+1]=ct[k*3+1];pix[q+2]=ct[k*3+2];pix[q+3]=255}}
  yield {pix,w:W,h:H,ms:ms>10?ms:100};
  if(disp===2)for(let yy=y;yy<Math.min(y+h,H);yy++)pix.fill(0,(yy*W+x)*4,(yy*W+Math.min(x+w,W))*4);
  else if(save)pix.set(save);
  ms=100;tr=-1;disp=0}}
async function* imgFrames(file,buf){
 if(buf[0]===71&&buf[1]===73&&buf[2]===70){const c=document.createElement("canvas");let g=null;
  for(const f of gifFrames(buf)){if(!g){c.width=f.w;c.height=f.h;g=c.getContext("2d")}g.putImageData(new ImageData(f.pix,f.w,f.h),0,0);yield {img:c,w:f.w,h:f.h,ms:f.ms}}
  if(!g)throw new Error("empty GIF");return}
 if("ImageDecoder" in window&&await ImageDecoder.isTypeSupported(file.type)){const dec=new ImageDecoder({data:buf,type:file.type});
  try{await dec.completed;for(let i=0;i<dec.tracks.selectedTrack.frameCount;i++){const {image}=await dec.decode({frameIndex:i});
   try{yield {img:image,w:image.displayWidth,h:image.displayHeight,ms:Math.round((image.duration||100000)/1000)||100}}finally{image.close()}}}
  finally{dec.close()}return}
 const b=await createImageBitmap(file);yield {img:b,w:b.width,h:b.height,ms:1000}}
async function bootFrames(file,round,cap,say){const buf=new Uint8Array(await file.arrayBuffer());
 const cv=document.createElement("canvas");cv.width=cv.height=240;const g=cv.getContext("2d");
 const draw=f=>{g.fillStyle="#000";g.fillRect(0,0,240,240);g.save();const s=round?212:240,o=(240-s)/2;
  if(round){g.beginPath();g.arc(120,120,106,0,2*Math.PI);g.clip()}
  const k=Math.max(s/f.w,s/f.h);g.drawImage(f.img,o+(s-f.w*k)/2,o+(s-f.h*k)/2,f.w*k,f.h*k);g.restore()};
 const jpg=q=>new Promise((ok,ko)=>cv.toBlob(b=>b?b.arrayBuffer().then(a=>ok(new Uint8Array(a)),ko):ko(new Error("JPEG")),"image/jpeg",q));
 for(const [q,every] of [[.85,1],[.7,1],[.55,1],[.4,1],[.4,2],[.4,3],[.4,4],[.4,6],[.4,8],[.3,12]]){
  const out=[];let i=0,size=12,big=false;
  for await(const f of imgFrames(file,buf)){
   if(i++%every){out[out.length-1].ms+=f.ms;continue}
   if(out.length>=300)break;
   draw(f);const j=await jpg(q);out.push({jpg:j,ms:f.ms});size+=12+j.length;
   if(size>cap){big=true;break}}
  if(!big){for(const f of out)f.ms=Math.min(Math.max(f.ms,20),10000);return out}
  if(say)say("Too big, trying "+(every>1?"fewer frames":"a lower quality")+"...")}
 throw new Error("too big even at low quality: use a shorter GIF")}
let loadN=0;  // fichier ou taille changé pendant le traitement : l'ancien résultat est jeté
async function loadImg(){const f=$("img").files[0];if(!f)return;const n=++loadN;frames=[];$("send").disabled=true;msg("Reading the picture...");
 try{const fr=await bootFrames(f,$("fit").value=="1",cfg.bootCapacity||1966080,t=>n==loadN&&msg(t));if(n!=loadN)return;frames=fr;const kb=Math.round(pack().length/1024);
  const b=await createImageBitmap(new Blob([frames[0].jpg],{type:"image/jpeg"}));$("prev").getContext("2d").drawImage(b,0,0);
  $("info").textContent=frames.length+" frame"+(frames.length>1?"s":"")+", "+kb+" KB";$("send").disabled=false;msg("")}
 catch(err){if(n==loadN)msg("This picture could not be read: "+err.message)}}
$("img").onchange=$("fit").onchange=loadImg;
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
