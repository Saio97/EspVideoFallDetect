// Rilevatore di cadute per anziani - Seeed Studio XIAO ESP32S3 Sense.
//
// Task di rilevamento (core 0): acquisisce frame dalla telecamera, li riduce a
// 80x60 e li passa a fall::FallDetector. Gli eventi finiscono in una coda.
// loop() (core 1): Wi-Fi, pagina web, Telegram, pulsante, buzzer e LED.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <img_converters.h>

#include "FallDetector.h"
#include "app.h"
#include "camera.h"
#include "config.h"
#include "settings.h"
#include "telegram.h"
#include "web.h"
#include "posture_cnn.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h mancante: uso secrets.example.h (copialo e inserisci i tuoi dati)"
#include "secrets.example.h"
#endif

namespace {

fall::FallDetector detector;
TelegramBot telegram;
SemaphoreHandle_t detMutex;   // protegge detector e i buffer JPEG
QueueHandle_t eventQueue;

// Richieste dal loop al task di rilevamento.
volatile bool snapshotRequested = false;
SemaphoreHandle_t snapshotReady;
std::vector<uint8_t> snapshotJpeg;
std::vector<uint8_t> alarmJpeg;   // foto scattata al momento dell'allarme
uint8_t small[DET_W * DET_H];     // ultimo frame 80x60 analizzato (protetto da detMutex)
// Postura del frame precedente, data dal classificatore CNN (se compilato).
fall::Posture cnnPosture = fall::Posture::UNKNOWN;

volatile uint32_t pausedUntilMs = 0;
volatile bool paused = false;

// Stato degli allarmi (solo loop).
bool alarmActive = false;
bool suspectedNotified = false;
uint32_t lastAlarmSentMs = 0;
uint32_t alarmStartMs = 0;
uint32_t alarmCount = 0;

// ------------------------------------------------------------------ utilità

void setLed(bool on) { digitalWrite(PIN_LED, (on ^ LED_ACTIVE_LOW) ? HIGH : LOW); }

void setBuzzer(bool on) {
  if (PIN_BUZZER >= 0) digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

bool encodeJpeg(camera_fb_t* fb, std::vector<uint8_t>& out) {
  uint8_t* buf = nullptr;
  size_t len = 0;
  if (!frame2jpg(fb, JPEG_QUALITY, &buf, &len)) return false;
  out.assign(buf, buf + len);
  free(buf);
  return true;
}

String uptimeString() {
  const uint32_t s = millis() / 1000;
  char buf[32];
  snprintf(buf, sizeof buf, "%lug %02luh %02lum", s / 86400, (s / 3600) % 24, (s / 60) % 60);
  return buf;
}

String stateLabel(fall::State s) {
  switch (s) {
    case fall::State::WARMUP: return "avvio (apprendimento ambiente)";
    case fall::State::MONITORING: return "sorveglianza attiva";
    case fall::State::FALL_SUSPECTED: return "possibile caduta, in verifica";
    case fall::State::ALARM: return "ALLARME CADUTA";
  }
  return "?";
}

// ------------------------------------------------------- task di rilevamento

void detectTask(void*) {
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(1000 / DETECT_FPS);
  bool wasPaused = false;

  for (;;) {
    vTaskDelayUntil(&last, period);
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) continue;
    if (fb->width != CAM_W || fb->height != CAM_H || fb->format != PIXFORMAT_GRAYSCALE) {
      esp_camera_fb_return(fb);
      continue;
    }
    const uint32_t now = millis();
    fall::Event ev = fall::Event::NONE;

    xSemaphoreTake(detMutex, portMAX_DELAY);
    if (paused) {
      wasPaused = true;
    } else {
      if (wasPaused) {
        detector.relearnBackground(now);
        wasPaused = false;
      }
      downsampleGray(fb->buf, fb->width, fb->height, small);
      // La CNN lavora sulla maschera del frame appena elaborato e il suo
      // risultato vale per il frame successivo (ritardo di 1/DETECT_FPS s).
      detector.setPosture(cnnPosture);
      ev = detector.update(small, now);
      if (posture::available() && detector.status().state != fall::State::WARMUP) {
        cnnPosture = detector.status().personPresent
                         ? posture::classify(small, detector.mask(), DET_W, DET_H, CNN_MIN_CONFIDENCE).posture
                         : fall::Posture::UNKNOWN;
      }
      if (ev == fall::Event::ALARM) encodeJpeg(fb, alarmJpeg);
    }
    if (snapshotRequested) {
      encodeJpeg(fb, snapshotJpeg);
      snapshotRequested = false;
      xSemaphoreGive(snapshotReady);
    }
    xSemaphoreGive(detMutex);

    esp_camera_fb_return(fb);
    if (ev != fall::Event::NONE) xQueueSend(eventQueue, &ev, 0);
  }
}

// ------------------------------------------------------------------ allarmi

void sendAlarm(bool repeat) {
  setBuzzer(false);
  String text = repeat ? "🚨 ALLARME ANCORA ATTIVO: nessuno ha risposto.\n"
                       : "🚨 ALLARME: probabile caduta rilevata!\n";
  text += String("Dispositivo: ") + DEVICE_NAME + "\n";
  text += "La persona è a terra da almeno " + String(appGetParams().groundTimeMs / 1000) +
          " secondi.\n";
  text += "Rispondi /ok quando la situazione è gestita.";

  bool sent = false;
  if (SEND_PHOTO_WITH_ALARM && !repeat) {
    std::vector<uint8_t> photo;
    xSemaphoreTake(detMutex, portMAX_DELAY);
    photo = alarmJpeg;
    xSemaphoreGive(detMutex);
    if (!photo.empty()) sent = telegram.broadcastPhoto(photo.data(), photo.size(), text);
  }
  if (!sent) sent = telegram.broadcast(text);
  Serial.printf("[alarm] notifica %s\n", sent ? "inviata" : "NON inviata");
  lastAlarmSentMs = millis();
}

void handleEvent(fall::Event ev) {
  Serial.printf("[det] evento %s\n", fall::eventName(ev));
  switch (ev) {
    case fall::Event::FALL_SUSPECTED:
      if (NOTIFY_ON_SUSPECTED) {
        telegram.broadcast("⚠️ Possibile caduta rilevata, verifico se la persona si rialza...");
        suspectedNotified = true;
      }
      break;
    case fall::Event::RECOVERED:
      if (suspectedNotified) telegram.broadcast("✅ La persona si è rialzata. Nessun allarme.");
      suspectedNotified = false;
      break;
    case fall::Event::ALARM:
      alarmActive = true;
      alarmStartMs = millis();
      ++alarmCount;
      sendAlarm(false);
      break;
    case fall::Event::BACKGROUND_RESET:
    case fall::Event::NONE:
      break;
  }
}

// Pattern di LED e buzzer in base allo stato.
void updateIndicators(fall::State state) {
  const uint32_t now = millis();
  bool led = false, buzz = false;
  if (alarmActive) {
    led = true;
    const uint32_t since = now - alarmStartMs;
    // Primo minuto: suono intermittente continuo; poi un bip ogni 10 s.
    buzz = since < 60000 ? (now / 500) % 2 : (now % 10000) < 300;
  } else if (state == fall::State::FALL_SUSPECTED) {
    led = (now / 150) % 2;
    buzz = (now % 2000) < 120;  // bip breve: "premi il pulsante se stai bene"
  } else if (paused) {
    led = (now % 3000) < 80;
  } else {
    led = (now % 5000) < 40;  // battito: dispositivo vivo
  }
  setLed(led);
  setBuzzer(buzz);
}

void handleButton() {
  static bool lastLevel = HIGH;
  static uint32_t lastChange = 0;
  const bool level = digitalRead(PIN_BUTTON);
  if (level != lastLevel && millis() - lastChange > 40) {
    lastChange = millis();
    lastLevel = level;
    if (level == LOW) appAcknowledge("pulsante sul dispositivo");
  }
}

// ----------------------------------------------------------------- Telegram

void handleTelegramCommand(const String& chatId, const String& from, const String& textIn) {
  String text = textIn;
  text.trim();
  const int at = text.indexOf('@');  // "/stato@NomeBot" nei gruppi
  String cmd = text, arg;
  const int sp = text.indexOf(' ');
  if (sp > 0) {
    cmd = text.substring(0, sp);
    arg = text.substring(sp + 1);
  }
  if (at > 0 && (sp < 0 || at < sp)) cmd = text.substring(0, at);
  cmd.toLowerCase();

  if (cmd == "/start" || cmd == "/id") {
    telegram.sendMessage(chatId, "ID di questa chat: " + chatId +
                                     (telegram.isAuthorized(chatId)
                                          ? "\nChat autorizzata: riceverai gli allarmi."
                                          : "\nAggiungi questo ID a TELEGRAM_CHAT_IDS in secrets.h."));
    return;
  }
  if (!telegram.isAuthorized(chatId)) return;

  const String who = from.length() ? from : String("Telegram");
  if (cmd == "/ok") {
    appAcknowledge(who);
  } else if (cmd == "/stato") {
    JsonDocument doc;
    appStatusJson(doc.to<JsonObject>());
    String msg = "Stato: " + doc["stateLabel"].as<String>();
    msg += "\nPersona inquadrata: " + String(doc["personPresent"].as<bool>() ? "sì" : "no");
    msg += "\nWi-Fi: " + String(WiFi.RSSI()) + " dBm, IP " + WiFi.localIP().toString();
    msg += "\nAcceso da: " + uptimeString();
    msg += "\nAllarmi dall'avvio: " + String(alarmCount);
    telegram.sendMessage(chatId, msg);
  } else if (cmd == "/foto") {
    std::vector<uint8_t> jpg;
    if (appSnapshot(jpg)) telegram.sendPhoto(chatId, jpg.data(), jpg.size(), "Foto richiesta da " + who);
    else telegram.sendMessage(chatId, "Impossibile scattare la foto.");
  } else if (cmd == "/pausa") {
    const long min = arg.length() ? arg.toInt() : 30;
    appPause(min > 0 ? min : 30);
    telegram.broadcast("⏸️ Sorveglianza in pausa per " + String(min > 0 ? min : 30) + " minuti (" + who + ").");
  } else if (cmd == "/riprendi") {
    appResume();
    telegram.broadcast("▶️ Sorveglianza ripresa (" + who + ").");
  } else if (cmd == "/test") {
    appTestAlarm(who);
  } else {
    telegram.sendMessage(chatId,
                         "Comandi:\n/stato - stato del dispositivo\n/foto - scatta una foto\n"
                         "/ok - allarme gestito / falso allarme\n/pausa [min] - sospendi la sorveglianza\n"
                         "/riprendi - riprendi la sorveglianza\n/test - allarme di prova\n/id - ID della chat");
  }
}

// --------------------------------------------------------------------- Wi-Fi

void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connessione a %s", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connesso, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] non connesso: il rilevamento funziona comunque, riprovo in background");
  }
}

}  // namespace

// ------------------------------------------------------------- API per web.cpp

void appStatusJson(JsonObject out) {
  fall::Status st;
  xSemaphoreTake(detMutex, portMAX_DELAY);
  st = detector.status();
  xSemaphoreGive(detMutex);
  out["state"] = fall::stateName(st.state);
  out["stateLabel"] = paused ? String("in pausa") : stateLabel(st.state);
  out["alarmActive"] = alarmActive;
  out["paused"] = paused;
  out["pausedForS"] = paused ? (pausedUntilMs - millis()) / 1000 : 0;
  out["personPresent"] = st.personPresent;
  out["aspect"] = st.aspect;
  out["fgFraction"] = st.fgFraction;
  out["motion"] = st.motion;
  JsonArray box = out["box"].to<JsonArray>();
  box.add(st.box.x0);
  box.add(st.box.y0);
  box.add(st.box.x1);
  box.add(st.box.y1);
  out["inStateS"] = (millis() - st.stateSinceMs) / 1000;
  out["frames"] = st.frames;
  out["alarmCount"] = alarmCount;
  out["uptime"] = uptimeString();
  out["rssi"] = WiFi.RSSI();
  out["telegram"] = telegram.configured();
  out["detW"] = DET_W;
  out["detH"] = DET_H;
  out["cnn"] = posture::available();
  if (posture::available()) {
    const posture::Result& r = posture::last();
    out["posture"] = fall::postureName(st.posture);
    out["postureConfidence"] = r.confidence;
    out["inferenceMs"] = r.inferenceUs / 1000.0f;
  }
}

void appAcknowledge(const String& who) {
  xSemaphoreTake(detMutex, portMAX_DELAY);
  const fall::State s = detector.status().state;
  detector.reset(millis());
  xSemaphoreGive(detMutex);
  if (alarmActive) {
    telegram.broadcast("✅ Allarme gestito da " + who + ".");
  } else if (s == fall::State::FALL_SUSPECTED && suspectedNotified) {
    telegram.broadcast("✅ Falso allarme: " + who + " ha confermato che va tutto bene.");
  }
  alarmActive = false;
  suspectedNotified = false;
  Serial.printf("[app] reset da %s\n", who.c_str());
}

void appTestAlarm(const String& who) {
  std::vector<uint8_t> jpg;
  const String caption = "🧪 Allarme di PROVA richiesto da " + who + ". Le notifiche funzionano.";
  if (SEND_PHOTO_WITH_ALARM && appSnapshot(jpg)) telegram.broadcastPhoto(jpg.data(), jpg.size(), caption);
  else telegram.broadcast(caption);
}

void appRelearnBackground() {
  xSemaphoreTake(detMutex, portMAX_DELAY);
  detector.relearnBackground(millis());
  xSemaphoreGive(detMutex);
}

void appPause(uint32_t minutes) {
  pausedUntilMs = millis() + minutes * 60000UL;
  paused = true;
}

void appResume() { paused = false; }

bool appSnapshot(std::vector<uint8_t>& jpg, uint32_t timeoutMs) {
  xSemaphoreTake(snapshotReady, 0);  // scarta segnali vecchi
  snapshotRequested = true;
  if (xSemaphoreTake(snapshotReady, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) return false;
  xSemaphoreTake(detMutex, portMAX_DELAY);
  jpg.swap(snapshotJpeg);
  snapshotJpeg.clear();
  xSemaphoreGive(detMutex);
  return !jpg.empty();
}

bool appFrameRaw(std::vector<uint8_t>& out) {
  xSemaphoreTake(detMutex, portMAX_DELAY);
  const bool ok = detector.status().frames > 0;
  if (ok) {
    out.assign(small, small + DET_W * DET_H);
    out.insert(out.end(), detector.mask(), detector.mask() + DET_W * DET_H);
  }
  xSemaphoreGive(detMutex);
  return ok;
}

fall::Params appGetParams() {
  xSemaphoreTake(detMutex, portMAX_DELAY);
  fall::Params p = detector.params();
  xSemaphoreGive(detMutex);
  return p;
}

void appSetParams(const fall::Params& p, bool persist) {
  xSemaphoreTake(detMutex, portMAX_DELAY);
  detector.setParams(p);
  xSemaphoreGive(detMutex);
  if (persist) settingsSave(p);
}

// ------------------------------------------------------------- setup / loop

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== " DEVICE_NAME " - XIAO ESP32S3 Sense ===");

  pinMode(PIN_LED, OUTPUT);
  setLed(false);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  if (PIN_BUZZER >= 0) {
    pinMode(PIN_BUZZER, OUTPUT);
    setBuzzer(false);
  }

  detMutex = xSemaphoreCreateMutex();
  snapshotReady = xSemaphoreCreateBinary();
  eventQueue = xQueueCreate(8, sizeof(fall::Event));

  // Tenere premuto il pulsante all'avvio ripristina le soglie predefinite.
  if (digitalRead(PIN_BUTTON) == LOW) {
    settingsClear();
    Serial.println("[cfg] soglie riportate ai valori predefiniti");
  }

  if (!cameraBegin() || !detector.begin(DET_W, DET_H, settingsLoad())) {
    Serial.println("[err] inizializzazione fallita, riavvio tra 10 s");
    for (int i = 0; i < 100; ++i) {
      setLed(i % 2);
      delay(100);
    }
    ESP.restart();
  }

#ifdef USE_POSTURE_CNN
  if (posture::begin()) Serial.println("[cnn] classificatore di postura attivo");
  else Serial.println("[cnn] classificatore non disponibile: uso il rapporto altezza/larghezza");
#endif

  wifiBegin();
  if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
  telegram.begin(TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_IDS);
  webBegin();

  xTaskCreatePinnedToCore(detectTask, "detect", 8192, nullptr, 2, nullptr, 0);

  if (WiFi.status() == WL_CONNECTED) {
    telegram.broadcast(String("🟢 ") + DEVICE_NAME + " avviato. Pagina web: http://" +
                       WiFi.localIP().toString() + " (o http://" MDNS_HOSTNAME ".local)");
  }
}

void loop() {
  fall::Event ev;
  while (xQueueReceive(eventQueue, &ev, 0) == pdTRUE) handleEvent(ev);

  if (paused && static_cast<int32_t>(millis() - pausedUntilMs) >= 0) {
    paused = false;
    telegram.broadcast("▶️ Fine pausa: sorveglianza ripresa.");
  }

  if (alarmActive && millis() - lastAlarmSentMs >= ALARM_REPEAT_MS) sendAlarm(true);

  handleButton();
  xSemaphoreTake(detMutex, portMAX_DELAY);
  const fall::State state = detector.status().state;
  xSemaphoreGive(detMutex);
  updateIndicators(state);

  webLoop();

  static uint32_t lastPoll = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastPoll >= TELEGRAM_POLL_MS) {
    lastPoll = millis();
    setBuzzer(false);  // la richiesta HTTPS blocca il loop per qualche istante
    telegram.poll(handleTelegramCommand);
  }

  // Notifica la riconnessione dopo un'interruzione del Wi-Fi.
  static bool wasConnected = WiFi.status() == WL_CONNECTED;
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    Serial.println("[wifi] riconnesso");
    if (alarmActive) sendAlarm(true);
  }
  wasConnected = connected;

  delay(10);
}
