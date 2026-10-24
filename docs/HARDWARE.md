# Hardware

## Componenti

### Indispensabili

- **Seeed Studio XIAO ESP32S3 Sense**: ESP32‑S3 dual core 240 MHz, 8 MB PSRAM, 8 MB flash, telecamera OV2640 sulla scheda di espansione. Verifica che la scheda di espansione con la telecamera sia **innestata** sul connettore B2B (si sente un clic) e che il cavo flat della telecamera sia ben inserito.
- **Cavo USB‑C dati** (non solo ricarica) e **alimentatore 5 V ≥ 1 A**.

### Consigliati

- **Buzzer attivo 3,3 V** (quelli "attivi" suonano da soli quando ricevono tensione; non serve un transistor se assorbono meno di 20 mA).
- **Pulsante normalmente aperto**, meglio se grande e colorato, perché la persona anziana lo trovi facilmente.
- **Dissipatore adesivo** sul chip ESP32‑S3: con telecamera e Wi‑Fi sempre attivi la scheda si scalda.
- **Antenna esterna**: la XIAO ESP32S3 Sense ha un connettore U.FL e un'antenna in dotazione. Collegala, migliora molto la portata del Wi‑Fi.
- **Supporto**: una scatolina stampata in 3D o un supporto per webcam con la telecamera libera.

## Collegamenti

| Componente | Pin XIAO | GPIO | Note |
|---|---|---|---|
| Buzzer + | D1 | GPIO2 | `PIN_BUZZER` in `include/config.h` (metti `-1` se assente) |
| Buzzer − | GND | | |
| Pulsante | D2 | GPIO3 | un capo sul pin, l'altro su GND (pull‑up interno) |
| Pulsante | GND | | |

Se usi il pulsante esterno su D2, imposta `#define PIN_BUTTON 3` in `include/config.h`. Senza pulsante esterno funziona il tasto **BOOT** sulla scheda (GPIO0).

La telecamera usa pin dedicati sulla scheda di espansione (`include/camera_pins.h`): non serve collegare nulla.

## Dove montarlo

L'algoritmo guarda la **forma della sagoma** (verticale in piedi, orizzontale a terra), quindi conta molto da dove guarda la telecamera.

- **In alto, in un angolo**, a circa 2–2,2 m, inclinato verso il basso di 20–30°. Deve vedere il pavimento della zona dove la persona cammina di più.
- **Inquadratura laterale, non dall'alto a picco**: montata al centro del soffitto, una persona in piedi e una a terra hanno forme simili e la caduta non si distingue.
- **Persona intera**: in piedi deve occupare circa metà dell'altezza dell'immagine. Se appare minuscola, avvicina il dispositivo o scegli una stanza più piccola.
- **Evita il controluce**: non puntarla verso una finestra. Tende mosse dal vento, TV e animali sono fonti di movimento: tienili fuori inquadratura quando puoi.
- **Luce**: la OV2640 non vede al buio. Di notte serve almeno una lucina accesa (una luce notturna a LED basta), oppure un modulo con illuminatore IR.

Stanze consigliate, in ordine di rischio: bagno (attenzione alla privacy, vedi SICUREZZA.md), camera da letto (percorso letto‑bagno), corridoio, cucina.

Usa la pagina web e il pulsante **Scatta istantanea** per verificare l'inquadratura: il riquadro verde deve seguire la persona.

## Alimentazione e consumi

Il dispositivo assorbe circa 150–250 mA a 5 V con telecamera e Wi‑Fi attivi. È pensato per restare sempre collegato alla rete elettrica. Se vuoi una batteria tampone, usa un power bank con funzione "pass‑through" (si ricarica mentre alimenta).
