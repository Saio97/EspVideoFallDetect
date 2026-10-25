// Classificatore di postura con TensorFlow Lite Micro (variante -DUSE_POSTURE_CNN).
//
// Ingresso: frame 80x60 in scala di grigi + maschera di primo piano, ridotti a
// 40x30 a 2 canali. Uscita: fall::Posture con una confidenza 0..1.
// Modello e preprocessing sono definiti in tools/train_posture_cnn.py.
#pragma once

#include <stdint.h>

#include "FallDetector.h"

namespace posture {

constexpr int kInW = 40;
constexpr int kInH = 30;
constexpr int kClasses = 4;  // NONE, STANDING, BENT, LYING

struct Result {
  fall::Posture posture = fall::Posture::UNKNOWN;
  float confidence = 0.0f;
  float scores[kClasses] = {0, 0, 0, 0};
  uint32_t inferenceUs = 0;
};

// Restituisce false se il modello non è compatibile o la memoria non basta.
bool begin();
bool available();

// gray e mask sono 80x60 (riga per riga). Con confidenza sotto `minConfidence`
// restituisce UNKNOWN, così FallDetector ricade sul rapporto altezza/larghezza.
Result classify(const uint8_t* gray, const uint8_t* mask, int w, int h, float minConfidence);

const Result& last();

}  // namespace posture
