// Test dell'algoritmo su scene sintetiche: `pio test -e native`.
//
// Ogni scena simula 10 fps su un'immagine 80x60 con sfondo rumoroso e una
// "persona" rettangolare più chiara, di cui variano posizione e dimensioni.
#include <unity.h>

#include <stdlib.h>
#include <string.h>

#include <vector>

#include "FallDetector.h"

using fall::Event;
using fall::FallDetector;
using fall::State;

namespace {

constexpr int W = 80;
constexpr int H = 60;
constexpr uint32_t DT = 100;  // 10 fps

struct Person {
  float cx, bottom, w, h;  // centro x, bordo inferiore, larghezza, altezza
};

struct Sim {
  FallDetector det;
  std::vector<uint8_t> img = std::vector<uint8_t>(W * H);
  uint32_t t = 0;
  int brightness = 0;
  std::vector<Event> events;
  fall::Posture posture = fall::Posture::UNKNOWN;  // se impostata, passata a ogni frame

  Sim() {
    srand(1234);
    TEST_ASSERT_TRUE(det.begin(W, H));
  }

  void render(const Person* p) {
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        int v = 60 + (x * 7 + y * 3) % 40 + brightness + (rand() % 9) - 4;  // texture + rumore
        img[y * W + x] = static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
      }
    if (!p) return;
    const int x0 = static_cast<int>(p->cx - p->w / 2), x1 = static_cast<int>(p->cx + p->w / 2);
    const int y1 = static_cast<int>(p->bottom), y0 = static_cast<int>(p->bottom - p->h);
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < H; ++y)
      for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < W; ++x)
        img[y * W + x] = static_cast<uint8_t>(190 + (rand() % 9) - 4 + brightness / 2);
  }

  void step(const Person* p) {
    render(p);
    det.setPosture(posture);
    Event e = det.update(img.data(), t);
    if (e != Event::NONE) events.push_back(e);
    t += DT;
  }

  void hold(const Person* p, uint32_t ms) {
    for (uint32_t el = 0; el < ms; el += DT) step(p);
  }

  // Interpolazione lineare tra due pose in `ms` millisecondi.
  void move(const Person& a, const Person& b, uint32_t ms) {
    const int steps = ms / DT;
    for (int i = 1; i <= steps; ++i) {
      const float k = static_cast<float>(i) / steps;
      Person p{a.cx + (b.cx - a.cx) * k, a.bottom + (b.bottom - a.bottom) * k, a.w + (b.w - a.w) * k,
               a.h + (b.h - a.h) * k};
      step(&p);
    }
  }

  bool has(Event e) const {
    for (Event x : events)
      if (x == e) return true;
    return false;
  }

  void warmEmptyRoom() { hold(nullptr, 4000); }
};

const Person kStanding{40, 55, 10, 34};
const Person kLying{44, 56, 34, 9};
const Person kSitting{40, 55, 14, 22};

void test_warmup_then_monitoring() {
  Sim s;
  s.warmEmptyRoom();
  TEST_ASSERT_EQUAL(State::MONITORING, s.det.status().state);
  TEST_ASSERT_FALSE(s.det.status().personPresent);
}

void test_standing_person_detected_as_vertical() {
  Sim s;
  s.warmEmptyRoom();
  s.hold(&kStanding, 1000);
  TEST_ASSERT_TRUE(s.det.status().personPresent);
  TEST_ASSERT_TRUE(s.det.status().aspect > 2.0f);
  TEST_ASSERT_EQUAL(State::MONITORING, s.det.status().state);
}

void test_fast_fall_and_stay_down_triggers_alarm() {
  Sim s;
  s.warmEmptyRoom();
  Person walkStart{15, 55, 10, 34};
  s.move(walkStart, kStanding, 2000);  // cammina verso il centro
  s.move(kStanding, kLying, 600);      // cade in 0,6 s
  TEST_ASSERT_EQUAL(State::FALL_SUSPECTED, s.det.status().state);
  s.hold(&kLying, 16000);              // resta a terra
  TEST_ASSERT_EQUAL(State::ALARM, s.det.status().state);
  TEST_ASSERT_TRUE(s.has(Event::FALL_SUSPECTED));
  TEST_ASSERT_TRUE(s.has(Event::ALARM));
}

void test_alarm_persists_until_reset() {
  Sim s;
  s.warmEmptyRoom();
  s.hold(&kStanding, 1000);
  s.move(kStanding, kLying, 500);
  s.hold(&kLying, 16000);
  TEST_ASSERT_EQUAL(State::ALARM, s.det.status().state);
  s.hold(&kStanding, 3000);  // anche se si rialza, l'allarme resta finché non viene gestito
  TEST_ASSERT_EQUAL(State::ALARM, s.det.status().state);
  s.det.reset(s.t);
  s.hold(&kStanding, 500);
  TEST_ASSERT_EQUAL(State::MONITORING, s.det.status().state);
}

void test_fall_then_get_up_recovers_without_alarm() {
  Sim s;
  s.warmEmptyRoom();
  s.hold(&kStanding, 1000);
  s.move(kStanding, kLying, 500);
  s.hold(&kLying, 4000);
  s.move(kLying, kStanding, 2000);
  s.hold(&kStanding, 15000);
  TEST_ASSERT_TRUE(s.has(Event::FALL_SUSPECTED));
  TEST_ASSERT_TRUE(s.has(Event::RECOVERED));
  TEST_ASSERT_FALSE(s.has(Event::ALARM));
  TEST_ASSERT_EQUAL(State::MONITORING, s.det.status().state);
}

void test_sitting_down_is_not_a_fall() {
  Sim s;
  s.warmEmptyRoom();
  s.hold(&kStanding, 1000);
  s.move(kStanding, kSitting, 800);
  s.hold(&kSitting, 20000);
  TEST_ASSERT_FALSE(s.has(Event::FALL_SUSPECTED));
  TEST_ASSERT_FALSE(s.has(Event::ALARM));
}

void test_slowly_lying_down_is_not_a_fall() {
  Sim s;
  s.warmEmptyRoom();
  s.hold(&kStanding, 1000);
  s.move(kStanding, kSitting, 2500);
  s.move(kSitting, kLying, 3000);
  s.hold(&kLying, 20000);
  TEST_ASSERT_FALSE(s.has(Event::FALL_SUSPECTED));
  TEST_ASSERT_FALSE(s.has(Event::ALARM));
}

void test_lights_on_resets_background_without_alarm() {
  Sim s;
  s.warmEmptyRoom();
  s.brightness = 70;  // luce accesa di colpo
  s.hold(nullptr, 5000);
  TEST_ASSERT_TRUE(s.has(Event::BACKGROUND_RESET));
  TEST_ASSERT_FALSE(s.has(Event::FALL_SUSPECTED));
  TEST_ASSERT_EQUAL(State::MONITORING, s.det.status().state);
}

void test_immobile_fallen_person_absorbed_in_background_still_alarms() {
  Sim s;
  fall::Params p;
  p.groundTimeMs = 60000;   // tempo lungo: la sagoma viene assorbita dallo sfondo
  p.bgLearnRateFg = 0.02f;  // assorbimento rapido per accelerare il test
  TEST_ASSERT_TRUE(s.det.begin(W, H, p));
  s.warmEmptyRoom();
  s.hold(&kStanding, 1000);
  s.move(kStanding, kLying, 500);
  s.hold(&kLying, 61000);
  TEST_ASSERT_EQUAL(State::ALARM, s.det.status().state);
}

// Una caduta "in scorcio" verso la telecamera lascia la sagoma quasi quadrata:
// il rapporto altezza/larghezza non basta, ma un classificatore esterno sì.
void test_external_posture_detects_foreshortened_fall() {
  const Person squarish{40, 56, 18, 18};
  {
    Sim s;
    s.warmEmptyRoom();
    s.hold(&kStanding, 1000);
    s.move(kStanding, squarish, 500);
    s.hold(&squarish, 16000);
    TEST_ASSERT_FALSE(s.has(Event::FALL_SUSPECTED));  // solo riquadro: non vista
  }
  {
    Sim s;
    s.warmEmptyRoom();
    s.posture = fall::Posture::STANDING;
    s.hold(&kStanding, 1000);
    s.posture = fall::Posture::LYING;
    s.move(kStanding, squarish, 500);
    s.hold(&squarish, 16000);
    TEST_ASSERT_EQUAL(State::ALARM, s.det.status().state);
  }
}

void test_external_posture_bent_is_not_lying() {
  Sim s;
  s.warmEmptyRoom();
  s.posture = fall::Posture::STANDING;
  s.hold(&kStanding, 1000);
  s.posture = fall::Posture::BENT;
  s.move(kStanding, kLying, 500);  // forma a terra ma il classificatore dice "chinato"
  s.hold(&kLying, 16000);
  TEST_ASSERT_FALSE(s.has(Event::FALL_SUSPECTED));
}

void test_walking_across_room_no_false_alarm() {
  Sim s;
  s.warmEmptyRoom();
  Person a{10, 55, 10, 34}, b{70, 50, 9, 30};
  for (int i = 0; i < 5; ++i) {
    s.move(a, b, 4000);
    s.move(b, a, 4000);
  }
  TEST_ASSERT_FALSE(s.has(Event::FALL_SUSPECTED));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_warmup_then_monitoring);
  RUN_TEST(test_standing_person_detected_as_vertical);
  RUN_TEST(test_fast_fall_and_stay_down_triggers_alarm);
  RUN_TEST(test_alarm_persists_until_reset);
  RUN_TEST(test_fall_then_get_up_recovers_without_alarm);
  RUN_TEST(test_sitting_down_is_not_a_fall);
  RUN_TEST(test_slowly_lying_down_is_not_a_fall);
  RUN_TEST(test_lights_on_resets_background_without_alarm);
  RUN_TEST(test_immobile_fallen_person_absorbed_in_background_still_alarms);
  RUN_TEST(test_walking_across_room_no_false_alarm);
  RUN_TEST(test_external_posture_detects_foreshortened_fall);
  RUN_TEST(test_external_posture_bent_is_not_lying);
  return UNITY_END();
}
