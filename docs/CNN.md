# Variante con CNN (classificatore di postura)

La versione base riconosce "in piedi" e "a terra" dal **rapporto altezza/larghezza** della sagoma. Funziona bene con la telecamera di lato, ma soffre in alcuni casi: caduta *verso* la telecamera (sagoma quasi quadrata), persona sdraiata sul divano, chinata a raccogliere qualcosa.

La variante CNN sostituisce solo quel criterio con una piccola rete neurale che guarda l'intera immagine 40×30 (grigio + sagoma) e risponde con una di quattro posture: **nessuno, in piedi, seduto/chinato, a terra**. Tutto il resto (caduta rapida, 15 s a terra, allarmi, pulsante) resta identico. Se la rete non è sicura (confidenza sotto `CNN_MIN_CONFIDENCE`, 60%), si torna automaticamente al rapporto altezza/larghezza.

## Numeri

| | |
|---|---|
| Modello | 4 strati convolutivi, ~12 000 parametri, int8 |
| Dimensione | ~23 KB di flash |
| Memoria di lavoro | ~40 KB (in PSRAM) |
| Tempo di inferenza stimato sull'ESP32‑S3 | 10–30 ms per fotogramma, compatibile con i 10 fps |
| Motore | TensorFlow Lite Micro (libreria `TensorFlowLite_ESP32`) |

Il tempo reale di inferenza è mostrato nella pagina web accanto alla postura.

## Compilare e caricare

```bash
pio run -e xiao_esp32s3_cnn -t upload
```

Il firmware stampa `[cnn] classificatore di postura attivo` nel log seriale. Se qualcosa non va (modello incompatibile, memoria), continua con l'algoritmo base e lo scrive nel log.

## Il modello incluso

`models/posture_cnn.tflite` (e `src/posture_model_data.cc`) è addestrato **solo su scene sintetiche**: sagome geometriche su sfondi con texture e rumore, generate da `tools/train_posture_cnn.py`. Serve a far funzionare la pipeline e dà già un comportamento ragionevole, ma la **tua stanza, la tua telecamera e la tua inquadratura** sono diverse. Per un risultato affidabile servono fotogrammi reali.

## Raccogliere dati reali (10 minuti)

1. Carica il firmware (base o CNN, è uguale) e apri la pagina web.
2. Apri la sezione **Raccolta dati per la CNN** sotto l'istantanea.
3. Con la stanza vuota premi **Nessuno** una decina di volte, in momenti e luci diversi.
4. Fatti inquadrare (o fai inquadrare qualcuno) **in piedi** in vari punti della stanza e premi **In piedi** ogni volta: 30–50 fotogrammi.
5. Idem **seduto o chinato** (sedia, divano, raccogliere un oggetto): 30–50 fotogrammi.
6. Idem **a terra** su un materasso, in varie direzioni rispetto alla telecamera, anche verso di essa: 30–50 fotogrammi.
7. Ogni pressione scarica un file `ETICHETTA_timestamp.bin` (9 600 byte: 80×60 grigio + 80×60 sagoma). Spostali in:

```
data/real/NONE/
data/real/STANDING/
data/real/BENT/
data/real/LYING/
```

Puoi anche scaricare `http://cadute.local/frame.bin` da uno script per raccogliere a raffica.

## Riaddestrare

```bash
pip install tensorflow-cpu numpy
python tools/train_posture_cnn.py --real data/real --epochs 30
pio run -e xiao_esp32s3_cnn -t upload
```

Lo script mescola i dati sintetici con quelli reali (pesati di più), esporta il modello int8 e riscrive `src/posture_model_data.cc`. Stampa l'accuratezza e la matrice di confusione: guarda soprattutto che **A terra** non venga confuso con **Seduto/chinato** (caduta non vista) e viceversa (falso allarme).

La cartella `data/` è esclusa da git: i fotogrammi della tua casa non finiscono su GitHub.

## Perché non una rete di stima della posa

MoveNet o PoseNet darebbero lo scheletro della persona, ma sull'ESP32‑S3 girano a 1–3 fotogrammi al secondo (ingresso 192×192, milioni di operazioni): troppo lenti per catturare una caduta che dura mezzo secondo e troppo pesanti per la memoria. La CNN di postura lavora sulla sagoma già estratta e costa cento volte meno.
