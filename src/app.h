// Funzioni dell'applicazione usate dalla pagina web e dai comandi Telegram.
// Sono implementate in main.cpp e sono sicure rispetto al task di rilevamento.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

#include "FallDetector.h"

void appStatusJson(JsonObject out);
void appAcknowledge(const String& who);   // "Sto bene" / allarme gestito
void appTestAlarm(const String& who);     // invia un allarme di prova
void appRelearnBackground();
void appPause(uint32_t minutes);
void appResume();
bool appSnapshot(std::vector<uint8_t>& jpg, uint32_t timeoutMs = 2000);
// Ultimo frame 80x60 in grigio seguito dalla maschera 80x60 (per addestrare la CNN).
bool appFrameRaw(std::vector<uint8_t>& out);
fall::Params appGetParams();
void appSetParams(const fall::Params& p, bool persist);
