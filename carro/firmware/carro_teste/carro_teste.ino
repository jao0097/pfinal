// ============================================================
//  CARRO ESP32 — ETAPA 1: TESTE E DESCOBERTA DOS PINOS
//
//  Hardware (pelas fotos):
//    ESP32 DevKit V1 (30 pinos) · ponte H L298N · 4 motores TT
//    (2 de cada lado, em paralelo) · 2 pilhas 18650 com chave
//    · sensor ultrassônico HC-SR04 na frente.
//
//  Como não dá pra ler nas fotos em qual pino do ESP32 cada fio
//  entrou, este programa DESCOBRE isso com você, pelo celular:
//
//    1. Acha sozinho os pinos TRIG e ECHO do sensor.
//    2. Liga um pino por vez (por 1 s) e você anota o que a roda fez.
//    3. Monta a configuração a partir das suas respostas e salva
//       na memória do ESP32 (a etapa 2 — controle — lê daqui).
//    4. Teste final: frente, ré, girar, com velocidade.
//
//  Como usar:
//    - Grave este arquivo (placa "ESP32 Dev Module").
//    - Conecte o celular na rede  Carro-ESP32  (senha carro1234).
//    - Abra  http://192.168.4.1
//    - Deixe o carro com as RODAS NO AR (em cima de um copo/caixa).
//
//  Segurança:
//    - Todo movimento para sozinho (máx. 3 s). Botão PARAR sempre visível.
//    - No boot, todos os pinos livres ficam em LOW com corrente fraca,
//      então nenhuma entrada da ponte H fica "solta" girando motor.
//    - Nunca mexe nos pinos 0, 1, 3 (USB), 6–11 (memória flash).
//
//  Funciona com o pacote ESP32 2.x e 3.x da Arduino IDE.
//  Nenhuma biblioteca extra.
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "driver/gpio.h"
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define CORE3 1
#else
#define CORE3 0
#endif

// ---------------- Configuração ----------------
const char* REDE  = "Carro-ESP32";
const char* SENHA = "carro1234";   // mínimo 8 caracteres

// Pinos que podem ser SAÍDA sem risco na DevKit V1
const int8_t SAIDAS[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
const int N_SAIDAS = sizeof(SAIDAS);
// Pinos que só servem de ENTRADA (podem ser o ECHO do sensor)
const int8_t SO_ENTRADA[] = {34, 35, 36, 39};
const int N_SO_ENTRADA = sizeof(SO_ENTRADA);

// Funções de cada pino. -1 = não usado.
//   EF/ET = lado esquerdo para frente/trás · DF/DT = lado direito
//   ENE/END_ = ENA/ENB da ponte H (só se NÃO tiver jumper)
enum { EF, ET, DF, DT, ENE, END_, TRIG, ECHO, N_FUNC };
const char* FUNC_ID[N_FUNC] = {"ef", "et", "df", "dt", "ene", "end", "trig", "echo"};
int8_t pino[N_FUNC];

const int PWM_FREQ = 1000;   // 1 kHz combina com o L298N
const int PWM_BITS = 8;      // velocidade 0..255
const uint32_t MS_MIN = 100, MS_MAX = 3000;

WebServer server(80);
Preferences prefs;

bool pwmLigado = false;
bool emAcao = false;
uint32_t inicioAcao = 0, duracaoAcao = 0;
String acao = "parado";

// ---------------- Utilidades de pinos ----------------
bool ehSaida(int p)     { for (int i = 0; i < N_SAIDAS; i++) if (SAIDAS[i] == p) return true; return false; }
bool ehSoEntrada(int p) { for (int i = 0; i < N_SO_ENTRADA; i++) if (SO_ENTRADA[i] == p) return true; return false; }

void carregarConfig() {
  for (int f = 0; f < N_FUNC; f++) pino[f] = -1;
  prefs.begin("carro", true);
  if (prefs.getBytesLength("pinos") == N_FUNC) prefs.getBytes("pinos", pino, N_FUNC);
  prefs.end();
  // Confere o que veio da memória
  for (int f = 0; f < N_FUNC; f++) {
    int p = pino[f];
    bool ok = p == -1 || ehSaida(p) || (f == ECHO && ehSoEntrada(p));
    if (!ok) pino[f] = -1;
  }
}

void salvarConfig() {
  prefs.begin("carro", false);
  prefs.putBytes("pinos", pino, N_FUNC);
  prefs.putUChar("versao", 1);
  prefs.end();
}

void desligarPWM() {
  if (!pwmLigado) return;
  for (int f = EF; f <= END_; f++) {
    if (pino[f] < 0) continue;
#if CORE3
    ledcDetach(pino[f]);
#else
    ledcDetachPin(pino[f]);
#endif
  }
  pwmLigado = false;
}

void ligarPWM() {
  if (pwmLigado) return;
  for (int f = EF; f <= END_; f++) {
    if (pino[f] < 0) continue;
#if CORE3
    ledcAttach(pino[f], PWM_FREQ, PWM_BITS);
    ledcWrite(pino[f], 0);
#else
    ledcSetup(f, PWM_FREQ, PWM_BITS);
    ledcAttachPin(pino[f], f);
    ledcWrite(f, 0);
#endif
  }
  pwmLigado = true;
}

void escrever(int f, int duty) {
  if (pino[f] < 0) return;
#if CORE3
  ledcWrite(pino[f], duty);
#else
  ledcWrite(f, duty);
#endif
}

// Estado seguro: tudo parado, todos os pinos livres em LOW (corrente fraca).
void pinosSeguros() {
  desligarPWM();
  for (int i = 0; i < N_SAIDAS; i++) {
    int p = SAIDAS[i];
    if (p == pino[ECHO]) { pinMode(p, INPUT); continue; }
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
    gpio_set_drive_capability((gpio_num_t)p, GPIO_DRIVE_CAP_0);
  }
  if (pino[ECHO] >= 0) pinMode(pino[ECHO], INPUT);
  emAcao = false;
  acao = "parado";
}

void iniciarAcao(uint32_t ms, const String& nome) {
  inicioAcao = millis();
  duracaoAcao = constrain(ms, MS_MIN, MS_MAX);
  emAcao = true;
  acao = nome;
  Serial.printf("[acao] %s (%lu ms)\n", nome.c_str(), (unsigned long)duracaoAcao);
}

// ---------------- Motores ----------------
// lado: 0 = esquerdo, 1 = direito · v: -255 (trás) .. 255 (frente)
void ladoSet(int lado, int v) {
  int f = lado ? DF : EF, t = lado ? DT : ET, e = lado ? END_ : ENE;
  int mag = constrain(abs(v), 0, 255);
  if (v == 0) { escrever(f, 0); escrever(t, 0); escrever(e, 0); return; }
  int ativo = v > 0 ? f : t, outro = v > 0 ? t : f;
  escrever(outro, 0);
  if (pino[e] >= 0) { escrever(ativo, 255); escrever(e, mag); }  // velocidade no ENA/ENB
  else              { escrever(ativo, mag); }                     // velocidade no próprio IN
}

bool motoresConfigurados() {
  return pino[EF] >= 0 && pino[ET] >= 0 && pino[DF] >= 0 && pino[DT] >= 0;
}

// ---------------- Sensor ultrassônico ----------------
// Retorna cm · -1 = não configurado · -2 = sem eco (longe demais ou fio solto)
float medirCm() {
  if (pino[TRIG] < 0 || pino[ECHO] < 0) return -1;
  digitalWrite(pino[TRIG], LOW);  delayMicroseconds(3);
  digitalWrite(pino[TRIG], HIGH); delayMicroseconds(10);
  digitalWrite(pino[TRIG], LOW);
  unsigned long us = pulseIn(pino[ECHO], HIGH, 25000);
  if (!us) return -2;
  return us / 58.0f;
}

uint32_t lerMascara(const int8_t* pinos, int n) {
  uint32_t m = 0;
  for (int i = 0; i < n; i++) if (digitalRead(pinos[i])) m |= (1u << i);
  return m;
}

// Testa cada pino como TRIG e vê qual outro pino devolve um pulso de ECHO.
// Só usa pulsos de 12 µs: não chega a mexer motor nenhum.
String procurarUltrassom() {
  pinosSeguros();
  int8_t todos[32];
  int n = 0;
  for (int i = 0; i < N_SAIDAS; i++) todos[n++] = SAIDAS[i];
  for (int i = 0; i < N_SO_ENTRADA; i++) todos[n++] = SO_ENTRADA[i];

  String out = "[";
  bool primeiro = true;
  for (int ti = 0; ti < N_SAIDAS; ti++) {
    int8_t T = SAIDAS[ti];
    for (int i = 0; i < n; i++) {
      if (todos[i] == T) continue;
      pinMode(todos[i], ehSaida(todos[i]) ? INPUT_PULLDOWN : INPUT);
    }
    pinMode(T, OUTPUT);
    digitalWrite(T, LOW);
    delay(3);

    uint32_t ok = 0xFFFFFFFFu;
    uint32_t largura[32] = {0};
    for (int rep = 0; rep < 2; rep++) {
      uint32_t antes = lerMascara(todos, n);
      uint32_t subiuEm[32] = {0};
      uint32_t subiu = 0, desceu = 0;
      digitalWrite(T, HIGH); delayMicroseconds(12); digitalWrite(T, LOW);
      uint32_t t0 = micros();
      while (micros() - t0 < 50000) {
        uint32_t m = lerMascara(todos, n);
        uint32_t agora = micros() - t0;
        uint32_t novos = m & ~subiu & ~antes;
        uint32_t caiu  = ~m & subiu & ~desceu;
        if (novos | caiu) {
          for (int i = 0; i < n; i++) {
            if (novos & (1u << i)) { subiuEm[i] = agora; subiu |= (1u << i); }
            if (caiu  & (1u << i)) { largura[i] = agora - subiuEm[i]; desceu |= (1u << i); }
          }
        }
      }
      ok &= desceu & ~antes;
      delay(60);  // o HC-SR04 precisa de ~60 ms entre medidas
    }
    ok &= ~(1u << ti);  // o próprio trig não conta (ti == índice em todos[])
    for (int i = 0; i < n; i++) {
      if (!(ok & (1u << i))) continue;
      if (!primeiro) out += ",";
      primeiro = false;
      out += "{\"trig\":" + String(T) + ",\"echo\":" + String(todos[i]) +
             ",\"cm\":" + String(largura[i] / 58.0f, 1) + "}";
      Serial.printf("[sensor] TRIG=%d ECHO=%d ~%.1f cm\n", T, todos[i], largura[i] / 58.0f);
    }
  }
  pinosSeguros();
  return out + "]";
}

// ---------------- Respostas HTTP ----------------
String jsonPinos() {
  String s = "{";
  for (int f = 0; f < N_FUNC; f++) {
    if (f) s += ",";
    s += "\"" + String(FUNC_ID[f]) + "\":" + String(pino[f]);
  }
  return s + "}";
}

uint32_t argMs() { return server.hasArg("ms") ? server.arg("ms").toInt() : 1000; }
int argVel()     { return constrain(server.hasArg("v") ? server.arg("v").toInt() : 200, 0, 255); }
void erro(const String& msg) { server.send(400, "text/plain; charset=utf-8", msg); }
void okJson(const String& s) { server.send(200, "application/json", s); }

void hEstado() {
  float cm = medirCm();
  String s = "{\"pinos\":" + jsonPinos() +
             ",\"acao\":\"" + acao + "\"" +
             ",\"cm\":" + String(cm, 1) +
             ",\"core\":" + String(CORE3 ? 3 : 2) +
             ",\"clientes\":" + String(WiFi.softAPgetStationNum()) + "}";
  okJson(s);
}

void hParar() { pinosSeguros(); okJson("{\"ok\":1}"); }

// Liga UM pino (ou, no modo invertido, todos MENOS ele) por alguns instantes.
void hPulso() {
  int p = server.arg("p").toInt();
  bool inv = server.arg("inv") == "1";
  if (!ehSaida(p)) return erro("Pino inválido.");
  if (p == pino[TRIG] || p == pino[ECHO]) return erro("Esse pino é do sensor ultrassônico.");
  pinosSeguros();
  if (inv) {
    for (int i = 0; i < N_SAIDAS; i++) {
      int s = SAIDAS[i];
      if (s != p && s != pino[TRIG] && s != pino[ECHO]) digitalWrite(s, HIGH);
    }
  } else {
    digitalWrite(p, HIGH);
  }
  iniciarAcao(argMs(), String(inv ? "todos menos D" : "pulso D") + p);
  okJson("{\"ok\":1}");
}

void hMotor() {
  if (!motoresConfigurados()) return erro("Configure e salve os 4 pinos dos motores primeiro.");
  int lado = server.arg("lado") == "d" ? 1 : 0;
  int dir = server.arg("dir").toInt() < 0 ? -1 : 1;
  int v = argVel();
  pinosSeguros();
  ligarPWM();
  ladoSet(lado, dir * v);
  iniciarAcao(argMs(), String(lado ? "direita " : "esquerda ") + (dir > 0 ? "frente" : "trás"));
  okJson("{\"ok\":1}");
}

void hCarro() {
  if (!motoresConfigurados()) return erro("Configure e salve os 4 pinos dos motores primeiro.");
  String m = server.arg("m");
  int v = argVel(), e = 0, d = 0;
  String nome;
  if (m == "f")      { e =  v; d =  v; nome = "frente"; }
  else if (m == "r") { e = -v; d = -v; nome = "ré"; }
  else if (m == "e") { e = -v; d =  v; nome = "gira p/ esquerda"; }
  else if (m == "d") { e =  v; d = -v; nome = "gira p/ direita"; }
  else return erro("Movimento inválido.");
  pinosSeguros();
  ligarPWM();
  ladoSet(0, e);
  ladoSet(1, d);
  iniciarAcao(argMs(), nome);
  okJson("{\"ok\":1}");
}

void hSalvar() {
  int8_t novo[N_FUNC];
  for (int f = 0; f < N_FUNC; f++) {
    int p = server.hasArg(FUNC_ID[f]) ? server.arg(FUNC_ID[f]).toInt() : -1;
    bool ok = p == -1 || ehSaida(p) || (f == ECHO && ehSoEntrada(p));
    if (!ok) return erro(String("Pino ") + p + " não serve para " + FUNC_ID[f] + ".");
    for (int g = 0; g < f; g++)
      if (p != -1 && novo[g] == p) return erro(String("O pino D") + p + " foi escolhido duas vezes.");
    novo[f] = p;
  }
  pinosSeguros();
  memcpy(pino, novo, N_FUNC);
  salvarConfig();
  pinosSeguros();
  Serial.println("[config] salva: " + jsonPinos());
  okJson("{\"ok\":1,\"pinos\":" + jsonPinos() + "}");
}

void hEsquecer() {
  pinosSeguros();
  for (int f = 0; f < N_FUNC; f++) pino[f] = -1;
  prefs.begin("carro", false); prefs.clear(); prefs.end();
  pinosSeguros();
  okJson("{\"ok\":1}");
}

void hUltra() { okJson("{\"achados\":" + procurarUltrassom() + "}"); }

// ---------------- Página ----------------
const char PAGINA[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Carro – Teste</title>
<style>
:root{--bg:#101418;--card:#1a2027;--tx:#e8edf2;--mut:#93a1b0;--ac:#ff4d3d;--ok:#2ecc71;--warn:#f5b041;--bd:#2a323c;--btn:#2b3540}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.45 system-ui,-apple-system,sans-serif;padding:0 12px 96px}
main{max-width:640px;margin:0 auto}
h1{font-size:20px;margin:14px 0 2px}h2{font-size:17px;margin:0 0 6px}
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:14px;margin:12px 0}
.mut{color:var(--mut);font-size:13px}
ul{margin:6px 0;padding-left:20px}li{margin:3px 0}
button{font:inherit;border:0;border-radius:10px;padding:11px 12px;background:var(--btn);color:var(--tx);cursor:pointer;touch-action:manipulation}
button:active{transform:scale(.97)}button:disabled{opacity:.35}
.pri{background:var(--ac);color:#fff;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(88px,1fr));gap:8px;margin-top:10px}
.pin{display:flex;flex-direction:column;align-items:center;padding:9px 4px;min-height:58px}
.pin b{font-size:16px}.pin small{font-size:11px;color:var(--mut);text-align:center}
.pin.tem{box-shadow:inset 0 0 0 2px var(--ok)}.pin.nada{opacity:.55}.pin.sel{box-shadow:inset 0 0 0 2px var(--ac)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.chips button{padding:8px 10px;font-size:13px}
#anot{display:none;margin-top:12px;padding:12px;border-radius:10px;background:#11171d;border:1px solid var(--bd)}
select,input[type=range]{width:100%}
select{font:inherit;background:#0d1115;color:var(--tx);border:1px solid var(--bd);border-radius:8px;padding:8px}
.cfg{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label{font-size:13px;color:var(--mut);display:block}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:8px}.row>*{flex:1}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:10px}
.pad button{padding:14px 6px}
#parar{position:fixed;left:12px;right:12px;bottom:12px;max-width:640px;margin:0 auto;padding:16px;font-size:18px;background:#c0392b;color:#fff;font-weight:700;border-radius:14px;z-index:9}
#st{position:sticky;top:0;z-index:5;background:var(--bg);padding:8px 0;font-size:13px;color:var(--mut);border-bottom:1px solid var(--bd)}
#st b{color:var(--tx)}
textarea{width:100%;min-height:150px;background:#0d1115;color:var(--tx);border:1px solid var(--bd);border-radius:8px;padding:10px;font:12px/1.4 ui-monospace,monospace}
.ok{color:var(--ok)}.warn{color:var(--warn)}.msg{margin-top:8px;font-size:14px}
</style></head><body><main>
<div id="st">● <b id="con">conectando…</b> · ação: <b id="acao">–</b> · sensor: <b id="cm">–</b></div>
<h1>Carro ESP32 · Etapa 1: teste</h1>
<p class="mut">Vamos descobrir em qual pino do ESP32 cada fio entrou e conferir o sentido de cada roda.</p>

<div class="card"><h2>Antes de começar</h2><ul>
<li><b>Rodas no ar</b>: apoie o carro num copo ou caixa.</li>
<li>Chave das pilhas <b>ligada</b> (LED vermelho da ponte H aceso).</li>
<li><b>GND do ESP32 ligado no GND da ponte H</b>. Sem isso nenhum motor gira.</li>
<li><b>Frente</b> do carro = lado do sensor. <b>Esquerda/direita</b> = como se você estivesse sentado nele.</li>
</ul></div>

<div class="card"><h2>1 · Achar o sensor ultrassônico</h2>
<p class="mut">O ESP32 testa os pinos sozinho com pulsos curtíssimos (não mexe motor). Leva uns 5 segundos.</p>
<button class="pri" id="bUltra">Procurar sensor</button>
<div id="ultraRes" class="msg"></div></div>

<div class="card"><h2>2 · Descobrir os motores</h2>
<p class="mut">Toque num pino: ele fica ligado por um instante. Depois diga o que as rodas fizeram.
Pinos sem nada ligado não fazem nada — é normal.</p>
<div class="row"><div><label>Modo</label><select id="modo">
<option value="0">Normal (jumpers no ENA/ENB)</option>
<option value="1">ENA/ENB com fio para o ESP32</option></select></div>
<div><label>Tempo ligado</label><select id="dur">
<option value="500">0,5 s</option><option value="1000" selected>1 s</option>
<option value="1500">1,5 s</option><option value="2000">2 s</option></select></div></div>
<p class="mut" id="modoAjuda"></p>
<div class="grid" id="grade"></div>
<div id="anot"><b id="anotT"></b><div class="chips" id="anotC"></div></div>
<div class="row"><button class="pri" id="bMontar">Montar configuração</button><button id="bLimpar">Limpar respostas</button></div>
<div id="montarMsg" class="msg"></div></div>

<div class="card"><h2>3 · Configuração</h2>
<p class="mut">Preenchida pelo passo 2, mas dá pra ajustar à mão. Fica salva no ESP32.</p>
<div class="cfg" id="cfg"></div>
<div class="row"><button class="pri" id="bSalvar">Salvar no ESP32</button><button id="bEsquecer">Apagar</button></div>
<div id="salvarMsg" class="msg"></div></div>

<div class="card"><h2>4 · Teste final</h2>
<label>Velocidade: <b id="velV">200</b></label>
<input type="range" id="vel" min="60" max="255" value="200">
<p class="mut">Lados separados:</p>
<div class="pad">
<button data-m="e,1">Esq. ↑</button><span></span><button data-m="d,1">Dir. ↑</button>
<button data-m="e,-1">Esq. ↓</button><span></span><button data-m="d,-1">Dir. ↓</button></div>
<p class="mut">Carro inteiro:</p>
<div class="pad">
<span></span><button data-c="f">Frente</button><span></span>
<button data-c="e">⟲ Gira</button><button data-c="r">Ré</button><button data-c="d">Gira ⟳</button></div>
<ul class="mut">
<li>"Frente": as 4 rodas giram pra frente?</li>
<li>"Gira ⟳": o carro giraria no sentido do relógio (visto de cima)?</li>
<li>Abaixe a velocidade até o carro quase não andar e anote o número.</li></ul></div>

<div class="card"><h2>5 · Me mande o resultado</h2>
<p class="mut">Copie e cole na conversa para a etapa 2 (controle pela web).</p>
<textarea id="res" readonly></textarea>
<div class="row"><button id="bCopiar">Copiar</button></div></div>
</main>
<button id="parar">PARAR</button>
<script>
const SAIDAS=[2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33],ENTRADAS=[34,35,36,39];
const NOME=p=>p==16?'RX2':p==17?'TX2':'D'+p;
const RESP=[['nada','Nada'],['ef','Esquerda ↑ frente'],['et','Esquerda ↓ trás'],['df','Direita ↑ frente'],['dt','Direita ↓ trás'],
 ['ambos','Os dois lados'],['briga','Rodas do mesmo lado em sentidos opostos'],['uma','Só 1 roda girou']];
const CURTO={nada:'nada',ef:'Esq ↑',et:'Esq ↓',df:'Dir ↑',dt:'Dir ↓',ambos:'2 lados',briga:'briga!',uma:'1 roda'};
const OPOSTO={ef:'et',et:'ef',df:'dt',dt:'df'};
const FUNCS=[['ef','Esquerda ↑ frente'],['et','Esquerda ↓ trás'],['df','Direita ↑ frente'],['dt','Direita ↓ trás'],
 ['ene','ENA esquerda (opcional)'],['end','ENB direita (opcional)'],['trig','Sensor TRIG'],['echo','Sensor ECHO']];
const $=id=>document.getElementById(id);
let anot={},pinos={},atual=null,ultra=[],nota='';
try{anot=JSON.parse(localStorage.getItem('anot')||'{}')}catch(e){}
const guardar=()=>{try{localStorage.setItem('anot',JSON.stringify(anot))}catch(e){}};

async function api(u){
  const c=new AbortController(),t=setTimeout(()=>c.abort(),15000);
  try{const r=await fetch(u,{cache:'no-store',signal:c.signal});const x=await r.text();
    if(!r.ok)throw new Error(x);try{return JSON.parse(x)}catch(e){return x}}
  finally{clearTimeout(t)}
}
const dur=()=>$('dur').value, vel=()=>$('vel').value, inv=()=>$('modo').value;

function grade(){
  const g=$('grade');g.innerHTML='';
  SAIDAS.forEach(p=>{
    const b=document.createElement('button');const a=anot[p];
    b.className='pin'+(a?(a.r=='nada'?' nada':' tem'):'')+(atual==p?' sel':'');
    const sensor=p==pinos.trig||p==pinos.echo;
    b.innerHTML='<b>'+NOME(p)+'</b><small>'+(sensor?'sensor':a?CURTO[a.r]+(a.inv?' (inv)':''):'—')+'</small>';
    b.disabled=sensor;b.onclick=()=>pulso(p);g.appendChild(b);
  });
}
async function pulso(p){
  atual=p;grade();
  try{await api(`/pulso?p=${p}&ms=${dur()}&inv=${inv()}`)}catch(e){alert(e.message);return}
  $('anot').style.display='block';
  $('anotT').textContent='O que aconteceu com '+NOME(p)+'?';
  const c=$('anotC');c.innerHTML='';
  RESP.forEach(([k,t])=>{const b=document.createElement('button');b.textContent=t;
    b.onclick=()=>{anot[p]={r:k,inv:inv()=='1'};guardar();$('anot').style.display='none';atual=null;grade();resumo()};c.appendChild(b)});
  const r=document.createElement('button');r.textContent='↻ Repetir';r.onclick=()=>pulso(p);c.appendChild(r);
}
function ajudaModo(){
  $('modoAjuda').innerHTML=inv()=='1'
   ?'Modo invertido: liga <b>todos os pinos menos o escolhido</b>. Use só se no modo normal nada girou. Faça o passo 1 antes.'
   :'Se no modo normal <b>nenhum</b> pino fizer roda girar, seus ENA/ENB devem estar ligados no ESP32: troque o modo.';
}
function montar(){
  const f={},avisos=[];
  for(const p in anot){const a=anot[p];let k=a.r;
    if(OPOSTO[k]){if(a.inv)k=OPOSTO[k];(f[k]=f[k]||[]).push(+p)}
    else if(k=='briga')avisos.push(NOME(p)+': as rodas do mesmo lado brigam — inverta os 2 fios de UM dos motores daquele lado.');
    else if(k=='uma')avisos.push(NOME(p)+': só 1 roda girou — o outro motor daquele lado está com fio solto.');
    else if(k=='ambos')avisos.push(NOME(p)+': mexeu os dois lados — confira se os fios IN não estão em curto.');
  }
  const falta=[];
  ['ef','et','df','dt'].forEach(k=>{const l=f[k]||[];
    if(l.length==1)setSel(k,l[0]);else if(!l.length)falta.push(FUNCS.find(x=>x[0]==k)[1]);
    else avisos.push(FUNCS.find(x=>x[0]==k)[1]+' apareceu em '+l.map(NOME).join(' e ')+'. Repita esses pinos.')});
  let h=falta.length?'<span class="warn">Falta descobrir: '+falta.join(', ')+'.</span><br>':'<span class="ok">Os 4 sentidos foram encontrados. Confira o passo 3 e salve.</span><br>';
  if(avisos.length)h+='<span class="warn">'+avisos.join('<br>')+'</span>';
  $('montarMsg').innerHTML=h;resumo();
}
function opcoes(k){
  const l=[-1,...SAIDAS,...(k=='echo'?ENTRADAS:[])];
  return l.map(p=>`<option value="${p}">${p<0?'— não usado —':NOME(p)}</option>`).join('');
}
function montarCfg(){
  $('cfg').innerHTML=FUNCS.map(([k,t])=>`<div><label>${t}</label><select id="s_${k}">${opcoes(k)}</select></div>`).join('');
  FUNCS.forEach(([k])=>$('s_'+k).onchange=resumo);
}
function setSel(k,p){$('s_'+k).value=p}
function preencherCfg(){FUNCS.forEach(([k])=>setSel(k,pinos[k]??-1));grade();resumo()}
async function salvar(){
  const q=FUNCS.map(([k])=>k+'='+$('s_'+k).value).join('&');
  try{const r=await api('/salvar?'+q);pinos=r.pinos;preencherCfg();$('salvarMsg').innerHTML='<span class="ok">Salvo. Agora faça o teste final.</span>'}
  catch(e){$('salvarMsg').innerHTML='<span class="warn">'+e.message+'</span>'}
}
async function procurar(){
  const b=$('bUltra');b.disabled=true;$('ultraRes').textContent='Procurando…';
  try{const r=await api('/ultra');ultra=r.achados;
    if(!ultra.length){$('ultraRes').innerHTML='<span class="warn">Nenhum sensor respondeu. Confira VCC no 5V, GND, e se TRIG/ECHO estão em pinos do ESP32.</span>'}
    else{$('ultraRes').innerHTML=ultra.map((a,i)=>`TRIG <b>${NOME(a.trig)}</b> · ECHO <b>${NOME(a.echo)}</b> · ${a.cm} cm <button data-i="${i}">Usar</button>`).join('<br>');
      $('ultraRes').querySelectorAll('button').forEach(x=>x.onclick=async()=>{const a=ultra[x.dataset.i];
        setSel('trig',a.trig);setSel('echo',a.echo);await salvar();
        $('ultraRes').innerHTML+='<br><span class="ok">Sensor salvo. Aponte a mão na frente e veja a distância lá em cima.</span>'})}
  }catch(e){$('ultraRes').innerHTML='<span class="warn">'+e.message+'</span>'}
  b.disabled=false;resumo();
}
function resumo(){
  const L=[];L.push('CARRO ESP32 - resultado da etapa 1');
  L.push('Configuração:');FUNCS.forEach(([k,t])=>{const v=+$('s_'+k).value;L.push('  '+t+': '+(v<0?'-':NOME(v)))});
  L.push('Salvo no ESP32: '+JSON.stringify(pinos));
  L.push('Respostas por pino:');
  Object.keys(anot).sort((a,b)=>a-b).forEach(p=>L.push('  '+NOME(+p)+' = '+RESP.find(x=>x[0]==anot[p].r)[1]+(anot[p].inv?' (modo invertido)':'')));
  if(ultra.length)L.push('Sensor achado: '+ultra.map(a=>NOME(a.trig)+'/'+NOME(a.echo)).join(', '));
  L.push('Velocidade mínima que anda: (anote aqui)');
  L.push('Frente/ré/giros corretos? (anote aqui)');
  $('res').value=L.join('\n');
}
async function estado(){
  try{const r=await api('/estado');$('con').textContent='conectado';$('acao').textContent=r.acao;
    $('cm').textContent=r.cm==-1?'não configurado':r.cm==-2?'sem eco':r.cm+' cm';
    if(JSON.stringify(r.pinos)!=JSON.stringify(pinos)){pinos=r.pinos;preencherCfg()}}
  catch(e){$('con').textContent='sem conexão'}
  setTimeout(estado,800);
}
async function mover(u){try{await api(u)}catch(e){alert(e.message)}}
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=()=>{const[l,d]=b.dataset.m.split(',');mover(`/motor?lado=${l}&dir=${d}&v=${vel()}&ms=${dur()}`)});
document.querySelectorAll('[data-c]').forEach(b=>b.onclick=()=>mover(`/carro?m=${b.dataset.c}&v=${vel()}&ms=${dur()}`));
$('vel').oninput=()=>$('velV').textContent=vel();
$('parar').onclick=()=>api('/parar').catch(()=>{});
$('bUltra').onclick=procurar;$('bMontar').onclick=montar;$('bSalvar').onclick=salvar;
$('bLimpar').onclick=()=>{if(confirm('Apagar todas as respostas do passo 2?')){anot={};guardar();grade();resumo()}};
$('bEsquecer').onclick=async()=>{if(confirm('Apagar a configuração salva no ESP32?')){await api('/esquecer');pinos={};preencherCfg()}};
$('bCopiar').onclick=()=>{const t=$('res');t.select();try{document.execCommand('copy');$('bCopiar').textContent='Copiado ✓'}catch(e){}
  if(navigator.clipboard)navigator.clipboard.writeText(t.value).catch(()=>{})};
$('modo').onchange=ajudaModo;
montarCfg();ajudaModo();grade();resumo();estado();
</script></body></html>)HTML";

void hPagina() { server.send_P(200, "text/html; charset=utf-8", PAGINA); }

// ---------------- Monitor Serial (alternativa sem celular) ----------------
//   p26  -> liga o pino 26 por 1 s      x -> para tudo
//   u    -> procura o sensor             c -> mostra a configuração
void serialComandos() {
  if (!Serial.available()) return;
  String c = Serial.readStringUntil('\n');
  c.trim();
  if (c.startsWith("p")) {
    int p = c.substring(1).toInt();
    if (!ehSaida(p)) { Serial.println("pino inválido"); return; }
    pinosSeguros(); digitalWrite(p, HIGH); iniciarAcao(1000, String("pulso D") + p);
  } else if (c == "x") { pinosSeguros(); Serial.println("parado");
  } else if (c == "u") { Serial.println(procurarUltrassom());
  } else if (c == "c") { Serial.println(jsonPinos());
  } else Serial.println("Comandos: p<pino> (ex: p26) · x parar · u procurar sensor · c config");
}

void setup() {
  Serial.begin(115200);
  carregarConfig();
  pinosSeguros();  // o quanto antes, para nenhum motor girar sozinho
  delay(300);
  Serial.println("\n=== Carro ESP32 · Etapa 1: teste ===");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(REDE, SENHA);
  Serial.printf("Rede: %s  Senha: %s\nAbra: http://%s\n", REDE, SENHA, WiFi.softAPIP().toString().c_str());
  Serial.println("Config salva: " + jsonPinos());
  Serial.println("Comandos: p<pino> · x · u · c");

  server.on("/", hPagina);
  server.on("/estado", hEstado);
  server.on("/parar", hParar);
  server.on("/pulso", hPulso);
  server.on("/motor", hMotor);
  server.on("/carro", hCarro);
  server.on("/salvar", hSalvar);
  server.on("/esquecer", hEsquecer);
  server.on("/ultra", hUltra);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();
}

void loop() {
  server.handleClient();
  serialComandos();
  if (emAcao && millis() - inicioAcao >= duracaoAcao) {
    pinosSeguros();
    Serial.println("[acao] parou");
  }
}
