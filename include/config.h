// Configurazione generale. I dati personali (Wi-Fi, Telegram, password)
// stanno in secrets.h: copia secrets.example.h in secrets.h e compilalo.
#pragma once

// ---------------------------------------------------------------- Dispositivo
#define DEVICE_NAME        "Rilevatore cadute"
#define MDNS_HOSTNAME      "cadute"          // pagina web: http://cadute.local

// ---------------------------------------------------------------- Pin
// LED utente della XIAO ESP32S3 (acceso a livello basso).
#define PIN_LED            21
#define LED_ACTIVE_LOW     1
// Pulsante "Sto bene": GPIO0 è il tasto BOOT della scheda. Per una persona
// anziana è meglio un pulsante grande esterno, ad esempio tra D2 (GPIO3) e GND.
#define PIN_BUTTON         0
// Buzzer attivo 3,3 V tra D1 (GPIO2) e GND. Metti -1 se non lo colleghi.
#define PIN_BUZZER         2

// ---------------------------------------------------------------- Telecamera
#define CAMERA_VFLIP       0    // 1 se l'immagine risulta capovolta
#define CAMERA_HMIRROR     0
#define DETECT_FPS         10   // frame analizzati al secondo
#define JPEG_QUALITY       80   // 0-100, qualità della foto inviata con l'allarme

// ---------------------------------------------------------------- CNN (opzionale)
// Solo con -DUSE_POSTURE_CNN (env xiao_esp32s3_cnn). Sotto questa confidenza
// il classificatore viene ignorato e si usa il rapporto altezza/larghezza.
#define CNN_MIN_CONFIDENCE         0.60f

// ---------------------------------------------------------------- Allarmi
// Ogni quanto ripetere l'allarme finché qualcuno non risponde.
#define ALARM_REPEAT_MS            (2UL * 60UL * 1000UL)
// Avvisa già alla caduta sospetta (prima della conferma). 0 = solo allarme.
#define NOTIFY_ON_SUSPECTED        0
// Invia la foto con l'allarme (in scala di grigi, bassa risoluzione).
#define SEND_PHOTO_WITH_ALARM      1
// Consente di vedere un'istantanea dalla pagina web (protetta da password).
#define ENABLE_WEB_SNAPSHOT        1
// Intervallo di controllo dei comandi Telegram.
#define TELEGRAM_POLL_MS           4000UL
