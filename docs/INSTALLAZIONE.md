# Installazione

## 1. Prepara il PC

1. Installa [Visual Studio Code](https://code.visualstudio.com/).
2. Da VS Code installa l'estensione **PlatformIO IDE** (icona Estensioni, cerca "PlatformIO").
3. Scarica questo repository (pulsante **Code → Download ZIP** su GitHub, oppure `git clone`) e apri la cartella con **File → Apri cartella**.

Al primo avvio PlatformIO scarica da solo il compilatore per l'ESP32‑S3 e le librerie (qualche minuto).

## 2. Crea il bot Telegram

1. In Telegram cerca **@BotFather** e scrivigli `/newbot`.
2. Scegli un nome (es. "Allarme Nonna") e uno username che finisca con `bot` (es. `allarme_nonna_bot`).
3. BotFather ti risponde con un **token** tipo `123456789:AAH...`. Tienilo segreto: chi lo ha può controllare il bot.
4. Ogni familiare che deve ricevere gli allarmi apre il bot e preme **Avvia** (`/start`).

**ID delle chat.** Per scoprirli puoi lasciare `TELEGRAM_CHAT_IDS` vuoto al primo caricamento: quando qualcuno scrive `/start` al bot, il dispositivo risponde con l'ID di quella chat. Poi inserisci gli ID in `secrets.h` e ricarica il firmware.

**Gruppo di famiglia** (consigliato): crea un gruppo, aggiungi il bot, scrivi `/start@nome_del_bot` nel gruppo e usa l'ID restituito (inizia con `-`). Così tutti vedono l'allarme e chi risponde `/ok`.

## 3. Inserisci i tuoi dati

Copia `include/secrets.example.h` in `include/secrets.h` e compila:

```c
#define WIFI_SSID          "NomeRete"
#define WIFI_PASSWORD      "PasswordRete"
#define TELEGRAM_BOT_TOKEN "123456789:AAH..."
#define TELEGRAM_CHAT_IDS  "111111111,-1002233445566"
#define WEB_USER           "admin"
#define WEB_PASSWORD       "una-password-vera"
```

`secrets.h` è escluso da git, quindi non finisce su GitHub.

Altre opzioni (pin, ripetizione allarme, foto, avviso anticipato) sono in `include/config.h`.

## 4. Carica il firmware

1. Collega la XIAO al PC con il cavo USB‑C.
2. In basso nella barra di PlatformIO premi **→ Upload** (oppure `pio run -t upload`).
3. Se il caricamento non parte: tieni premuto il tasto **BOOT**, premi e rilascia **RESET**, rilascia **BOOT**, poi riprova.
4. Apri il **Serial Monitor** (icona spina) a 115200 baud: vedrai l'indirizzo IP e i messaggi del rilevamento.

## 5. Primo avvio

1. Monta il dispositivo nella posizione definitiva (vedi [HARDWARE.md](HARDWARE.md#dove-montarlo)).
2. **Esci dall'inquadratura** e alimentalo: per 3 secondi impara com'è la stanza vuota.
3. Su Telegram arriva "🟢 Rilevatore cadute avviato" con l'indirizzo della pagina web.
4. Apri `http://cadute.local` (o l'IP) dal telefono o dal PC, sulla stessa rete Wi‑Fi. Inserisci utente e password di `secrets.h`.

## 6. Prova

1. Scrivi `/test` al bot: deve arrivare un allarme di prova con foto.
2. Cammina davanti alla telecamera: nella pagina web "Persona inquadrata" deve diventare **sì** e la forma deve superare 1,3.
3. **Simula una caduta** su un materasso o tappetino: lasciati andare a terra in modo deciso e resta giù. Senti il bip di avviso; dopo 15 secondi arriva l'allarme.
4. Rifai la prova e premi il pulsante durante i bip: la verifica si annulla.
5. Rifai la prova e rialzati entro 15 secondi: nessun allarme.

Se qualcosa non va come previsto, vedi [TARATURA.md](TARATURA.md).

## Comandi Telegram

| Comando | Cosa fa |
|---|---|
| `/stato` | stato del dispositivo, Wi‑Fi, allarmi dall'avvio |
| `/foto` | scatta e invia una foto |
| `/ok` | allarme gestito oppure falso allarme: torna in sorveglianza |
| `/pausa 60` | sospende per 60 minuti (default 30), es. durante le pulizie o con ospiti |
| `/riprendi` | riprende subito la sorveglianza |
| `/test` | invia un allarme di prova |
| `/id` | mostra l'ID della chat |

Solo le chat elencate in `TELEGRAM_CHAT_IDS` possono usare i comandi.

## Problemi comuni

| Sintomo | Soluzione |
|---|---|
| `esp_camera_init fallito` nel log | la scheda della telecamera non è innestata bene; scollega l'alimentazione e reinnestala |
| `PSRAM non trovata` | in `platformio.ini` deve esserci `board_build.arduino.memory_type = qio_opi` |
| Non si connette al Wi‑Fi | la rete deve essere a 2,4 GHz; collega l'antenna esterna |
| Nessun messaggio Telegram | controlla token e ID; scrivi `/id` al bot e confronta |
| `cadute.local` non si apre | usa l'indirizzo IP (nel log seriale o nel messaggio di avvio); alcuni Android non supportano `.local` |
| Immagine capovolta | `#define CAMERA_VFLIP 1` in `config.h` |
