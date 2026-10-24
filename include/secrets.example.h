// Copia questo file in include/secrets.h e inserisci i tuoi dati.
// secrets.h è escluso da git: le tue password non finiscono su GitHub.
#pragma once

// Rete Wi-Fi a 2,4 GHz (l'ESP32-S3 non supporta i 5 GHz).
#define WIFI_SSID          "NomeRete"
#define WIFI_PASSWORD      "PasswordRete"

// Bot Telegram: crealo con @BotFather e incolla qui il token.
#define TELEGRAM_BOT_TOKEN "123456789:AA..."
// Chat che ricevono gli allarmi, separate da virgola. Per scoprire il tuo
// ID scrivi /start al bot: il dispositivo risponde con l'ID della chat.
// Puoi usare anche l'ID di un gruppo di famiglia (inizia con -).
#define TELEGRAM_CHAT_IDS  "111111111,222222222"

// Credenziali della pagina web locale.
#define WEB_USER           "admin"
#define WEB_PASSWORD       "cambiami"
