# SpotatuiPSP 🎵

Spotify controller homebrew per **Sony PSP**, sviluppato in C con **PSPSDK**.

L'obiettivo di SpotatuiPSP è realizzare un'interfaccia ispirata ai terminali di **Metal Gear Solid / VR Missions** che permetta di controllare Spotify direttamente da PSP.

> **Stato del progetto:** Work in Progress  
> **Versione corrente:** v0.16.5  
> **Checkpoint:** `09edbbc`

---

## 🎯 Obiettivo

SpotatuiPSP nasce come progetto sperimentale per trasformare una PSP in un piccolo controller Spotify.

L'architettura attuale utilizza un backend Python:

```text
PSP / PPSSPP
     │
     │ HTTP
     ▼
Spotatui Backend
   Python
     │
     │ Spotify Web API
     ▼
  Spotify
```

Il backend gestisce autenticazione e comunicazione con Spotify, mentre la PSP si occupa dell'interfaccia e dell'invio dei comandi.

L'obiettivo futuro è ridurre progressivamente la dipendenza dal PC.

---

## 🖥️ Interfaccia

L'interfaccia utilizza la risoluzione nativa PSP:

```text
480 × 272
```

ed è ispirata all'estetica di Metal Gear Solid / VR Missions:

- palette verde fosforo
- sfondo scuro
- effetto CRT / scanlines
- font pixel
- navigazione tramite controlli PSP
- UI progettata specificamente per il display 16:9 della console

Le schermate principali attualmente previste sono:

```text
HOME
SEARCH
PLAYLISTS
LIBRARY
NOW PLAYING
SETTINGS
```

---

## 🧩 Funzionalità

### Implementate

- Interfaccia grafica PSP
- Navigazione tramite controller
- Schermata Settings
- Informazioni batteria
- Stato alimentazione
- Temperatura
- Stato WLAN
- Connessione Wi-Fi tramite Netconf PSP
- Backend HTTP Python
- Connessione alla Spotify Web API
- Autenticazione Spotify
- Ricerca brani
- Tastiera OSK nativa PSP
- Visualizzazione risultati di ricerca
- Avvio di un brano tramite Spotify
- Diagnostica networking PSP/backend

### In sviluppo

- Stabilizzazione networking su hardware PSP
- Comunicazione PSP → backend
- Discovery automatica del backend
- Player controls completi
- Playlist
- Libreria Spotify
- Now Playing
- Refactoring e modularizzazione del codice

---

# 🏗️ Architettura

## PSP

Il client PSP è scritto principalmente in **C** utilizzando PSPSDK.

Componenti utilizzati:

```text
PSPSDK
PSP GU
PSP Utility
PSP Net
PSP Net APCTL
PSP Net INET
PSP Power
```

Il networking viene inizializzato tramite:

```c
pspSdkInetInit();
```

La versione v0.16.5 sta inoltre migrando il test HTTP verso le API INET native PSP:

```c
sceNetInetSocket()
sceNetInetConnect()
sceNetInetSend()
sceNetInetRecv()
sceNetInetClose()
```

Questo lavoro è attualmente in fase di test.

---

## Backend

Il backend è scritto in **Python**.

File principale:

```text
backend/server.py
```

Il server espone un'API HTTP utilizzata dalla PSP.

Porta HTTP:

```text
8080
```

Durante lo sviluppo è stata inoltre sperimentata una discovery UDP sulla porta:

```text
8081
```

### Endpoint principali

```text
/ping

/spotify/login
/spotify/callback
/spotify/status
/spotify/me

/spotify/player
/spotify/search?q=...

/spotify/play-track?id=...

/spotify/play
/spotify/pause
/spotify/next
/spotify/previous

/spotify/devices
```

---

# 🔎 Ricerca Spotify

La PSP può aprire la tastiera OSK nativa per inserire una ricerca.

Il flusso attuale è:

```text
PSP OSK
   │
   ▼
GET /spotify/search?q=...
   │
   ▼
Python Backend
   │
   ▼
Spotify Web API
   │
   ▼
Risultati
   │
   ▼
PSP
```

Il backend restituisce i risultati in un formato semplice:

```text
TITLE|ARTIST|TRACK_ID
```

La PSP può quindi selezionare un risultato e richiedere:

```text
/spotify/play-track?id=TRACK_ID
```

---

# 🌐 Networking

Il networking è attualmente una delle principali aree di sviluppo.

La connessione WLAN utilizza il sistema Netconf della PSP e APCTL.

La sequenza attuale utilizza:

```c
sceUtilityLoadNetModule(...)
pspSdkInetInit()
sceUtilityNetconfInitStart(...)
```

Lo stato della connessione viene verificato tramite APCTL.

Quando viene ottenuto un indirizzo IP:

```text
WLAN ONLINE
```

viene mostrato nella schermata Settings.

---

## Backend discovery

È stata sperimentata una discovery automatica tramite UDP:

```text
PSP
 │
 │ UDP :8081
 ▼
Backend
```

La discovery tramite broadcast si è però dimostrata problematica durante i test con PPSSPP.

Per questo motivo la **v0.16.5 utilizza temporaneamente un indirizzo backend definito durante lo sviluppo**, in modo da isolare e verificare prima la comunicazione TCP.

La discovery automatica verrà reintrodotta dopo la stabilizzazione dei socket.

---

# 🧪 Network Debugging

Durante lo sviluppo sono stati introdotti strumenti diagnostici direttamente nella schermata Settings.

La diagnostica può mostrare informazioni come:

```text
BACKEND 192.168.x.x:8080

SOCKET
CONNECT
SEND
RECV

INET ERRNO

STAGE
CODE
```

Questo permette di identificare precisamente in quale fase della comunicazione si verifica un errore.

---

## Stato networking al checkpoint v0.16.5

Durante i test PPSSPP:

```text
WLAN
  ↓
ONLINE
  ↓
SOCKET
  ↓
CONNECT
  ↓
SEND
  ↓
RECV
```

L'introduzione di:

```c
pspSdkInetInit();
```

ha permesso di superare un precedente problema durante `connect()`.

La v0.16.5 introduce quindi l'utilizzo diretto delle API:

```text
sceNetInet*
```

per continuare il debugging della comunicazione.

Il prossimo test dovrà essere eseguito prima di procedere con ulteriori modifiche alla discovery.

---

# 🛠️ Compilazione

È necessario avere un ambiente PSPSDK configurato.

Esempio:

```bash
export PSPDEV="$HOME/pspdev"
export PATH="$PSPDEV/bin:$PATH"
```

Verifica:

```bash
which psp-config
psp-config --pspsdk-path
```

Quindi:

```bash
cd ~/SpotatuiPSP

make clean
make
```

Al termine viene generato:

```text
EBOOT.PBP
```

---

## PPSSPP

Per i test su Windows:

```bash
cp ~/SpotatuiPSP/EBOOT.PBP /mnt/c/Users/utente/Downloads/EBOOT.PBP
```

Aprire quindi `EBOOT.PBP` tramite PPSSPP.

---

# ⚠️ Warning linker conosciuto

Durante la compilazione può attualmente comparire:

```text
Warning: could not fixup imports, stubs out of order.
Ensure the SDK libraries are linked in last to correct this.
Continuing, your binary may or not work.
```

Questo warning è **conosciuto e non ancora risolto**.

Il Makefile e l'ordine delle librerie dovranno essere verificati prima di considerare stabile una build destinata all'hardware reale.

---

# 📁 Struttura del progetto

Attualmente:

```text
SpotatuiPSP/
│
├── main.c
├── network.h
├── Makefile
├── ICON0.PNG
│
└── backend/
    └── server.py
```

È previsto un refactoring futuro verso una struttura più modulare:

```text
SpotatuiPSP/
│
├── main.c
│
├── ui.c
├── ui.h
│
├── network.c
├── network.h
│
├── spotify.c
├── spotify.h
│
└── backend/
    └── server.py
```

---

# 🔐 Spotify

Il backend utilizza l'autenticazione Spotify e mantiene localmente la sessione/token.

Le credenziali e i token **non devono essere inseriti nel repository**.

File come:

```text
.env
spotify_token.json
```

devono rimanere esclusi tramite `.gitignore`.

---

# 🚧 Roadmap

```text
[✓] UI PSP
[✓] Navigazione
[✓] WLAN
[✓] Backend Python
[✓] Spotify OAuth
[✓] Spotify Search
[✓] PSP OSK
[✓] Play Track
[✓] Network diagnostics

[~] PSP native sockets
[ ] Stabilizzazione PSP ↔ backend
[ ] Backend auto-discovery
[ ] Now Playing
[ ] Play / Pause
[ ] Next / Previous
[ ] Playlist
[ ] Library
[ ] Refactoring main.c
[ ] Test hardware PSP
[ ] HTTPS / standalone research
```

---

# 🔮 Obiettivo futuro

L'architettura attuale è:

```text
PSP
 │
 ▼
Python Backend
 │
 ▼
Spotify Web API
```

L'obiettivo sperimentale a lungo termine è valutare:

```text
PSP
 │
 │ HTTPS
 ▼
Spotify Web API
```

Questo richiederà lo studio di:

- TLS/HTTPS su PSP
- OAuth
- refresh token
- gestione JSON
- persistenza delle credenziali
- compatibilità con la Spotify Web API

---

# 📌 Development checkpoint

Ultimo checkpoint stabile del repository:

```text
09edbbc
SpotatuiPSP v0.16.5 - network and backend checkpoint
```

Checkpoint Spotify precedente:

```text
492f559
SpotatuiPSP v0.11.4 - Spotify integration checkpoint
```

In caso di ripresa dello sviluppo, partire dalla **v0.16.5** e completare prima il test delle API `sceNetInet*`.

---

## Disclaimer

SpotatuiPSP è un progetto homebrew sperimentale e non è affiliato con Sony Interactive Entertainment o Spotify.

Il progetto è attualmente in sviluppo e alcune funzionalità possono essere incomplete o instabili.