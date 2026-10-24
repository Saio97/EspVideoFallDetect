# Come funziona il rilevamento

L'algoritmo è in `lib/FallDetector/` ed è C++ puro, senza dipendenze dall'ESP32, così si può testare sul PC (`pio test -e native`).

## Perché non una rete neurale

Una rete di stima della posa (es. MoveNet) sarebbe più precisa ma sull'ESP32‑S3 gira a 1–3 fotogrammi al secondo, troppo pochi per vedere una caduta che dura mezzo secondo. La sottrazione dello sfondo con analisi della sagoma gira a 10 fps, consuma poco, non scalda e si spiega e tara facilmente. Per una telecamera fissa in una stanza è un buon compromesso.

## Passi per ogni fotogramma

1. **Acquisizione**: la OV2640 produce un'immagine 320×240 in scala di grigi, ridotta a **80×60** con la media dei blocchi 4×4 (meno rumore e calcoli più veloci).

2. **Modello dello sfondo**: per ogni pixel si tiene una media mobile della luminosità. I pixel di sfondo si aggiornano velocemente (si adattano alle variazioni lente di luce), quelli occupati dalla persona molto lentamente (la persona non viene "assorbita" subito).

3. **Primo piano**: un pixel è primo piano se differisce dallo sfondo più di `diffThreshold`. Se oltre il 55% dell'immagine cambia insieme (luce accesa o spenta, esposizione automatica) lo sfondo viene ricalcolato.

4. **Sagoma**: dal primo piano si ricava un riquadro **robusto**, usando i percentili 3%–97% delle righe e delle colonne, così puntini di rumore isolati non lo allargano. Si calcolano:
   - **forma** = altezza / larghezza del riquadro (circa 2–3 in piedi, sotto 1 a terra)
   - **alto** del riquadro (la testa)
   - **baricentro** verticale

5. **Storico**: gli ultimi 48 campioni (circa 5 s) di forma e posizione.

## Macchina a stati

```
WARMUP ──3 s──► MONITORING ──caduta──► FALL_SUSPECTED ──15 s a terra──► ALARM
                    ▲                        │                            │
                    └──────── si rialza ─────┘          /ok, pulsante, web ┘
```

**Caduta** (MONITORING → FALL_SUSPECTED): nell'ultima finestra di `fallWindowMs` (1,5 s) esiste un campione in cui la persona era **in piedi** (forma ≥ 1,25) e ora è **a terra** (forma ≤ 0,85) con la testa **scesa** di almeno il 20% dell'altezza dell'immagine. Tutte e tre le condizioni insieme distinguono la caduta da:
- sedersi: la testa scende ma la forma resta verticale o quasi quadrata
- sdraiarsi lentamente: la transizione dura più di 1,5 s
- chinarsi a raccogliere qualcosa: la forma non scende sotto 0,85

**Conferma** (FALL_SUSPECTED → ALARM): la persona non torna in piedi entro `groundTimeMs`. Se resta immobile a lungo e la sagoma viene assorbita dallo sfondo, conta comunque come "ancora a terra": solo vedere di nuovo una persona in piedi, con la testa vicino alla posizione di prima, annulla.

**Allarme**: resta attivo finché qualcuno non lo gestisce (`/ok`, pulsante, pagina web), anche se la persona si rialza. Un cambio di luce non annulla mai un allarme o una verifica in corso.

## Test

`test/test_detector/test_main.cpp` genera scene sintetiche (sfondo con texture e rumore, persona come rettangolo) e verifica:

- caduta rapida e permanenza a terra → allarme
- caduta e rialzata entro 15 s → nessun allarme
- sedersi, sdraiarsi lentamente, camminare avanti e indietro → nessun allarme
- luce accesa di colpo → sfondo ricalcolato, nessun allarme
- persona immobile assorbita dallo sfondo → allarme comunque
- l'allarme resta attivo finché non viene gestito

## Limiti noti

- Al buio la telecamera non vede (serve una luce notturna o un modulo IR).
- Con più persone nell'inquadratura il riquadro le unisce: il sistema è pensato per chi vive da solo. Con ospiti usa `/pausa`.
- Una caduta *verso* la telecamera può lasciare la sagoma quasi verticale: monta il dispositivo in un angolo, in modo che i percorsi principali siano di lato.
- Non si vede ciò che è coperto da mobili.

## Possibili evoluzioni

- Modello di classificazione (TensorFlow Lite Micro / ESP‑DL) sulla sagoma per ridurre i falsi allarmi sul letto e sul divano.
- Foto a colori ad alta risoluzione al momento dell'allarme (riconfigurando il sensore).
- Messaggio periodico "dispositivo attivo" e allarme se il dispositivo smette di rispondere (watchdog esterno).
- Integrazione con Home Assistant via MQTT.
