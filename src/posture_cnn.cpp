#include "posture_cnn.h"

#ifdef USE_POSTURE_CNN

#include <Arduino.h>

#include <TensorFlowLite_ESP32.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include "posture_model_data.h"

namespace posture {

namespace {

constexpr size_t kArenaSize = 40 * 1024;
alignas(16) uint8_t* arena = nullptr;

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
bool ready = false;
Result lastResult;

const fall::Posture kClassToPosture[kClasses] = {fall::Posture::NONE, fall::Posture::STANDING,
                                                 fall::Posture::BENT, fall::Posture::LYING};

}  // namespace

bool begin() {
  model = tflite::GetModel(g_posture_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("[cnn] schema del modello %lu diverso da %d\n",
                  static_cast<unsigned long>(model->version()), TFLITE_SCHEMA_VERSION);
    return false;
  }

  // PSRAM se c'è, così la RAM interna resta alla telecamera e al Wi-Fi.
  arena = static_cast<uint8_t*>(psramFound() ? ps_malloc(kArenaSize) : malloc(kArenaSize));
  if (!arena) {
    Serial.println("[cnn] memoria insufficiente per l'arena");
    return false;
  }

  static tflite::MicroMutableOpResolver<8> resolver;
  resolver.AddConv2D();
  resolver.AddMaxPool2D();
  resolver.AddMean();
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddReshape();
  resolver.AddQuantize();
  resolver.AddDequantize();

  static tflite::MicroInterpreter staticInterpreter(model, resolver, arena, kArenaSize);
  interpreter = &staticInterpreter;
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[cnn] AllocateTensors fallito: aumenta kArenaSize");
    return false;
  }
  input = interpreter->input(0);
  output = interpreter->output(0);
  if (input->dims->size != 4 || input->dims->data[1] != kInH || input->dims->data[2] != kInW ||
      input->dims->data[3] != 2 || input->type != kTfLiteInt8) {
    Serial.println("[cnn] forma dell'ingresso inattesa");
    return false;
  }
  ready = true;
  Serial.printf("[cnn] modello caricato (%u byte), arena usata %u byte\n", g_posture_model_data_len,
                static_cast<unsigned>(interpreter->arena_used_bytes()));
  return true;
}

bool available() { return ready; }

Result classify(const uint8_t* gray, const uint8_t* mask, int w, int h, float minConfidence) {
  Result r;
  if (!ready || w != kInW * 2 || h != kInH * 2) return r;

  const uint32_t t0 = micros();
  const float scale = input->params.scale;
  const int zero = input->params.zero_point;
  int8_t* in = input->data.int8;
  // Stesso preprocessing di tools/train_posture_cnn.py::to_input:
  // grigio = media 2x2, maschera = massimo 2x2 * 255.
  for (int y = 0; y < kInH; ++y) {
    const uint8_t* g0 = gray + (2 * y) * w;
    const uint8_t* g1 = g0 + w;
    const uint8_t* m0 = mask + (2 * y) * w;
    const uint8_t* m1 = m0 + w;
    for (int x = 0; x < kInW; ++x) {
      const int gx = 2 * x;
      const float g = (g0[gx] + g0[gx + 1] + g1[gx] + g1[gx + 1]) * 0.25f;
      const float m = (m0[gx] | m0[gx + 1] | m1[gx] | m1[gx + 1]) ? 255.0f : 0.0f;
      int qg = static_cast<int>(lroundf(g / scale)) + zero;
      int qm = static_cast<int>(lroundf(m / scale)) + zero;
      in[(y * kInW + x) * 2] = static_cast<int8_t>(qg < -128 ? -128 : qg > 127 ? 127 : qg);
      in[(y * kInW + x) * 2 + 1] = static_cast<int8_t>(qm < -128 ? -128 : qm > 127 ? 127 : qm);
    }
  }

  if (interpreter->Invoke() != kTfLiteOk) return r;

  int best = 0;
  for (int i = 0; i < kClasses; ++i) {
    r.scores[i] = (output->data.int8[i] - output->params.zero_point) * output->params.scale;
    if (r.scores[i] > r.scores[best]) best = i;
  }
  r.confidence = r.scores[best];
  r.posture = r.confidence >= minConfidence ? kClassToPosture[best] : fall::Posture::UNKNOWN;
  r.inferenceUs = micros() - t0;
  lastResult = r;
  return r;
}

const Result& last() { return lastResult; }

}  // namespace posture

#else  // USE_POSTURE_CNN

namespace posture {
bool begin() { return false; }
bool available() { return false; }
Result classify(const uint8_t*, const uint8_t*, int, int, float) { return Result(); }
const Result& last() {
  static Result r;
  return r;
}
}  // namespace posture

#endif
