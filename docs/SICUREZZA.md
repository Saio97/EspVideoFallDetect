# Privacy, sicurezza e limiti

## Privacy

- Le immagini vengono elaborate **solo sulla scheda**, a 80×60 pixel in scala di grigi. Non c'è streaming e nulla viene salvato.
- Una foto lascia il dispositivo solo in tre casi: un **allarme** (`SEND_PHOTO_WITH_ALARM`), un comando **`/foto`** da una chat autorizzata, o un'**istantanea** dalla pagina web protetta da password (`ENABLE_WEB_SNAPSHOT`). Ognuna di queste funzioni si può disattivare in `include/config.h`.
- Le foto inviate passano dai server di Telegram.
- **Parlane con la persona sorvegliata** e ottieni il suo consenso. In bagno valuta di disattivare foto e istantanee (`SEND_PHOTO_WITH_ALARM 0`, `ENABLE_WEB_SNAPSHOT 0`): l'allarme testuale arriva comunque.
- Se il dispositivo inquadra spazi in cui entrano altre persone (badanti, ospiti), informale. In Italia valgono le regole del GDPR sulla videosorveglianza domestica.

## Sicurezza

- **Token del bot**: chi lo ha può leggere e inviare messaggi come il bot. Tienilo solo in `include/secrets.h` (escluso da git). Se lo diffondi per errore, rigeneralo con @BotFather (`/revoke`).
- **Chat autorizzate**: solo gli ID in `TELEGRAM_CHAT_IDS` ricevono allarmi e possono dare comandi. Chiunque altro scriva al bot riceve solo l'ID della propria chat.
- **Pagina web**: protetta da autenticazione HTTP Basic e raggiungibile solo dalla rete di casa. Cambia la password predefinita. Non esporla su Internet con il port forwarding.
- **Certificato TLS di Telegram**: la connessione è cifrata, ma il certificato del server non viene verificato (`setInsecure()` in `src/telegram.cpp`), per non dover aggiornare il firmware quando Telegram lo cambia. Su una rete domestica fidata il rischio è basso. Per verificarlo, sostituisci `client.setInsecure()` con `client.setCACert(...)` e il certificato radice di `api.telegram.org`.

## Limiti

Questo **non è un dispositivo medico**. Può non rilevare una caduta (buio, persona fuori inquadratura o coperta da mobili, caduta lenta, Wi‑Fi assente, mancanza di corrente) e può dare falsi allarmi. Usalo insieme ad altre precauzioni, per esempio un pulsante di chiamata da polso, visite e telefonate regolari.

Buone pratiche:
- fai una prova con `/test` ogni settimana
- controlla ogni tanto con `/stato` che il dispositivo sia acceso e connesso
- assicurati che chi riceve gli allarmi sappia cosa fare e abbia le chiavi o un contatto vicino
