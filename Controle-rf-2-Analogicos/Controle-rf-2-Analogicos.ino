#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
// Wi-Fi/WebServer (ESP32-C3)
#include <WiFi.h>
#include <WebServer.h>

// ================= RF24 =================
#define CE_PIN   6
#define CSN_PIN  10

RF24 radio(CE_PIN, CSN_PIN);
const byte endereco[6] = "CTRL1";

// ================= WIFI =================
const char* AP_SSID = "ESP32-RF-Control";  // Nome da rede criada pelo ESP
const char* AP_PASS = "12345678";          // Senha (mínimo 8 caracteres)
WebServer server(80);

// ================= PINOS =================
#define JOY1_X 1
#define JOY1_Y 0
#define JOY2_X 3
#define JOY2_Y 2
#define CHAVE_PIN 5

// ================= STRUCT =================
struct DadosControle {
  int joy1X;
  int joy1Y;
  int joy2X;
  int joy2Y;
  bool chave;
};

DadosControle dados;

// ================= STATUS GLOBAIS =================
bool g_tx_ok = false;
bool g_tx_fail = false;
bool g_rx_ready = false;
bool g_envio_ok = false;

// ================= CALIBRAÇÃO =================
struct CalibAxis {
  int minX = 0;
  int midX = 2048;
  int maxX = 4095;
  int minY = 0;
  int midY = 2048;
  int maxY = 4095;
  bool invertX = false;
  bool invertY = false; // invertido no layout (top usa 100 - Y)
};

CalibAxis calib1; // Joystick 1
CalibAxis calib2; // Joystick 2

int percentAxis(int v, int minV, int midV, int maxV) {
  if (maxV <= midV) maxV = midV + 1;
  if (midV <= minV) minV = midV - 1;
  float p;
  if (v >= midV) {
    p = 50.0f + 50.0f * float(v - midV) / float(maxV - midV);
  } else {
    p = 50.0f - 50.0f * float(midV - v) / float(midV - minV);
  }
  if (p < 0) p = 0; if (p > 100) p = 100;
  return int(p + 0.5f);
}

// ================= PÁGINAS (otimizado PROGMEM) =================
const char HTML_HEAD[] PROGMEM = 
  "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<title>ESP32 RF Controller</title>"
  "<style>:root{--ink:#0c1021;--text:#e2e8f0;--muted:#9ca3af;--accent:#7de0ff;--border:rgba(255,255,255,0.12);--shadow:0 30px 90px rgba(0,0,0,0.45)}"
  "*{margin:0;padding:0;box-sizing:border-box}"
  "body{font-family:system-ui,sans-serif;background:radial-gradient(circle at 20% 20%,rgba(124,58,237,0.25),transparent),radial-gradient(circle at 80% 0%,rgba(34,211,238,0.3),transparent),var(--ink);color:var(--text);min-height:100vh;padding:28px}"
  ".bg{position:fixed;inset:0;z-index:-2;background:linear-gradient(135deg,rgba(109,40,217,0.3),rgba(14,165,233,0.25));filter:blur(60px)}"
  ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:18px;margin-top:18px}"
  ".shell{max-width:1120px;margin:0 auto;position:relative;z-index:1}"
  ".hero{display:flex;flex-wrap:wrap;gap:16px;padding:22px;border:1px solid var(--border);border-radius:18px;background:linear-gradient(145deg,rgba(255,255,255,0.04),rgba(255,255,255,0.02));box-shadow:var(--shadow);backdrop-filter:blur(10px)}"
  "h1{font-weight:700;font-size:2.4em;line-height:1.1;color:#fff}"
  ".eyebrow{display:inline-block;padding:6px 12px;border-radius:999px;border:1px solid var(--border);background:rgba(255,255,255,0.04);color:var(--muted);font-size:0.85em;margin-bottom:10px}"
  ".lede{color:var(--muted);max-width:640px;font-size:1em}"
  ".status-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px;flex:1}"
  ".chip{border:1px solid var(--border);background:rgba(255,255,255,0.06);padding:12px 14px;border-radius:14px;display:flex;align-items:center;justify-content:space-between;color:var(--text);box-shadow:0 10px 30px rgba(0,0,0,0.35)}"
  ".chip span{font-size:0.92em;color:var(--muted)}"
  ".chip.ok{border-color:rgba(52,211,153,0.4);background:linear-gradient(145deg,rgba(16,185,129,0.25),rgba(16,185,129,0.08))}"
  ".chip.not{border-color:rgba(248,113,113,0.3);background:linear-gradient(145deg,rgba(248,113,113,0.24),rgba(248,113,113,0.08))}"
  ".card{border:1px solid var(--border);background:linear-gradient(160deg,rgba(255,255,255,0.05),rgba(255,255,255,0.02));border-radius:18px;padding:18px;box-shadow:var(--shadow);backdrop-filter:blur(10px);display:flex;flex-direction:column;gap:12px}"
  ".card h2{display:flex;align-items:center;gap:10px;font-size:1.2em;color:#fff;font-weight:700}"
  ".pill{padding:8px 12px;border-radius:999px;background:rgba(255,255,255,0.06);border:1px solid var(--border);font-size:0.9em;color:var(--muted)}"
  ".joy-label{font-weight:700;color:var(--accent);text-transform:uppercase;font-size:0.78em;letter-spacing:1px;margin-top:6px}"
  ".pad{width:220px;height:220px;position:relative;border:1px solid var(--border);border-radius:14px;background:radial-gradient(circle at 50% 50%,rgba(125,224,255,0.15),transparent 60%);box-shadow:0 10px 30px rgba(0,0,0,0.35)}"
  ".pad::before{content:\"\";position:absolute;left:50%;top:0;bottom:0;width:1px;background:rgba(255,255,255,0.12)}"
  ".pad::after{content:\"\";position:absolute;top:50%;left:0;right:0;height:1px;background:rgba(255,255,255,0.12)}"
  ".dot{width:24px;height:24px;border-radius:50%;background:linear-gradient(135deg,#38bdf8,#a78bfa);box-shadow:0 0 0 6px rgba(59,130,246,0.25),0 8px 20px rgba(0,0,0,0.35);position:absolute;pointer-events:none}"
  ".bar{height:26px;background:rgba(255,255,255,0.06);border-radius:12px;overflow:hidden;border:1px solid var(--border)}"
  ".fill{height:100%;display:flex;align-items:center;justify-content:flex-end;padding-right:10px;color:#fff;font-weight:700;font-size:0.85em;background:linear-gradient(90deg,#38bdf8,#a78bfa);box-shadow:0 0 30px rgba(59,130,246,0.35);transition:width 0.35s}"
  ".val{font-size:0.86em;color:var(--muted);text-align:right;margin-top:4px}"
  ".switch-row{display:flex;align-items:center;justify-content:space-between;padding:12px;border-radius:14px;border:1px solid var(--border);background:rgba(255,255,255,0.04)}"
  ".sw{width:64px;height:34px;border-radius:18px;background:rgba(255,255,255,0.14);position:relative;transition:all 0.3s;border:1px solid var(--border)}"
  ".sw::after{content:\"\";width:28px;height:28px;border-radius:50%;background:#fff;position:absolute;top:2px;left:3px;transition:left 0.3s}"
  ".sw.on{background:linear-gradient(135deg,#22c55e,#16a34a)}"
  ".sw.on::after{left:33px}"
  ".footer{margin-top:18px;text-align:center;color:var(--muted)}"
  ".btn{display:inline-flex;align-items:center;gap:8px;padding:12px 18px;border-radius:12px;border:1px solid var(--border);color:#fff;text-decoration:none;background:linear-gradient(145deg,rgba(255,255,255,0.08),rgba(255,255,255,0.03));font-weight:700;box-shadow:var(--shadow)}"
  "@media(max-width:768px){h1{font-size:1.8em}}"
  "</style>"
  "<script>"
  "setInterval(()=>{"
  "fetch(\"/status\").then(r=>r.json()).then(d=>{"
  "const tx=document.getElementById(\"tx\"),rx=document.getElementById(\"rx\"),env=document.getElementById(\"env\");"
  "tx.className=\"chip \"+(d.tx==\"OK\"?\"ok\":\"not\");rx.className=\"chip \"+(d.rx==\"OK\"?\"ok\":\"not\");env.className=\"chip \"+(d.envio==\"OK\"?\"ok\":\"not\");"
  "tx.querySelector(\"strong\").textContent=d.tx;rx.querySelector(\"strong\").textContent=d.rx;env.querySelector(\"strong\").textContent=d.envio;"
  "const j1xp=d.joy1p[0],j1yp=d.joy1p[1],j2xp=d.joy2p[0],j2yp=d.joy2p[1];"
  "const j1dot=document.getElementById(\"j1dot\"),j2dot=document.getElementById(\"j2dot\");"
  "j1dot.style.left='calc(' + j1xp + '% - 12px)';"
  "j1dot.style.top='calc(' + (100 - j1yp) + '% - 12px)';"
  "j2dot.style.left='calc(' + j2xp + '% - 12px)';"
  "j2dot.style.top='calc(' + (100 - j2yp) + '% - 12px)';"
  "document.getElementById(\"j1xv\").textContent=d.joy1[0];document.getElementById(\"j1yv\").textContent=d.joy1[1];"
  "document.getElementById(\"j2xv\").textContent=d.joy2[0];document.getElementById(\"j2yv\").textContent=d.joy2[1];"
  "document.getElementById(\"sw\").className=\"sw \"+(d.chave?\"on\":\"\");"
  "});"
  "},1000);"
  "</script>"
  "</head><body><div class=\"bg\"></div><div class=\"shell\">";

String statusHtml() {
  int j1x = percentAxis(dados.joy1X, calib1.minX, calib1.midX, calib1.maxX);
  int j1y = percentAxis(dados.joy1Y, calib1.minY, calib1.midY, calib1.maxY);
  int j2x = percentAxis(dados.joy2X, calib2.minX, calib2.midX, calib2.maxX);
  int j2y = percentAxis(dados.joy2Y, calib2.minY, calib2.midY, calib2.maxY);
  
  String s;
  s.reserve(2000);
  s = FPSTR(HTML_HEAD);

  s += "<header class=\"hero\"><div>";
  s += "<span class=\"eyebrow\">ESP32-C3 • RF 2.4GHz</span>";
  s += "<h1>ESP32 RF Controller</h1>";
  s += "<p class=\"lede\">Monitoramento em tempo real de joysticks e chave seletora via nRF24L01, servido direto pelo ESP32 em modo AP.</p>";
  s += "</div><div class=\"status-row\">";
  s += "<div class=\"chip " + String(g_tx_ok ? "ok" : "not") + "\" id=\"tx\"><span>Transmissão</span><strong>" + String(g_tx_ok ? "OK" : "NOT") + "</strong></div>";
  s += "<div class=\"chip " + String(g_rx_ready ? "ok" : "not") + "\" id=\"rx\"><span>Recepção</span><strong>" + String(g_rx_ready ? "OK" : "NOT") + "</strong></div>";
  s += "<div class=\"chip " + String(g_envio_ok ? "ok" : "not") + "\" id=\"env\"><span>Status Envio</span><strong>" + String(g_envio_ok ? "OK" : "NOT") + "</strong></div>";
  s += "</div></header>";

  s += "<section class=\"grid\">";

  s += "<div class=\"card\"><h2>🕹️ Joystick 1</h2>";
  s += "<div class=\"pad\" id=\"j1pad\"><div class=\"dot\" id=\"j1dot\" style=\"left:calc(" + String(j1x) + "% - 12px);top:calc(" + String(100 - j1y) + "% - 12px)\"></div></div>";
  s += "<div class=\"val\">X: <span id=\"j1xv\">" + String(dados.joy1X) + "</span> • Y: <span id=\"j1yv\">" + String(dados.joy1Y) + "</span></div></div>";

  s += "<div class=\"card\"><h2>🕹️ Joystick 2</h2>";
  s += "<div class=\"pad\" id=\"j2pad\"><div class=\"dot\" id=\"j2dot\" style=\"left:calc(" + String(j2x) + "% - 12px);top:calc(" + String(100 - j2y) + "% - 12px)\"></div></div>";
  s += "<div class=\"val\">X: <span id=\"j2xv\">" + String(dados.joy2X) + "</span> • Y: <span id=\"j2yv\">" + String(dados.joy2Y) + "</span></div></div>";

  s += "<div class=\"card\"><h2>⚙️ Controles</h2>";
  s += "<div class=\"switch-row\"><div><div class=\"pill\">Chave seletora</div><div class=\"val\">Estado remoto</div></div><div class=\"sw " + String(dados.chave ? "on" : "") + "\" id=\"sw\"></div></div>";
  s += "<div class=\"pill\">AP: " + String(AP_SSID) + "</div>";
  s += "<div class=\"footer\"><a href=\"/calib/center\" class=\"btn\">🧭 Calibrar Centro</a></div>";
  s += "</div>";

  s += "</section>";
  s += "<div class=\"footer\"><a href=\"/status\" class=\"btn\">📊 Ver JSON</a></div></div></body></html>";
  return s;
}

String statusJson() {
  String s;
  s.reserve(200);
  s = "{\"tx\":\"";
  s += g_tx_ok ? "OK" : "NOT";
  s += "\",\"rx\":\"";
  s += g_rx_ready ? "OK" : "NOT";
  s += "\",\"envio\":\"";
  s += g_envio_ok ? "OK" : "NOT";
  s += "\",\"joy1\":[";
  s += String(dados.joy1X) + "," + String(dados.joy1Y);
  s += "],\"joy2\":[";
  s += String(dados.joy2X) + "," + String(dados.joy2Y);
  s += "],\"chave\":";
  s += dados.chave ? "1" : "0";
  s += ",\"joy1p\":[";
  s += String(percentAxis(dados.joy1X, calib1.minX, calib1.midX, calib1.maxX));
  s += ",";
  s += String(percentAxis(dados.joy1Y, calib1.minY, calib1.midY, calib1.maxY));
  s += "],\"joy2p\":[";
  s += String(percentAxis(dados.joy2X, calib2.minX, calib2.midX, calib2.maxX));
  s += ",";
  s += String(percentAxis(dados.joy2Y, calib2.minY, calib2.midY, calib2.maxY));
  s += "]";
  s += "}";
  return s;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  // Aguarda porta serial abrir, mas não trava se não houver porta nativa
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 1500) {}
  delay(100);
  Serial.println("Boot...");

  pinMode(CHAVE_PIN, INPUT_PULLUP);

  // SPI personalizado (ESP32-C3)
  // Informe também o pino SS (CSN) para evitar conflitos no ESP32-C3
  SPI.begin(9, 8, 7, CSN_PIN); // SCK, MISO, MOSI, SS

#ifdef ESP32
  analogReadResolution(12); // Leitura de 0-4095 para mapeamento consistente
#endif

  // Inicializa RF24 com diagnósticos
  bool rfOk = radio.begin();
  if (!rfOk) {
    Serial.println("RF24 não detectado (begin falhou)");
  }

  // Checagem adicional do chip
  if (!radio.isChipConnected()) {
    Serial.println("RF24 não detectado (chip não responde)");
    Serial.print("Pinos -> CE:"); Serial.print(CE_PIN);
    Serial.print(" CSN:"); Serial.print(CSN_PIN);
    Serial.print(" MOSI:"); Serial.print(7);
    Serial.print(" MISO:"); Serial.print(8);
    Serial.print(" SCK:"); Serial.println(9);
    Serial.println("Verifique alimentação 3V3, GND comum e capacitor de desacoplamento no módulo (10-100uF).");
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(108);
  radio.setRetries(5, 15); // tentativas e atraso de reenvio
  // Desliga auto-ack para permitir teste de transmissão sem receptor
  radio.setAutoAck(false);
  radio.openWritingPipe(endereco);
  radio.stopListening();

  Serial.println("Transmissor pronto");
  // Opcional: imprime detalhes do rádio (útil para depuração)
  radio.printDetails();

  // Self-test simples sem receptor: entra em RX brevemente e lê RPD
  radio.startListening();
  delay(2);
  bool rpd = radio.testRPD();
  radio.stopListening();
  Serial.print("Self-test RPD (energia no canal): ");
  Serial.println(rpd ? "SIM" : "NAO");

  // ================= WIFI/WEB =================
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(500);
  IPAddress IP = WiFi.softAPIP();
  Serial.println();
  Serial.println("========================================");
  Serial.print("Rede Wi-Fi criada: "); Serial.println(AP_SSID);
  Serial.print("Senha: "); Serial.println(AP_PASS);
  Serial.print("IP do ESP32: "); Serial.println(IP);
  Serial.println("Conecte seu celular/PC a esta rede");
  Serial.print("Acesse: http://"); Serial.println(IP);
  Serial.println("========================================");

  server.on("/", [](){ server.send(200, "text/html", statusHtml()); });
  server.on("/status", [](){ server.send(200, "application/json", statusJson()); });
  server.on("/calib/center", [](){
    calib1.midX = dados.joy1X; calib1.midY = dados.joy1Y;
    calib2.midX = dados.joy2X; calib2.midY = dados.joy2Y;
    server.send(200, "text/plain", "OK");
    Serial.println("Calibracao centro aplicada (joy1 & joy2)");
  });
  server.begin();
  Serial.println("WebServer iniciado em / e /status");
}

// ================= LOOP =================
void loop() {
  // Joysticks
  dados.joy1X = analogRead(JOY1_X);
  dados.joy1Y = analogRead(JOY1_Y);
  dados.joy2X = analogRead(JOY2_X);
  dados.joy2Y = analogRead(JOY2_Y);

  // Potenciômetro por níveis
  // (Removido)

  // Chave seletora
  dados.chave = !digitalRead(CHAVE_PIN);

  // Limpa IRQs pendentes antes do envio
  {
    bool _tx=false, _fail=false, _rx=false;
    radio.whatHappened(_tx, _fail, _rx);
    if (_rx) radio.flush_rx();
  }

  // Envio RF
  (void)radio.write(&dados, sizeof(dados));
  bool tx_ok=false, tx_fail=false, rx_ready=false;
  radio.whatHappened(tx_ok, tx_fail, rx_ready);
  // Atualiza status globais
  g_tx_ok = tx_ok;
  g_tx_fail = tx_fail;
  g_rx_ready = rx_ready;
  g_envio_ok = (tx_ok && !tx_fail);

  // Debug
  Serial.print("J1: ");
  Serial.print(dados.joy1X); Serial.print(",");
  Serial.print(dados.joy1Y);
  Serial.print(" | J2: ");
  Serial.print(dados.joy2X); Serial.print(",");
  Serial.print(dados.joy2Y);
  Serial.print(" | Chave: ");
  Serial.print(dados.chave);
  // Status simplificado
  Serial.print(" | TX="); Serial.print(tx_ok ? "OK" : "NOT");
  Serial.print(" | RX="); Serial.print(rx_ready ? "OK" : "NOT");
  Serial.print(" | ENVIO="); Serial.println((tx_ok && !tx_fail) ? "OK" : "NOT");
  // Detalhes (opcional)
  Serial.print("  IRQ tx_ok:"); Serial.print(tx_ok);
  Serial.print(" tx_fail:"); Serial.print(tx_fail);
  Serial.print(" rx_ready:"); Serial.println(rx_ready);

  delay(20);
  // Atende requisições HTTP
  server.handleClient();
}
