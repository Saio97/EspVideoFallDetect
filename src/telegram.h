// Client minimo per la Bot API di Telegram (messaggi, foto, comandi).
#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

class TelegramBot {
 public:
  using CommandHandler =
      std::function<void(const String& chatId, const String& from, const String& text)>;

  void begin(const char* token, const char* chatIdsCsv);
  bool configured() const { return token_.length() > 20; }

  bool isAuthorized(const String& chatId) const;
  bool sendMessage(const String& chatId, const String& text);
  bool sendPhoto(const String& chatId, const uint8_t* jpg, size_t len, const String& caption);

  // Invia a tutte le chat autorizzate. Restituisce true se almeno un invio riesce.
  bool broadcast(const String& text);
  bool broadcastPhoto(const uint8_t* jpg, size_t len, const String& caption);

  // Legge i nuovi messaggi e chiama `handler` per ognuno.
  void poll(const CommandHandler& handler);

 private:
  String token_;
  std::vector<String> chatIds_;
  int64_t nextUpdateId_ = 0;
};
