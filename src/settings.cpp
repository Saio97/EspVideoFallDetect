#include "settings.h"

#include <Preferences.h>

namespace {
constexpr const char* kNamespace = "falldet";
constexpr uint32_t kVersion = 1;

template <typename T>
void clampField(JsonObjectConst in, const char* key, T& field, T lo, T hi) {
  if (in[key].isNull()) return;
  T v = in[key].as<T>();
  field = v < lo ? lo : v > hi ? hi : v;
}
}  // namespace

fall::Params settingsLoad() {
  fall::Params p;
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return p;
  if (prefs.getUInt("ver", 0) == kVersion && prefs.getBytesLength("params") == sizeof(p)) {
    prefs.getBytes("params", &p, sizeof(p));
  }
  prefs.end();
  return p;
}

void settingsSave(const fall::Params& p) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return;
  prefs.putUInt("ver", kVersion);
  prefs.putBytes("params", &p, sizeof(p));
  prefs.end();
}

void settingsClear() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return;
  prefs.clear();
  prefs.end();
}

void paramsToJson(const fall::Params& p, JsonObject out) {
  out["diffThreshold"] = p.diffThreshold;
  out["minBlobFraction"] = p.minBlobFraction;
  out["fallWindowMs"] = p.fallWindowMs;
  out["standingAspect"] = p.standingAspect;
  out["lyingAspect"] = p.lyingAspect;
  out["fallDropFraction"] = p.fallDropFraction;
  out["groundTimeMs"] = p.groundTimeMs;
  out["recoverTopFraction"] = p.recoverTopFraction;
}

void paramsFromJson(JsonObjectConst in, fall::Params& p) {
  clampField<uint8_t>(in, "diffThreshold", p.diffThreshold, 8, 120);
  clampField<float>(in, "minBlobFraction", p.minBlobFraction, 0.001f, 0.2f);
  clampField<uint32_t>(in, "fallWindowMs", p.fallWindowMs, 300, 5000);
  clampField<float>(in, "standingAspect", p.standingAspect, 0.8f, 4.0f);
  clampField<float>(in, "lyingAspect", p.lyingAspect, 0.2f, 2.0f);
  clampField<float>(in, "fallDropFraction", p.fallDropFraction, 0.05f, 0.8f);
  clampField<uint32_t>(in, "groundTimeMs", p.groundTimeMs, 3000, 300000);
  clampField<float>(in, "recoverTopFraction", p.recoverTopFraction, 0.02f, 0.5f);
}
