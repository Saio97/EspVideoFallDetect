#include "FallDetector.h"

#include <stdlib.h>
#include <string.h>

namespace fall {

namespace {

// Indice del primo bin in cui la somma cumulativa supera `target`.
int percentileIndex(const uint16_t* counts, int n, uint32_t target) {
  uint32_t acc = 0;
  for (int i = 0; i < n; ++i) {
    acc += counts[i];
    if (acc > target) return i;
  }
  return n - 1;
}

}  // namespace

const char* stateName(State s) {
  switch (s) {
    case State::WARMUP: return "WARMUP";
    case State::MONITORING: return "MONITORING";
    case State::FALL_SUSPECTED: return "FALL_SUSPECTED";
    case State::ALARM: return "ALARM";
  }
  return "?";
}

const char* eventName(Event e) {
  switch (e) {
    case Event::NONE: return "NONE";
    case Event::FALL_SUSPECTED: return "FALL_SUSPECTED";
    case Event::ALARM: return "ALARM";
    case Event::RECOVERED: return "RECOVERED";
    case Event::BACKGROUND_RESET: return "BACKGROUND_RESET";
  }
  return "?";
}

const char* postureName(Posture p) {
  switch (p) {
    case Posture::UNKNOWN: return "UNKNOWN";
    case Posture::NONE: return "NONE";
    case Posture::STANDING: return "STANDING";
    case Posture::BENT: return "BENT";
    case Posture::LYING: return "LYING";
  }
  return "?";
}

FallDetector::~FallDetector() { release(); }

void FallDetector::release() {
  free(bg_);
  free(prev_);
  free(mask_);
  free(rowCount_);
  free(colCount_);
  bg_ = nullptr;
  prev_ = nullptr;
  mask_ = nullptr;
  rowCount_ = nullptr;
  colCount_ = nullptr;
}

bool FallDetector::begin(uint16_t width, uint16_t height, const Params& params) {
  release();
  w_ = width;
  h_ = height;
  params_ = params;
  const size_t n = static_cast<size_t>(w_) * h_;
  bg_ = static_cast<float*>(malloc(n * sizeof(float)));
  prev_ = static_cast<uint8_t*>(malloc(n));
  mask_ = static_cast<uint8_t*>(malloc(n));
  rowCount_ = static_cast<uint16_t*>(malloc(h_ * sizeof(uint16_t)));
  colCount_ = static_cast<uint16_t*>(malloc(w_ * sizeof(uint16_t)));
  if (!bg_ || !prev_ || !mask_ || !rowCount_ || !colCount_) {
    release();
    return false;
  }
  memset(mask_, 0, n);
  status_ = Status();
  hasPrev_ = false;
  histHead_ = histCount_ = 0;
  return true;
}

void FallDetector::setState(State s, uint32_t nowMs) {
  status_.state = s;
  status_.stateSinceMs = nowMs;
}

void FallDetector::relearnBackground(uint32_t nowMs) {
  hasPrev_ = false;
  histHead_ = histCount_ = 0;
  warmupStartMs_ = nowMs;
  // Un allarme in corso non viene mai cancellato da un cambio di luce.
  if (status_.state != State::ALARM && status_.state != State::FALL_SUSPECTED) {
    setState(State::WARMUP, nowMs);
  }
}

void FallDetector::reset(uint32_t nowMs) {
  histHead_ = histCount_ = 0;
  setState(hasPrev_ ? State::MONITORING : State::WARMUP, nowMs);
}

void FallDetector::pushSample(const Sample& s) {
  history_[histHead_] = s;
  histHead_ = (histHead_ + 1) % kHistory;
  if (histCount_ < kHistory) ++histCount_;
}

// Cerca nello storico recente un campione "in piedi" seguito, entro fallWindowMs,
// dal campione attuale "a terra" e notevolmente più in basso.
bool FallDetector::detectFallTransition(uint32_t nowMs, float* preFallTop) const {
  if (histCount_ < 2) return false;
  const Sample& cur = history_[(histHead_ + kHistory - 1) % kHistory];
  if (!cur.lying) return false;

  const float minDrop = params_.fallDropFraction * h_;
  for (size_t i = 1; i < histCount_; ++i) {
    const Sample& s = history_[(histHead_ + kHistory - 1 - i) % kHistory];
    if (nowMs - s.t > params_.fallWindowMs) break;
    if (s.standing && (cur.top - s.top) >= minDrop) {
      *preFallTop = s.top;
      return true;
    }
  }
  return false;
}

bool FallDetector::isRecovered() const {
  if (!status_.personPresent) return false;
  const float tol = params_.recoverTopFraction * h_;
  return status_.standing && status_.box.y0 <= preFallTop_ + tol;
}

Event FallDetector::update(const uint8_t* gray, uint32_t nowMs) {
  const size_t n = static_cast<size_t>(w_) * h_;
  ++status_.frames;
  const Posture posture = posture_;
  posture_ = Posture::UNKNOWN;
  status_.posture = posture;

  if (!hasPrev_) {
    for (size_t i = 0; i < n; ++i) bg_[i] = gray[i];
    memcpy(prev_, gray, n);
    hasPrev_ = true;
    warmupStartMs_ = nowMs;
    if (status_.state != State::ALARM && status_.state != State::FALL_SUSPECTED) {
      setState(State::WARMUP, nowMs);
    }
    status_.personPresent = false;
    return Event::NONE;
  }

  // 1-2. Maschera di primo piano, movimento, istogrammi di righe/colonne.
  memset(rowCount_, 0, h_ * sizeof(uint16_t));
  memset(colCount_, 0, w_ * sizeof(uint16_t));
  uint32_t fg = 0, moved = 0;
  uint64_t sumY = 0;
  const bool warming = status_.state == State::WARMUP;
  const float rateBg = warming ? 0.3f : params_.bgLearnRate;
  const float rateFg = warming ? 0.3f : params_.bgLearnRateFg;
  const int thr = params_.diffThreshold;

  for (uint16_t y = 0; y < h_; ++y) {
    const size_t row = static_cast<size_t>(y) * w_;
    for (uint16_t x = 0; x < w_; ++x) {
      const size_t i = row + x;
      const int px = gray[i];
      const int d = abs(px - static_cast<int>(bg_[i]));
      const bool isFg = d > thr;
      mask_[i] = isFg;
      if (abs(px - static_cast<int>(prev_[i])) > thr) ++moved;
      if (isFg) {
        ++fg;
        ++rowCount_[y];
        ++colCount_[x];
        sumY += y;
      }
      bg_[i] += (px - bg_[i]) * (isFg ? rateFg : rateBg);
    }
  }
  memcpy(prev_, gray, n);
  status_.fgFraction = static_cast<float>(fg) / n;
  status_.motion = static_cast<float>(moved) / n;

  if (warming) {
    status_.personPresent = false;
    if (nowMs - warmupStartMs_ >= params_.warmupMs) setState(State::MONITORING, nowMs);
    return Event::NONE;
  }

  // Cambio improvviso di illuminazione: quasi tutta l'immagine è "primo piano".
  if (status_.fgFraction > params_.globalChangeReset) {
    for (size_t i = 0; i < n; ++i) bg_[i] = gray[i];
    relearnBackground(nowMs);
    hasPrev_ = true;
    status_.personPresent = false;
    return Event::BACKGROUND_RESET;
  }

  // 2. Riquadro robusto: percentili 3%-97% su righe e colonne (ignora rumore isolato).
  status_.personPresent = fg >= params_.minBlobFraction * n;
  if (status_.personPresent) {
    const uint32_t lo = fg * 3 / 100;
    const uint32_t hi = fg * 97 / 100;
    status_.box.y0 = percentileIndex(rowCount_, h_, lo);
    status_.box.y1 = percentileIndex(rowCount_, h_, hi);
    status_.box.x0 = percentileIndex(colCount_, w_, lo);
    status_.box.x1 = percentileIndex(colCount_, w_, hi);
    status_.aspect = static_cast<float>(status_.box.height()) / status_.box.width();
    status_.centroidY = static_cast<float>(sumY) / fg;
    if (posture == Posture::UNKNOWN) {
      status_.standing = status_.aspect >= params_.standingAspect;
      status_.lying = status_.aspect <= params_.lyingAspect;
    } else {
      status_.standing = posture == Posture::STANDING;
      status_.lying = posture == Posture::LYING;
    }
    pushSample({nowMs, static_cast<float>(status_.box.y0), status_.centroidY, status_.aspect,
                status_.standing, status_.lying});
  } else {
    status_.standing = status_.lying = false;
  }

  // 4. Macchina a stati.
  switch (status_.state) {
    case State::MONITORING: {
      float top = 0;
      if (status_.personPresent && detectFallTransition(nowMs, &top)) {
        preFallTop_ = top;
        setState(State::FALL_SUSPECTED, nowMs);
        return Event::FALL_SUSPECTED;
      }
      break;
    }
    case State::FALL_SUSPECTED:
      // Se la persona resta immobile può "sparire" nello sfondo: conta come ancora a terra.
      if (isRecovered()) {
        setState(State::MONITORING, nowMs);
        histHead_ = histCount_ = 0;
        return Event::RECOVERED;
      }
      if (nowMs - status_.stateSinceMs >= params_.groundTimeMs) {
        setState(State::ALARM, nowMs);
        return Event::ALARM;
      }
      break;
    case State::ALARM:
    case State::WARMUP:
      break;
  }
  return Event::NONE;
}

}  // namespace fall
