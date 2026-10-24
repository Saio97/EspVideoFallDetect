# Taratura

Le soglie si cambiano dalla pagina web (sezione **Taratura**, poi **Salva soglie**). Vengono salvate nella memoria della scheda e restano anche dopo un riavvio. Per tornare ai valori predefiniti tieni premuto il pulsante mentre colleghi l'alimentazione.

Per capire cosa vede il dispositivo usa la pagina web: il valore **Forma (alt./larg.)** e il riquadro verde sull'istantanea. Il log seriale mostra ogni evento (`FALL_SUSPECTED`, `RECOVERED`, `ALARM`).

## Le soglie

| Soglia | Predefinito | Cosa fa |
|---|---|---|
| Sensibilità al contrasto (`diffThreshold`) | 28 | differenza di luminosità minima tra persona e sfondo. Più bassa = vede anche vestiti simili al pavimento, ma più rumore |
| Dimensione minima persona (`minBlobFraction`) | 0,008 | sotto questa frazione dell'immagine la sagoma viene ignorata (animali, ombre piccole) |
| Durata massima della caduta (`fallWindowMs`) | 1500 ms | il passaggio da in piedi a terra deve avvenire entro questo tempo |
| Forma in piedi (`standingAspect`) | 1,25 | rapporto altezza/larghezza sopra il quale la persona è "in piedi" |
| Forma a terra (`lyingAspect`) | 0,85 | rapporto sotto il quale la persona è "a terra" |
| Abbassamento della testa (`fallDropFraction`) | 0,20 | di quanto deve scendere l'alto della sagoma, in frazione dell'altezza dell'immagine |
| Tempo a terra (`groundTimeMs`) | 15000 ms | quanto aspettare prima dell'allarme |
| Tolleranza rialzata (`recoverTopFraction`) | 0,12 | quanto vicino alla posizione di prima deve tornare la testa per considerarla rialzata |

## Problemi tipici

**Una caduta vera non viene rilevata**
- Controlla con l'istantanea che la persona sia inquadrata intera e che il riquadro verde la segua.
- Se in piedi la forma è sotto 1,25 (telecamera troppo alta o troppo di lato): abbassa *Forma in piedi* a 1,1.
- Se a terra la forma resta sopra 0,85 (persona caduta verso la telecamera, in "scorcio"): alza *Forma a terra* a 1,0.
- Se la persona è lenta nel cadere (scivola lungo un mobile): alza *Durata massima della caduta* a 2500 ms.
- Se la persona appare piccola nell'immagine: abbassa *Abbassamento della testa* a 0,15.

**Falsi allarmi**
- Animale domestico: alza *Dimensione minima persona* (es. 0,02).
- Sdraiarsi sul divano o sul letto inquadrato: è il caso più difficile. Prova ad alzare *Abbassamento della testa* a 0,25, a ridurre *Durata massima della caduta* a 1000 ms, oppure sposta l'inquadratura per escludere il letto.
- Cambi di luce (sole, nuvole, lampade): il dispositivo si riadatta da solo. Se il problema persiste alza *Sensibilità al contrasto* a 35–40.

**Allarme troppo lento o troppo veloce**
- *Tempo a terra* da 15 s è un buon compromesso. Abbassalo (es. 8 s) se vuoi un allarme più rapido, alzalo se la persona tende a restare seduta a terra per scelta.

## Consiglio pratico

Dopo l'installazione lascia `NOTIFY_ON_SUSPECTED` a `1` in `config.h` per qualche giorno: riceverai un messaggio anche per ogni caduta *sospetta* che poi si risolve. È il modo più semplice per capire se le soglie sono troppo sensibili. Quando sei soddisfatto riportalo a `0`.
