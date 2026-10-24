#pragma once

#include <esp_camera.h>
#include <stdint.h>

// Risoluzione di acquisizione e di analisi.
constexpr int CAM_W = 320;   // QVGA
constexpr int CAM_H = 240;
constexpr int DET_SCALE = 4;
constexpr int DET_W = CAM_W / DET_SCALE;  // 80
constexpr int DET_H = CAM_H / DET_SCALE;  // 60

bool cameraBegin();

// Riduce un'immagine in scala di grigi di un fattore DET_SCALE (media dei blocchi).
void downsampleGray(const uint8_t* src, int srcW, int srcH, uint8_t* dst);
