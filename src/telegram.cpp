#include "telegram.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

const char* kHost = "api.telegram.org";

String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); ++i) {
    const uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0xF];
    }
  }
  return out;
}

// La Bot API usa HTTPS. Il certificato non viene verificato per non dover
// aggiornare il firmware quando Telegram cambia certificato: vedi docs/SICUREZZA.md.
void prepare(WiFiClientSecure& client) {
  client.setInsecure();
  client.setTimeout(15);
}

}  // namespace

void TelegramBot::begin(const char* token, const char* chatIdsCsv) {
  token_ = token;
  chatIds_.clear();
  String csv = chatIdsCsv;
  int start = 0;
  while (start <= static_cast<int>(csv.length())) {
    int comma = csv.indexOf(',', start);
    if (comma < 0) comma = csv.length();
    String id = csv.substring(start, comma);
    id.trim();
    if (id.length()) chatIds_.push_back(id);
    start = comma + 1;
  }
}

bool TelegramBot::isAuthorized(const String& chatId) const {
  for (const String& id : chatIds_)
    if (id == chatId) return true;
  return false;
}

bool TelegramBot::sendMessage(const String& chatId, const String& text) {
  if (!configured()) return false;
  WiFiClientSecure client;
  prepare(client);
  HTTPClient http;
  if (!http.begin(client, String("https://") + kHost + "/bot" + token_ + "/sendMessage")) return false;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const int code = http.POST("chat_id=" + urlEncode(chatId) + "&text=" + urlEncode(text));
  http.end();
  if (code != 200) Serial.printf("[tg] sendMessage a %s: HTTP %d\n", chatId.c_str(), code);
  return code == 200;
}

bool TelegramBot::sendPhoto(const String& chatId, const uint8_t* jpg, size_t len,
                            const String& caption) {
  if (!configured() || !jpg || !len) return false;
  WiFiClientSecure client;
  prepare(client);
  if (!client.connect(kHost, 443)) {
    Serial.println("[tg] connessione fallita");
    return false;
  }
  const String boundary = "----FallDetect7d1b";
  const String head = "--" + boundary +
                      "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chatId +
                      "\r\n--" + boundary +
                      "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption +
                      "\r\n--" + boundary +
                      "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"caduta.jpg\""
                      "\r\nContent-Type: image/jpeg\r\n\r\n";
  const String tail = "\r\n--" + boundary + "--\r\n";

  client.print(String("POST /bot") + token_ + "/sendPhoto HTTP/1.1\r\n" + "Host: " + kHost +
               "\r\nContent-Type: multipart/form-data; boundary=" + boundary +
               "\r\nContent-Length: " + String(head.length() + len + tail.length()) +
               "\r\nConnection: close\r\n\r\n");
  client.print(head);
  for (size_t off = 0; off < len;) {
    const size_t n = client.write(jpg + off, min<size_t>(2048, len - off));
    if (n == 0) break;
    off += n;
  }
  client.print(tail);

  const String status = client.readStringUntil('\n');
  client.stop();
  const bool ok = status.indexOf(" 200") > 0;
  if (!ok) Serial.printf("[tg] sendPhoto a %s: %s\n", chatId.c_str(), status.c_str());
  return ok;
}

bool TelegramBot::broadcast(const String& text) {
  bool any = false;
  for (const String& id : chatIds_) any |= sendMessage(id, text);
  return any;
}

bool TelegramBot::broadcastPhoto(const uint8_t* jpg, size_t len, const String& caption) {
  bool any = false;
  for (const String& id : chatIds_) any |= sendPhoto(id, jpg, len, caption);
  return any;
}

void TelegramBot::poll(const CommandHandler& handler) {
  if (!configured()) return;
  WiFiClientSecure client;
  prepare(client);
  HTTPClient http;
  String url = String("https://") + kHost + "/bot" + token_ +
               "/getUpdates?timeout=0&limit=10&allowed_updates=%5B%22message%22%5D";
  if (nextUpdateId_) {
    char buf[24];
    snprintf(buf, sizeof buf, "%lld", static_cast<long long>(nextUpdateId_));
    url += String("&offset=") + buf;
  }
  if (!http.begin(client, url)) return;
  const int code = http.GET();
  if (code != 200) {
    http.end();
    return;
  }
  const String body = http.getString();
  http.end();

  JsonDocument filter;
  filter["result"][0]["update_id"] = true;
  filter["result"][0]["message"]["chat"]["id"] = true;
  filter["result"][0]["message"]["from"]["first_name"] = true;
  filter["result"][0]["message"]["text"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

  for (JsonObject upd : doc["result"].as<JsonArray>()) {
    nextUpdateId_ = upd["update_id"].as<int64_t>() + 1;
    JsonObject msg = upd["message"];
    if (msg.isNull() || msg["text"].isNull()) continue;
    char id[24];
    snprintf(id, sizeof id, "%lld", static_cast<long long>(msg["chat"]["id"].as<int64_t>()));
    handler(String(id), msg["from"]["first_name"] | "", msg["text"].as<String>());
  }
}
