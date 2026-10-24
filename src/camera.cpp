#include "camera.h"

#include <Arduino.h>

#include "camera_pins.h"
#include "config.h"

bool cameraBegin() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_GRAYSCALE;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("[cam] PSRAM non trovata: controlla 'board_build.arduino.memory_type = qio_opi'");
    c.fb_count = 1;
    c.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[cam] esp_camera_init fallito: 0x%x\n", err);
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, CAMERA_VFLIP);
    s->set_hmirror(s, CAMERA_HMIRROR);
    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);  // esposizione automatica migliorata per ambienti poco illuminati
  }
  return true;
}

void downsampleGray(const uint8_t* src, int srcW, int srcH, uint8_t* dst) {
  const int dw = srcW / DET_SCALE, dh = srcH / DET_SCALE;
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      uint32_t sum = 0;
      const uint8_t* p = src + (y * DET_SCALE) * srcW + x * DET_SCALE;
      for (int k = 0; k < DET_SCALE; ++k, p += srcW)
        for (int j = 0; j < DET_SCALE; ++j) sum += p[j];
      dst[y * dw + x] = sum / (DET_SCALE * DET_SCALE);
    }
  }
}
