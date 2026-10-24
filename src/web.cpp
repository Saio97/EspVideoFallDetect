#include "web.h"

#include <ArduinoJson.h>
#include <WebServer.h>

#include "app.h"
#include "config.h"
#include "settings.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

namespace {

WebServer server(80);

const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Rilevatore cadute</title>
<style>
:root{--bg:#f5f6f8;--card:#fff;--fg:#1d2330;--mut:#667085;--ok:#12805c;--warn:#b54708;--bad:#c01048;--acc:#2f5bd3}
@media (prefers-color-scheme:dark){:root{--bg:#111418;--card:#1b2027;--fg:#e8eaee;--mut:#98a2b3;--acc:#7aa2ff}}
*{box-sizing:border-box}body{margin:0;font:16px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:760px;margin:0 auto;padding:16px}h1{font-size:1.4rem;margin:.2rem 0 1rem}
.card{background:var(--card);border-radius:12px;padding:16px;margin-bottom:14px;box-shadow:0 1px 3px #0002}
.state{font-size:1.5rem;font-weight:700}.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
.k{color:var(--mut);font-size:.85rem}button{font:inherit;padding:10px 14px;border-radius:9px;border:0;
background:var(--acc);color:#fff;cursor:pointer;margin:4px 6px 4px 0}button.sec{background:#667085}
button.big{background:var(--ok);font-size:1.1rem;padding:14px 20px}
label{display:block;margin:6px 0}input{font:inherit;width:100%;padding:8px;border-radius:8px;border:1px solid #8884;background:transparent;color:inherit}
.wrap{position:relative;max-width:640px}canvas,img{width:100%;border-radius:8px;display:block}
canvas{position:absolute;inset:0}small{color:var(--mut)}
</style></head><body><main>
<h1>Rilevatore cadute</h1>
<div class="card"><div class="k">Stato</div><div id="st" class="state">…</div>
<div class="grid" style="margin-top:10px">
<div><div class="k">Persona inquadrata</div><div id="pp">-</div></div>
<div><div class="k">Forma (alt./larg.)</div><div id="asp">-</div></div>
<div><div class="k">Nello stato da</div><div id="ins">-</div></div>
<div><div class="k">Allarmi dall'avvio</div><div id="ac">-</div></div>
<div><div class="k">Acceso da</div><div id="up">-</div></div>
<div><div class="k">Wi-Fi</div><div id="rs">-</div></div>
</div>
<p><button class="big" onclick="post('/api/ack')">Sto bene / allarme gestito</button></p>
<button onclick="post('/api/test')">Allarme di prova</button>
<button class="sec" onclick="post('/api/pause?min=30')">Pausa 30 min</button>
<button class="sec" onclick="post('/api/resume')">Riprendi</button>
<button class="sec" onclick="post('/api/relearn')">Riapprendi ambiente</button>
</div>
<div class="card"><div class="k">Inquadratura (il riquadro verde è la sagoma rilevata)</div>
<div class="wrap" id="wrap" style="display:none"><img id="img" alt="istantanea"><canvas id="cv" width="320" height="240"></canvas></div>
<p><button onclick="snap()">Scatta istantanea</button><small> L'immagine non viene salvata.</small></p></div>
<div class="card"><div class="k">Taratura</div><form id="pf"></form>
<button onclick="saveParams()">Salva soglie</button><small id="pmsg"></small>
<p><small>Spiegazione delle soglie in docs/TARATURA.md. Tenendo premuto il pulsante all'accensione si ripristinano i valori predefiniti.</small></p></div>
</main><script>
const L={diffThreshold:'Sensibilità al contrasto (8-120, più basso = più sensibile)',minBlobFraction:'Dimensione minima della persona (frazione immagine)',
fallWindowMs:'Durata massima della caduta (ms)',standingAspect:'Rapporto alt./larg. minimo in piedi',lyingAspect:'Rapporto alt./larg. massimo a terra',
fallDropFraction:'Abbassamento minimo della testa (frazione altezza immagine)',groundTimeMs:'Tempo a terra prima dell\'allarme (ms)',recoverTopFraction:'Tolleranza per considerare la persona rialzata'};
let last={};
async function post(u){await fetch(u,{method:'POST'});refresh()}
function fmt(s){return s<60?s+' s':Math.floor(s/60)+' min'}
async function refresh(){try{const r=await fetch('/api/status');const s=await r.json();last=s;
const e=document.getElementById('st');e.textContent=s.stateLabel;
e.className='state '+(s.alarmActive||s.state=='ALARM'?'bad':s.state=='FALL_SUSPECTED'||s.paused?'warn':'ok');
pp.textContent=s.personPresent?'sì':'no';asp.textContent=s.personPresent?s.aspect.toFixed(2):'-';
ins.textContent=fmt(s.inStateS);ac.textContent=s.alarmCount;up.textContent=s.uptime;rs.textContent=s.rssi+' dBm';
drawBox()}catch(e){}}
function drawBox(){const c=cv.getContext('2d');c.clearRect(0,0,320,240);if(!last.personPresent)return;
const k=320/last.detW,b=last.box;c.strokeStyle='#22c55e';c.lineWidth=3;c.strokeRect(b[0]*k,b[1]*k,(b[2]-b[0]+1)*k,(b[3]-b[1]+1)*k)}
function snap(){img.onload=()=>{wrap.style.display='block';drawBox()};img.src='/snapshot.jpg?t='+Date.now()}
async function loadParams(){const p=await (await fetch('/api/params')).json();pf.innerHTML='';
for(const k in p){pf.insertAdjacentHTML('beforeend',`<label>${L[k]||k}<input name="${k}" type="number" step="any" value="${p[k]}"></label>`)}}
async function saveParams(){const o={};for(const i of pf.querySelectorAll('input'))o[i.name]=parseFloat(i.value);
const r=await fetch('/api/params',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});
pmsg.textContent=r.ok?' Salvate.':' Errore.';loadParams()}
refresh();loadParams();setInterval(refresh,1000);
</script></body></html>)HTML";

bool auth() {
  if (server.authenticate(WEB_USER, WEB_PASSWORD)) return true;
  server.requestAuthentication(BASIC_AUTH, DEVICE_NAME);
  return false;
}

void sendJson(const JsonDocument& doc, int code = 200) {
  String body;
  serializeJson(doc, body);
  server.send(code, "application/json", body);
}

void ok() { server.send(200, "application/json", "{\"ok\":true}"); }

}  // namespace

void webBegin() {
  server.on("/", HTTP_GET, [] {
    if (!auth()) return;
    server.send_P(200, "text/html; charset=utf-8", kIndexHtml);
  });

  server.on("/api/status", HTTP_GET, [] {
    if (!auth()) return;
    JsonDocument doc;
    appStatusJson(doc.to<JsonObject>());
    sendJson(doc);
  });

  server.on("/api/ack", HTTP_POST, [] {
    if (!auth()) return;
    appAcknowledge("pagina web");
    ok();
  });

  server.on("/api/test", HTTP_POST, [] {
    if (!auth()) return;
    appTestAlarm("pagina web");
    ok();
  });

  server.on("/api/relearn", HTTP_POST, [] {
    if (!auth()) return;
    appRelearnBackground();
    ok();
  });

  server.on("/api/pause", HTTP_POST, [] {
    if (!auth()) return;
    long min = server.hasArg("min") ? server.arg("min").toInt() : 30;
    appPause(min > 0 && min <= 24 * 60 ? min : 30);
    ok();
  });

  server.on("/api/resume", HTTP_POST, [] {
    if (!auth()) return;
    appResume();
    ok();
  });

  server.on("/api/params", HTTP_GET, [] {
    if (!auth()) return;
    JsonDocument doc;
    paramsToJson(appGetParams(), doc.to<JsonObject>());
    sendJson(doc);
  });

  server.on("/api/params", HTTP_POST, [] {
    if (!auth()) return;
    JsonDocument in;
    if (deserializeJson(in, server.arg("plain"))) {
      server.send(400, "application/json", "{\"ok\":false}");
      return;
    }
    fall::Params p = appGetParams();
    paramsFromJson(in.as<JsonObjectConst>(), p);
    appSetParams(p, true);
    ok();
  });

  server.on("/snapshot.jpg", HTTP_GET, [] {
    if (!auth()) return;
    if (!ENABLE_WEB_SNAPSHOT) {
      server.send(403, "text/plain", "Istantanee disattivate (ENABLE_WEB_SNAPSHOT)");
      return;
    }
    std::vector<uint8_t> jpg;
    if (!appSnapshot(jpg)) {
      server.send(503, "text/plain", "Telecamera occupata, riprova");
      return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "image/jpeg", reinterpret_cast<const char*>(jpg.data()), jpg.size());
  });

  server.onNotFound([] { server.send(404, "text/plain", "Non trovato"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
