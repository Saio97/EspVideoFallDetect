# EspVideoFallDetect

Rilevatore di cadute per persone anziane basato su **Seeed Studio XIAO ESP32S3 Sense** e sulla sua telecamera integrata (OV2640).

Il dispositivo sorveglia una stanza, riconosce una caduta **direttamente sulla scheda** (nessun cloud, nessuno streaming video) e, se la persona non si rialza entro 15 secondi, invia un **allarme Telegram con foto** a uno o più familiari. L'allarme si ripete ogni 2 minuti finché qualcuno non risponde.

```
 Telecamera ──► analisi 80x60 a 10 fps ──► caduta? ──► a terra per 15 s? ──► 🚨 Telegram + foto
                                              │                │
                                              │                └─► si rialza ──► nessun allarme
                                              └─► bip di avviso: "premi il pulsante se stai bene"
```

## Cosa fa

- **Riconosce la caduta** combinando tre segnali: la sagoma passa da verticale a orizzontale, la testa si abbassa molto, tutto in meno di 1,5 secondi. Sedersi o sdraiarsi lentamente non fa scattare nulla.
- **Conferma prima di allarmare**: dopo una caduta aspetta 15 secondi. Se la persona si rialza, annulla. Se resta a terra (anche immobile), allarma.
- **Pulsante "Sto bene"**: durante la verifica il buzzer emette un bip ogni 2 secondi; premendo il pulsante si annulla.
- **Telegram**: allarme con foto, comandi `/stato`, `/foto`, `/ok`, `/pausa`, `/riprendi`, `/test`.
- **Pagina web locale** (`http://cadute.local`, protetta da password): stato in tempo reale, istantanea con la sagoma rilevata, pulsanti, taratura delle soglie.
- **Robusto**: si adatta ai cambi di luce, continua a funzionare senza Wi-Fi e rimanda l'allarme quando la rete torna.
- **Privacy**: le immagini lasciano il dispositivo solo con un allarme o quando un familiare autorizzato le chiede.

## Cosa ti serve

| Cosa | Note | Costo indicativo |
|---|---|---|
| Seeed Studio **XIAO ESP32S3 Sense** | con scheda di espansione telecamera (inclusa nel kit "Sense") | ~15 € |
| Cavo **USB‑C** dati | per programmarla e alimentarla | — |
| **Alimentatore USB 5 V, 1 A** | il dispositivo resta sempre acceso | ~5 € |
| Rete **Wi‑Fi a 2,4 GHz** | l'ESP32‑S3 non usa i 5 GHz | — |
| Smartphone con **Telegram** | per ogni familiare che riceve gli allarmi | gratis |
| PC con **VS Code + PlatformIO** | solo per caricare il firmware | gratis |
| *Consigliato:* **buzzer attivo 3,3 V** | su D1 (GPIO2) e GND, per il bip di avviso e la sirena | ~1 € |
| *Consigliato:* **pulsante grande** | su D2 (GPIO3) e GND, più comodo del tasto BOOT | ~2 € |
| *Consigliato:* **supporto/scatola** | da fissare in alto, in un angolo della stanza | — |

Dettagli su collegamenti e montaggio: [docs/HARDWARE.md](docs/HARDWARE.md).

## Avvio rapido

1. Installa [VS Code](https://code.visualstudio.com/) e l'estensione **PlatformIO IDE**.
2. Scarica questo repository e aprilo in VS Code.
3. Copia `include/secrets.example.h` in `include/secrets.h` e inserisci Wi‑Fi, token del bot Telegram e ID delle chat ([guida Telegram](docs/INSTALLAZIONE.md#2-crea-il-bot-telegram)).
4. Collega la XIAO via USB e premi **Upload** (oppure `pio run -t upload`).
5. Fissa il dispositivo in alto, in un angolo, con la stanza **vuota** davanti: nei primi 3 secondi impara com'è l'ambiente.
6. Scrivi `/test` al bot per verificare che gli allarmi arrivino.

Guida completa passo passo: [docs/INSTALLAZIONE.md](docs/INSTALLAZIONE.md).

## Documentazione

- [HARDWARE.md](docs/HARDWARE.md): componenti, collegamenti, dove e come montarlo
- [INSTALLAZIONE.md](docs/INSTALLAZIONE.md): firmware, bot Telegram, primo avvio, prova
- [TARATURA.md](docs/TARATURA.md): come ridurre falsi allarmi e cadute non rilevate
- [ALGORITMO.md](docs/ALGORITMO.md): come funziona il rilevamento
- [SICUREZZA.md](docs/SICUREZZA.md): privacy, sicurezza e limiti

## Struttura

```
include/            configurazione (config.h, secrets.example.h, pin telecamera)
lib/FallDetector/   algoritmo di rilevamento, C++ puro e testabile sul PC
src/                firmware: telecamera, Telegram, pagina web, logica di allarme
test/               test dell'algoritmo su scene simulate (pio test -e native)
docs/               documentazione
```

## Sviluppo

```bash
pio test -e native          # test dell'algoritmo sul PC
pio run -e xiao_esp32s3     # compila il firmware
pio run -t upload           # carica sulla scheda
pio device monitor          # log seriale
```

La CI su GitHub esegue i test e compila il firmware a ogni push; il `firmware.bin` si scarica dagli artifact della build.

## ⚠️ Avvertenza

Questo progetto **non è un dispositivo medico** e non sostituisce un sistema di teleassistenza certificato. Funziona solo dove la telecamera vede, con un minimo di luce e con il Wi‑Fi attivo. Usalo come aiuto in più, non come unica protezione. Vedi [SICUREZZA.md](docs/SICUREZZA.md).

## Licenza

MIT
