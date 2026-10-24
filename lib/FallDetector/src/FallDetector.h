// FallDetector: rilevamento cadute su immagini in scala di grigi a bassa risoluzione.
//
// Libreria C++ pura (nessuna dipendenza Arduino), così può essere testata sul PC
// con `pio test -e native`.
//
// Pipeline per ogni frame:
//   1. sottrazione dello sfondo (modello a media mobile, aggiornamento selettivo)
//   2. maschera di primo piano -> riquadro robusto (percentili) e baricentro
//   3. storico breve di (alto del riquadro, baricentro, rapporto altezza/larghezza)
//   4. macchina a stati:
//        MONITORING  -> FALL_SUSPECTED  se, entro una finestra breve, la sagoma passa
//                                       da verticale a orizzontale E si abbassa molto
//        FALL_SUSPECTED -> ALARM        se la persona resta a terra per groundTimeMs
//        FALL_SUSPECTED -> MONITORING   se la persona si rialza (evento RECOVERED)
//        ALARM -> MONITORING            solo con reset() esplicito (pulsante, Telegram, web)
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace fall {

struct Params {
  uint8_t diffThreshold = 28;        // differenza minima pixel/sfondo per il primo piano
  float bgLearnRate = 0.05f;         // apprendimento sfondo per pixel di sfondo
  float bgLearnRateFg = 0.002f;      // apprendimento sfondo per pixel di primo piano
  float minBlobFraction = 0.008f;    // frazione minima di pixel per considerare una persona
  float globalChangeReset = 0.55f;   // oltre questa frazione: cambio luce -> reset sfondo
  uint32_t warmupMs = 3000;          // tempo di apprendimento iniziale dello sfondo

  uint32_t fallWindowMs = 1500;      // la transizione deve avvenire entro questo tempo
  float standingAspect = 1.25f;      // altezza/larghezza minima "in piedi"
  float lyingAspect = 0.85f;         // altezza/larghezza massima "a terra"
  float fallDropFraction = 0.20f;    // abbassamento minimo dell'alto della sagoma (frazione H)
  uint32_t groundTimeMs = 15000;     // permanenza a terra prima dell'allarme
  float recoverTopFraction = 0.12f;  // tolleranza per considerare la persona rialzata
};

enum class State : uint8_t { WARMUP, MONITORING, FALL_SUSPECTED, ALARM };
enum class Event : uint8_t { NONE, FALL_SUSPECTED, ALARM, RECOVERED, BACKGROUND_RESET };

const char* stateName(State s);
const char* eventName(Event e);

struct Box {
  int16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // estremi inclusi
  int16_t width() const { return x1 - x0 + 1; }
  int16_t height() const { return y1 - y0 + 1; }
};

struct Status {
  State state = State::WARMUP;
  bool personPresent = false;
  Box box;
  float aspect = 0.0f;          // altezza / larghezza del riquadro
  float centroidY = 0.0f;       // in pixel
  float fgFraction = 0.0f;      // frazione di pixel di primo piano
  float motion = 0.0f;          // frazione di pixel cambiati rispetto al frame precedente
  uint32_t stateSinceMs = 0;    // istante di ingresso nello stato attuale
  uint32_t frames = 0;
};

class FallDetector {
 public:
  FallDetector() = default;
  ~FallDetector();
  FallDetector(const FallDetector&) = delete;
  FallDetector& operator=(const FallDetector&) = delete;

  // Alloca i buffer per immagini width x height. Restituisce false se manca memoria.
  bool begin(uint16_t width, uint16_t height, const Params& params = Params());

  // Elabora un frame in scala di grigi (width*height byte, riga per riga).
  Event update(const uint8_t* gray, uint32_t nowMs);

  // Torna al monitoraggio (dopo un allarme gestito o un falso allarme).
  void reset(uint32_t nowMs);

  // Forza un nuovo apprendimento dello sfondo.
  void relearnBackground(uint32_t nowMs);

  const Status& status() const { return status_; }
  const Params& params() const { return params_; }
  void setParams(const Params& p) { params_ = p; }

  // Maschera di primo piano dell'ultimo frame (1 = primo piano), utile per debug.
  const uint8_t* mask() const { return mask_; }
  uint16_t width() const { return w_; }
  uint16_t height() const { return h_; }

 private:
  struct Sample {
    uint32_t t;
    float top;
    float centroidY;
    float aspect;
  };
  static constexpr size_t kHistory = 48;

  void release();
  void setState(State s, uint32_t nowMs);
  void pushSample(const Sample& s);
  bool detectFallTransition(uint32_t nowMs, float* preFallTop) const;
  bool isRecovered() const;

  Params params_;
  uint16_t w_ = 0, h_ = 0;
  float* bg_ = nullptr;
  uint8_t* prev_ = nullptr;
  uint8_t* mask_ = nullptr;
  uint16_t* rowCount_ = nullptr;
  uint16_t* colCount_ = nullptr;
  bool hasPrev_ = false;

  Sample history_[kHistory];
  size_t histHead_ = 0;
  size_t histCount_ = 0;

  float preFallTop_ = 0.0f;
  uint32_t warmupStartMs_ = 0;
  Status status_;
};

}  // namespace fall
