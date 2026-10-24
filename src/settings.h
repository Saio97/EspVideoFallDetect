// Salvataggio delle soglie dell'algoritmo nella memoria flash (NVS).
#pragma once

#include <ArduinoJson.h>

#include "FallDetector.h"

fall::Params settingsLoad();
void settingsSave(const fall::Params& p);
void settingsClear();

void paramsToJson(const fall::Params& p, JsonObject out);
// Applica solo i campi presenti, limitandoli a intervalli sensati.
void paramsFromJson(JsonObjectConst in, fall::Params& p);
