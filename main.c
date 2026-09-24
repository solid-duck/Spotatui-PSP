#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <psputility_osk.h>
#include <pspsdk.h>
#include <psppower.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>

PSP_MODULE_INFO("SpotatuiPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

#define COLOR_BG     0xFF0B1006
#define COLOR_GREEN  0xFF88FF39
#define COLOR_TEXT   0xFFB5FF8C
#define COLOR_DIM    0xFF3A6B17
#define COLOR_SCAN   0xFF111A08

static unsigned int __attribute__((aligned(16))) list[262144];


/* --------------------------------------------------
   STRUTTURE
-------------------------------------------------- */

typedef struct
{
    unsigned int color;
    float x;
    float y;
    float z;
} Vertex;


typedef enum
{
    SCREEN_HOME,
    SCREEN_SEARCH,
    SCREEN_PLAYLISTS,
    SCREEN_LIBRARY,
    SCREEN_NOW_PLAYING,
    SCREEN_SETTINGS
} AppScreen;


typedef enum
{
    WLAN_OFFLINE,
    WLAN_CONNECTING,
    WLAN_ONLINE,
    WLAN_ERROR
} WlanStatus;


typedef enum
{
    WLAN_ERROR_NONE,
    WLAN_ERROR_COMMON_MODULE,
    WLAN_ERROR_INET_MODULE,
    WLAN_ERROR_NET_INIT,
    WLAN_ERROR_INET_INIT,
    WLAN_ERROR_RESOLVER_INIT,
    WLAN_ERROR_APCTL_INIT,
    WLAN_ERROR_NETCONF,
    WLAN_ERROR_AP_CONNECT,
    WLAN_ERROR_AP_STATE,
    WLAN_ERROR_TIMEOUT,
    WLAN_ERROR_THREAD_CREATE,
    WLAN_ERROR_THREAD_START
} WlanErrorStage;


typedef enum
{
    HTTP_IDLE,
    HTTP_CONNECTING,
    HTTP_OK,
    HTTP_ERROR
} HttpStatus;


typedef enum
{
    HTTP_ERROR_NONE,
    HTTP_ERROR_WLAN_OFFLINE,
    HTTP_ERROR_MODULE_URI,
    HTTP_ERROR_MODULE_PARSE,
    HTTP_ERROR_MODULE_LOAD,
    HTTP_ERROR_INIT,
    HTTP_ERROR_TEMPLATE,
    HTTP_ERROR_CONNECTION,
    HTTP_ERROR_REQUEST,
    HTTP_ERROR_SEND,
    HTTP_ERROR_STATUS,
    HTTP_ERROR_THREAD_CREATE,
    HTTP_ERROR_THREAD_START
} HttpErrorStage;

typedef enum
{
    SPOTIFY_UNKNOWN,
    SPOTIFY_CHECKING,
    SPOTIFY_CONNECTED,
    SPOTIFY_DISCONNECTED,
    SPOTIFY_ERROR
} SpotifyStatus;


/* --------------------------------------------------
   STATO APPLICAZIONE
-------------------------------------------------- */

AppScreen currentScreen = SCREEN_HOME;

int selectedMenu = 0;
int selectedSetting = 0;
int settingsScroll = 0;

volatile WlanStatus wlanStatus = WLAN_OFFLINE;
volatile int wlanThreadRunning = 0;
volatile WlanErrorStage wlanErrorStage = WLAN_ERROR_NONE;
volatile int wlanErrorCode = 0;

char wlanIp[32] = "";


#define BACKEND_HTTP_PORT 8080
#define BACKEND_DISCOVERY_PORT 8081

char backendIp[32] = "";
volatile int backendDiscovered = 0;
volatile int backendConnectError = 0;


volatile HttpStatus httpStatus = HTTP_IDLE;
volatile HttpErrorStage httpErrorStage = HTTP_ERROR_NONE;
volatile int httpErrorCode = 0;
volatile int httpThreadRunning = 0;
volatile int httpResponseCode = 0;

/* v0.16.4 NETWORK TRACE */
volatile int traceSocketResult = -9999;
volatile int traceConnectResult = -9999;
volatile int traceSendResult = -9999;
volatile int traceSendExpected = 0;
volatile int traceRecvResult = -9999;
volatile int traceInetErrno = 0;
char traceBackendIp[32] = "";

volatile SpotifyStatus spotifyStatus = SPOTIFY_UNKNOWN;
volatile int spotifyThreadRunning = 0;
volatile int spotifyResponseCode = 0;
char spotifyUser[64] = "";


#define SEARCH_MAX_RESULTS 5

typedef struct
{
    char title[64];
    char artist[64];
    char id[32];
} SearchResult;

SearchResult searchResults[SEARCH_MAX_RESULTS];
volatile int searchResultCount = 0;
volatile int searchSelected = 0;
volatile int searchThreadRunning = 0;
volatile int searchPlayRunning = 0;
volatile int searchStatusCode = 0;
char searchQuery[64] = "";
char searchMessage[64] = "PRESS X TO SEARCH";
char pendingTrackId[32] = "";

volatile int playerThreadRunning = 0;
volatile int playerLoaded = 0;
volatile int playerIsPlaying = 0;
volatile int playerProgressMs = 0;
volatile int playerDurationMs = 0;
char playerTrack[64] = "NO TRACK";
char playerArtist[64] = "PLAYER STANDBY";
volatile unsigned int playerLastSyncUs = 0;
volatile int playerMonitorRunning = 0;

typedef enum
{
    PLAYER_CMD_NONE,
    PLAYER_CMD_PLAY_PAUSE,
    PLAYER_CMD_NEXT,
    PLAYER_CMD_PREVIOUS
} PlayerCommand;

volatile PlayerCommand pendingPlayerCommand = PLAYER_CMD_NONE;
volatile int playerCommandRunning = 0;

int bootActive = 1;
int bootFrame = 0;




/* --------------------------------------------------
   FONT BITMAP 5x7
-------------------------------------------------- */

const unsigned char *getCharacter(char c)
{
    static const unsigned char SPACE[7] = {
        0, 0, 0, 0, 0, 0, 0
    };

    static const unsigned char A[7] = {
        14, 17, 17, 31, 17, 17, 17
    };

    static const unsigned char B[7] = {
        30, 17, 17, 30, 17, 17, 30
    };

    static const unsigned char C[7] = {
        14, 17, 16, 16, 16, 17, 14
    };

    static const unsigned char D[7] = {
        30, 17, 17, 17, 17, 17, 30
    };

    static const unsigned char E[7] = {
        31, 16, 16, 30, 16, 16, 31
    };

    static const unsigned char F[7] = {
        31, 16, 16, 30, 16, 16, 16
    };

    static const unsigned char G[7] = {
        14, 17, 16, 23, 17, 17, 14
    };

    static const unsigned char H[7] = {
        17, 17, 17, 31, 17, 17, 17
    };

    static const unsigned char I[7] = {
        31, 4, 4, 4, 4, 4, 31
    };

    static const unsigned char J[7] = {
        7, 2, 2, 2, 18, 18, 12
    };

    static const unsigned char K[7] = {
        17, 18, 20, 24, 20, 18, 17
    };

    static const unsigned char L[7] = {
        16, 16, 16, 16, 16, 16, 31
    };

    static const unsigned char M[7] = {
        17, 27, 21, 21, 17, 17, 17
    };

    static const unsigned char N[7] = {
        17, 25, 21, 19, 17, 17, 17
    };

    static const unsigned char O[7] = {
        14, 17, 17, 17, 17, 17, 14
    };

    static const unsigned char P[7] = {
        30, 17, 17, 30, 16, 16, 16
    };

    static const unsigned char Q[7] = {
        14, 17, 17, 17, 21, 18, 13
    };

    static const unsigned char R[7] = {
        30, 17, 17, 30, 20, 18, 17
    };

    static const unsigned char S[7] = {
        15, 16, 16, 14, 1, 1, 30
    };

    static const unsigned char T[7] = {
        31, 4, 4, 4, 4, 4, 4
    };

    static const unsigned char U[7] = {
        17, 17, 17, 17, 17, 17, 14
    };

    static const unsigned char V[7] = {
        17, 17, 17, 17, 17, 10, 4
    };

    static const unsigned char W[7] = {
        17, 17, 17, 21, 21, 21, 10
    };

    static const unsigned char X[7] = {
        17, 17, 10, 4, 10, 17, 17
    };

    static const unsigned char Y[7] = {
        17, 17, 10, 4, 4, 4, 4
    };

    static const unsigned char Z[7] = {
        31, 1, 2, 4, 8, 16, 31
    };

    static const unsigned char ZERO[7] = {
        14, 17, 19, 21, 25, 17, 14
    };

    static const unsigned char ONE[7] = {
        4, 12, 4, 4, 4, 4, 14
    };

    static const unsigned char TWO[7] = {
        14, 17, 1, 2, 4, 8, 31
    };

    static const unsigned char THREE[7] = {
        30, 1, 1, 14, 1, 1, 30
    };

    static const unsigned char FOUR[7] = {
        2, 6, 10, 18, 31, 2, 2
    };

    static const unsigned char FIVE[7] = {
        31, 16, 16, 30, 1, 1, 30
    };

    static const unsigned char SIX[7] = {
        14, 16, 16, 30, 17, 17, 14
    };

    static const unsigned char SEVEN[7] = {
        31, 1, 2, 4, 8, 8, 8
    };

    static const unsigned char EIGHT[7] = {
        14, 17, 17, 14, 17, 17, 14
    };

    static const unsigned char NINE[7] = {
        14, 17, 17, 15, 1, 1, 14
    };

    static const unsigned char SLASH[7] = {
        1, 2, 2, 4, 8, 8, 16
    };

    static const unsigned char COLON[7] = {
        0, 4, 4, 0, 4, 4, 0
    };

    static const unsigned char DOT[7] = {
        0, 0, 0, 0, 0, 12, 12
    };

    static const unsigned char LEFT_BRACKET[7] = {
        14, 8, 8, 8, 8, 8, 14
    };

    static const unsigned char RIGHT_BRACKET[7] = {
        14, 2, 2, 2, 2, 2, 14
    };

    static const unsigned char DASH[7] = {
        0, 0, 0, 31, 0, 0, 0
    };

    static const unsigned char GREATER[7] = {
        16, 8, 4, 2, 4, 8, 16
    };

    switch (c)
    {
        case 'A': return A;
        case 'B': return B;
        case 'C': return C;
        case 'D': return D;
        case 'E': return E;
        case 'F': return F;
        case 'G': return G;
        case 'H': return H;
        case 'I': return I;
        case 'J': return J;
        case 'K': return K;
        case 'L': return L;
        case 'M': return M;
        case 'N': return N;
        case 'O': return O;
        case 'P': return P;
        case 'Q': return Q;
        case 'R': return R;
        case 'S': return S;
        case 'T': return T;
        case 'U': return U;
        case 'V': return V;
        case 'W': return W;
        case 'X': return X;
        case 'Y': return Y;
        case 'Z': return Z;

        case '0': return ZERO;
        case '1': return ONE;
        case '2': return TWO;
        case '3': return THREE;
        case '4': return FOUR;
        case '5': return FIVE;
        case '6': return SIX;
        case '7': return SEVEN;
        case '8': return EIGHT;
        case '9': return NINE;

        case '/': return SLASH;
        case ':': return COLON;
        case '.': return DOT;
        case '[': return LEFT_BRACKET;
        case ']': return RIGHT_BRACKET;
        case '-': return DASH;
        case '>': return GREATER;

        default: return SPACE;
    }
}


/* --------------------------------------------------
   GRAFICA
-------------------------------------------------- */

void initGraphics(void)
{
    void *fbp0;
    void *fbp1;
    void *zbp;

    sceGuInit();

    /*
       PSP framebuffer pitch is 512 pixels even though the visible
       display is 480 pixels wide. Allocate VRAM using the same pitch
       passed to sceGuDrawBuffer/sceGuDispBuffer, otherwise the buffers
       overlap and the beginning of the frame can reappear at the end.
    */
    fbp0 = guGetStaticVramBuffer(
        512,
        SCREEN_HEIGHT,
        GU_PSM_8888
    );

    fbp1 = guGetStaticVramBuffer(
        512,
        SCREEN_HEIGHT,
        GU_PSM_8888
    );

    zbp = guGetStaticVramBuffer(
        512,
        SCREEN_HEIGHT,
        GU_PSM_4444
    );

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        fbp0,
        512
    );

    sceGuDispBuffer(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        fbp1,
        512
    );

    sceGuDepthBuffer(
        zbp,
        512
    );

    sceGuOffset(
        2048 - (SCREEN_WIDTH / 2),
        2048 - (SCREEN_HEIGHT / 2)
    );

    sceGuViewport(
        2048,
        2048,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    sceGuScissor(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();

    sceGuSync(
        GU_SYNC_FINISH,
        GU_SYNC_WHAT_DONE
    );

    sceDisplayWaitVblankStart();

    sceGuDisplay(GU_TRUE);
}


/* --------------------------------------------------
   PRIMITIVE 2D
-------------------------------------------------- */

void drawLine(
    float x1,
    float y1,
    float x2,
    float y2,
    unsigned int color
)
{
    Vertex *vertices;

    vertices = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    vertices[0].color = color;
    vertices[0].x = x1;
    vertices[0].y = y1;
    vertices[0].z = 0.0f;

    vertices[1].color = color;
    vertices[1].x = x2;
    vertices[1].y = y2;
    vertices[1].z = 0.0f;

    sceGuDrawArray(
        GU_LINES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        NULL,
        vertices
    );
}


void drawRectangle(
    float x,
    float y,
    float width,
    float height,
    unsigned int color
)
{
    drawLine(x, y, x + width, y, color);
    drawLine(x + width, y, x + width, y + height, color);
    drawLine(x + width, y + height, x, y + height, color);
    drawLine(x, y + height, x, y, color);
}


void drawFilledRectangle(float x, float y, float width, float height, unsigned int color)
{
    Vertex *vertices = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;

    vertices[1].color = color;
    vertices[1].x = x + width;
    vertices[1].y = y + height;
    vertices[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2,
        NULL,
        vertices
    );
}


void drawPixel(
    float x,
    float y,
    float size,
    unsigned int color
)
{
    Vertex *vertices;

    vertices = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;

    vertices[1].color = color;
    vertices[1].x = x + size;
    vertices[1].y = y + size;
    vertices[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        NULL,
        vertices
    );
}


/* --------------------------------------------------
   TESTO
-------------------------------------------------- */

void drawCharacter(
    float x,
    float y,
    char character,
    float scale,
    unsigned int color
)
{
    const unsigned char *bitmap;

    int row;
    int column;

    bitmap = getCharacter(character);

    for (row = 0; row < 7; row++)
    {
        for (column = 0; column < 5; column++)
        {
            if (
                bitmap[row] &
                (1 << (4 - column))
            )
            {
                drawPixel(
                    x + (column * scale),
                    y + (row * scale),
                    scale,
                    color
                );
            }
        }
    }
}


void drawText(
    float x,
    float y,
    const char *text,
    float scale,
    unsigned int color
)
{
    int i = 0;

    while (text[i] != '\0')
    {
        drawCharacter(
            x,
            y,
            text[i],
            scale,
            color
        );

        x += 6.0f * scale;

        i++;
    }
}


/* --------------------------------------------------
   WLAN
-------------------------------------------------- */

const char *getWlanStatusText(void)
{
    switch (wlanStatus)
    {
        case WLAN_CONNECTING:
            return "CONNECTING";

        case WLAN_ONLINE:
            return "ONLINE";

        case WLAN_ERROR:
            return "ERROR";

        case WLAN_OFFLINE:
        default:
            return "OFFLINE";
    }
}


unsigned int getWlanStatusColor(void)
{
    if (wlanStatus == WLAN_ONLINE)
    {
        return COLOR_GREEN;
    }

    if (wlanStatus == WLAN_CONNECTING)
    {
        return COLOR_TEXT;
    }

    return COLOR_DIM;
}


const char *getWlanErrorStageText(void)
{
    switch (wlanErrorStage)
    {
        case WLAN_ERROR_COMMON_MODULE:
            return "MODULE COMMON";

        case WLAN_ERROR_INET_MODULE:
            return "MODULE INET";

        case WLAN_ERROR_NET_INIT:
            return "NET INIT";

        case WLAN_ERROR_INET_INIT:
            return "INET INIT";

        case WLAN_ERROR_RESOLVER_INIT:
            return "RESOLVER INIT";

        case WLAN_ERROR_APCTL_INIT:
            return "APCTL INIT";

        case WLAN_ERROR_NETCONF:
            return "NETCONF";

        case WLAN_ERROR_AP_CONNECT:
            return "AP CONNECT";

        case WLAN_ERROR_AP_STATE:
            return "AP STATE";

        case WLAN_ERROR_TIMEOUT:
            return "TIMEOUT";

        case WLAN_ERROR_THREAD_CREATE:
            return "THREAD CREATE";

        case WLAN_ERROR_THREAD_START:
            return "THREAD START";

        case WLAN_ERROR_NONE:
        default:
            return "NONE";
    }
}


void setWlanError(
    WlanErrorStage stage,
    int code
)
{
    wlanErrorStage = stage;
    wlanErrorCode = code;
    wlanStatus = WLAN_ERROR;
    wlanThreadRunning = 0;
}


/*
   Questo thread si occupa solamente della rete.

   In questo modo il ciclo principale può continuare
   a disegnare l'interfaccia mentre la PSP tenta
   di collegarsi al Wi-Fi.
*/
int networkStackInitialized = 0;

int initNetworkStack(void)
{
    int result;

    if (networkStackInitialized)
        return 1;

    result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (result < 0)
    {
        setWlanError(WLAN_ERROR_COMMON_MODULE, result);
        return 0;
    }

    result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (result < 0)
    {
        setWlanError(WLAN_ERROR_INET_MODULE, result);
        return 0;
    }

    /*
       v0.16.3 NETFIX

       Usa l'inizializzazione INET fornita direttamente dal PSPSDK.
       pspSdkInetInit() inizializza lo stack Net/Inet/Resolver/APCTL
       con i parametri previsti dal SDK, evitando di mantenere qui
       una seconda sequenza manuale divergente.
    */
    result = pspSdkInetInit();

    if (result < 0)
    {
        setWlanError(WLAN_ERROR_NET_INIT, result);
        return 0;
    }

    networkStackInitialized = 1;
    return 1;
}

int updateWlanFromApctl(void)
{
    int state = 0;
    int result;
    union SceNetApctlInfo info;

    if (!networkStackInitialized)
        return 0;

    result = sceNetApctlGetState(&state);

    if (result != 0)
    {
        wlanErrorStage = WLAN_ERROR_AP_STATE;
        wlanErrorCode = result;
        return 0;
    }

    if (state != 4)
        return 0;

    memset(&info, 0, sizeof(info));

    if (sceNetApctlGetInfo(8, &info) == 0)
    {
        strncpy(
            wlanIp,
            info.ip,
            sizeof(wlanIp) - 1
        );

        wlanIp[sizeof(wlanIp) - 1] = '\0';
    }
    else
    {
        wlanIp[0] = '\0';
    }

    wlanStatus = WLAN_ONLINE;
    wlanErrorStage = WLAN_ERROR_NONE;
    wlanErrorCode = 0;
    wlanThreadRunning = 0;

    return 1;
}


/*
   Apre il dialogo di rete ufficiale della PSP.

   Spotatui non sceglie piu' un profilo numerico con
   sceNetApctlConnect(1). La PSP mostra invece le connessioni
   salvate e gestisce autonomamente quale configurazione usare.

   Se APCTL e' gia' nello stato 4, il dialogo non viene aperto:
   Spotatui usa direttamente la connessione gia' disponibile.
*/
int openNetworkDialog(void)
{
    pspUtilityNetconfData data;
    struct pspUtilityNetconfAdhoc adhocparam;
    int status;
    int done = 0;
    int initResult;

    memset(&data, 0, sizeof(data));
    memset(&adhocparam, 0, sizeof(adhocparam));

    data.base.size = sizeof(data);
    data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    data.base.graphicsThread = 17;
    data.base.accessThread = 19;
    data.base.fontThread = 18;
    data.base.soundThread = 16;

    data.action = PSP_NETCONF_ACTION_CONNECTAP;
    data.adhocparam = &adhocparam;

    initResult = sceUtilityNetconfInitStart(&data);

    if (initResult < 0)
    {
        setWlanError(WLAN_ERROR_NETCONF, initResult);
        return 0;
    }

    while (!done)
    {
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(COLOR_BG);
        sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

        status = sceUtilityNetconfGetStatus();

        switch (status)
        {
            case PSP_UTILITY_DIALOG_VISIBLE:
                sceUtilityNetconfUpdate(1);
                break;

            case PSP_UTILITY_DIALOG_QUIT:
                sceUtilityNetconfShutdownStart();
                break;

            case PSP_UTILITY_DIALOG_FINISHED:
                done = 1;
                break;

            case PSP_UTILITY_DIALOG_NONE:
                break;

            default:
                break;
        }

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 1;
}


/*
   Connessione WLAN dinamica.

   1. Inizializza lo stack di rete.
   2. Se esiste gia' una connessione APCTL completa, la usa.
   3. Altrimenti apre il Netconf ufficiale della PSP.
   4. Dopo il dialogo controlla lo stato e recupera l'IP.

   Nessun numero di profilo e' hardcoded.
*/
void startWlanConnection(void)
{
    int attempts = 0;

    if (wlanThreadRunning)
        return;

    if (wlanStatus == WLAN_ONLINE && updateWlanFromApctl())
        return;

    wlanStatus = WLAN_CONNECTING;
    wlanErrorStage = WLAN_ERROR_NONE;
    wlanErrorCode = 0;
    wlanIp[0] = '\0';

    backendIp[0] = '\0';
    backendDiscovered = 0;

    wlanThreadRunning = 1;

    if (!initNetworkStack())
    {
        wlanThreadRunning = 0;
        return;
    }

    /*
       Caso ideale: APCTL e' gia' connesso.
       Non chiediamo nulla all'utente e usiamo la rete corrente.
    */
    if (updateWlanFromApctl())
        return;

    /*
       Nessuna connessione APCTL attiva.
       Lasciamo che sia il firmware PSP a gestire la connessione,
       invece di imporre sceNetApctlConnect(1).
    */
    if (!openNetworkDialog())
    {
        wlanThreadRunning = 0;
        return;
    }

    /*
       Il dialogo puo' chiudersi poco prima del passaggio definitivo
       allo stato 4. Attendiamo per un massimo di circa 10 secondi.
    */
    while (attempts < 200)
    {
        if (updateWlanFromApctl())
            return;

        sceKernelDelayThread(50 * 1000);
        attempts++;
    }

    setWlanError(
        WLAN_ERROR_TIMEOUT,
        0
    );
}



/* --------------------------------------------------
   BACKEND AUTO DISCOVERY v0.16.1

   Nessun IP del PC e' hardcoded.

   Strategia:
   1. broadcast globale 255.255.255.255:8081
   2. broadcast diretto della /24 ricavata dall'IP WLAN
      (es. 192.168.137.42 -> 192.168.137.255)
   3. il backend risponde "SPOTATUI_BACKEND"
   4. Spotatui usa automaticamente l'IP del mittente
-------------------------------------------------- */

int receiveBackendDiscovery(int socketId, int timeoutMs)
{
    int result;
    int received;
    fd_set readSet;
    struct timeval timeout;
    struct sockaddr_in senderAddress;
    socklen_t senderLength;
    char response[96];

    FD_ZERO(&readSet);
    FD_SET(socketId, &readSet);

    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    result = select(socketId + 1, &readSet, NULL, NULL, &timeout);

    if (result < 0)
    {
        backendConnectError = errno;
        return 0;
    }

    if (result == 0 || !FD_ISSET(socketId, &readSet))
        return 0;

    senderLength = sizeof(senderAddress);
    memset(&senderAddress, 0, sizeof(senderAddress));
    memset(response, 0, sizeof(response));

    received = recvfrom(
        socketId,
        response,
        sizeof(response) - 1,
        0,
        (struct sockaddr *)&senderAddress,
        &senderLength
    );

    if (received < 0)
    {
        backendConnectError = errno;
        return 0;
    }

    if (received == 0)
        return 0;

    response[received] = '\0';

    if (strncmp(response, "SPOTATUI_BACKEND", 16) != 0)
        return 0;

    {
        const char *ip = inet_ntoa(senderAddress.sin_addr);

        if (ip == NULL || ip[0] == '\0')
            return 0;

        strncpy(backendIp, ip, sizeof(backendIp) - 1);
        backendIp[sizeof(backendIp) - 1] = '\0';
    }

    backendDiscovered = 1;
    backendConnectError = 0;
    return 1;
}

int sendBackendDiscovery(int socketId, const char *targetIp)
{
    struct sockaddr_in address;
    const char message[] = "SPOTATUI_DISCOVER";
    int result;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(BACKEND_DISCOVERY_PORT);
    address.sin_addr.s_addr = inet_addr(targetIp);

    result = sendto(
        socketId,
        message,
        strlen(message),
        0,
        (struct sockaddr *)&address,
        sizeof(address)
    );

    if (result < 0)
    {
        backendConnectError = errno;
        return 0;
    }

    return 1;
}

int getLocal24Broadcast(char *broadcastIp, int size)
{
    int a;
    int b;
    int c;
    int d;

    if (wlanIp[0] == '\0')
        return 0;

    if (sscanf(wlanIp, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
        return 0;

    if (a < 0 || a > 255 ||
        b < 0 || b > 255 ||
        c < 0 || c > 255 ||
        d < 0 || d > 255)
    {
        return 0;
    }

    snprintf(
        broadcastIp,
        size,
        "%d.%d.%d.255",
        a,
        b,
        c
    );

    return 1;
}

int discoverBackend(void)
{
    /*
       v0.16.2 TEST
       Bypass temporaneo della discovery UDP.

       Serve esclusivamente a verificare se PPSSPP riesce a
       raggiungere il backend Windows tramite TCP diretto.
    */
    const char *testIp = "192.168.137.155";

    if (wlanStatus != WLAN_ONLINE)
    {
        backendConnectError = 0;
        return 0;
    }

    backendIp[0] = '\0';
    backendDiscovered = 0;
    backendConnectError = 0;

    strncpy(backendIp, testIp, sizeof(backendIp) - 1);
    backendIp[sizeof(backendIp) - 1] = '\0';
    backendDiscovered = 1;

    return 1;
}

int ensureBackend(void)
{
    if (backendDiscovered && backendIp[0] != '\0')
        return 1;

    return discoverBackend();
}


/* --------------------------------------------------
   SERVER TEST - TCP SOCKET
-------------------------------------------------- */

const char *getHttpStatusText(void)
{
    switch (httpStatus)
    {
        case HTTP_CONNECTING:
            return "CONNECTING";
        case HTTP_OK:
            return "OK";
        case HTTP_ERROR:
            return "ERROR";
        case HTTP_IDLE:
        default:
            return "READY";
    }
}

unsigned int getHttpStatusColor(void)
{
    if (httpStatus == HTTP_OK) return COLOR_GREEN;
    if (httpStatus == HTTP_CONNECTING) return COLOR_TEXT;
    return COLOR_DIM;
}

const char *getHttpErrorStageText(void)
{
    switch (httpErrorStage)
    {
        case HTTP_ERROR_WLAN_OFFLINE: return "WLAN OFFLINE";
        case HTTP_ERROR_CONNECTION: return "SOCKET CONNECT";
        case HTTP_ERROR_SEND: return "SOCKET SEND";
        case HTTP_ERROR_STATUS: return "SOCKET RECV";
        case HTTP_ERROR_THREAD_CREATE: return "THREAD CREATE";
        case HTTP_ERROR_THREAD_START: return "THREAD START";
        case HTTP_ERROR_NONE:
        default: return "NONE";
    }
}

void setHttpError(HttpErrorStage stage, int code)
{
    httpErrorStage = stage;
    httpErrorCode = code;
    httpStatus = HTTP_ERROR;
    httpThreadRunning = 0;
}

int httpThread(SceSize args, void *argp)
{
    int socketId;
    int connectResult;
    size_t sendResult;
    size_t recvResult;
    size_t totalReceived = 0;
    int statusCode = 0;
    size_t requestLength;
    struct sockaddr_in serverAddress;
    char response[1024];
    char request[256];

    httpThreadRunning = 1;
    httpStatus = HTTP_CONNECTING;
    httpErrorStage = HTTP_ERROR_NONE;
    httpErrorCode = 0;
    httpResponseCode = 0;

    traceSocketResult = -9999;
    traceConnectResult = -9999;
    traceSendResult = -9999;
    traceSendExpected = 0;
    traceRecvResult = -9999;
    traceInetErrno = 0;
    traceBackendIp[0] = '\0';

    if (wlanStatus != WLAN_ONLINE)
    {
        setHttpError(HTTP_ERROR_WLAN_OFFLINE, 0);
        return 0;
    }

    if (!ensureBackend())
    {
        traceInetErrno = backendConnectError;
        setHttpError(HTTP_ERROR_CONNECTION, backendConnectError);
        return 0;
    }

    strncpy(traceBackendIp, backendIp, sizeof(traceBackendIp) - 1);
    traceBackendIp[sizeof(traceBackendIp) - 1] = '\0';

    socketId = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    traceSocketResult = socketId;

    if (socketId < 0)
    {
        traceInetErrno = sceNetInetGetErrno();
        setHttpError(HTTP_ERROR_CONNECTION, traceInetErrno);
        return 0;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(BACKEND_HTTP_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(backendIp);

    connectResult = sceNetInetConnect(
        socketId,
        (struct sockaddr *)&serverAddress,
        sizeof(serverAddress)
    );

    traceConnectResult = connectResult;

    if (connectResult < 0)
    {
        traceInetErrno = sceNetInetGetErrno();
        sceNetInetClose(socketId);
        setHttpError(HTTP_ERROR_CONNECTION, traceInetErrno);
        return 0;
    }

    snprintf(
        request,
        sizeof(request),
        "GET /ping HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: SpotatuiPSP/0.16.5-NATIVE\r\n"
        "Connection: close\r\n"
        "\r\n",
        backendIp,
        BACKEND_HTTP_PORT
    );

    requestLength = strlen(request);
    traceSendExpected = (int)requestLength;

    sendResult = sceNetInetSend(
        socketId,
        request,
        requestLength,
        0
    );

    if (sendResult == (size_t)-1)
    {
        traceSendResult = -1;
        traceInetErrno = sceNetInetGetErrno();
        sceNetInetClose(socketId);
        setHttpError(HTTP_ERROR_SEND, traceInetErrno);
        return 0;
    }

    traceSendResult = (int)sendResult;

    /*
       Una send TCP può inviare meno byte di quelli richiesti.
       Completiamo quindi esplicitamente l'intera richiesta HTTP.
    */
    while (sendResult < requestLength)
    {
        size_t moreSent = sceNetInetSend(
            socketId,
            request + sendResult,
            requestLength - sendResult,
            0
        );

        if (moreSent == (size_t)-1)
        {
            traceSendResult = -1;
            traceInetErrno = sceNetInetGetErrno();
            sceNetInetClose(socketId);
            setHttpError(HTTP_ERROR_SEND, traceInetErrno);
            return 0;
        }

        if (moreSent == 0)
        {
            traceInetErrno = 0;
            sceNetInetClose(socketId);
            setHttpError(HTTP_ERROR_SEND, 0);
            return 0;
        }

        sendResult += moreSent;
        traceSendResult = (int)sendResult;
    }

    memset(response, 0, sizeof(response));

    while (totalReceived < sizeof(response) - 1)
    {
        recvResult = sceNetInetRecv(
            socketId,
            response + totalReceived,
            sizeof(response) - 1 - totalReceived,
            0
        );

        if (recvResult == (size_t)-1)
        {
            traceRecvResult = -1;
            traceInetErrno = sceNetInetGetErrno();
            sceNetInetClose(socketId);
            setHttpError(HTTP_ERROR_STATUS, traceInetErrno);
            return 0;
        }

        traceRecvResult = (int)recvResult;

        if (recvResult == 0)
            break;

        totalReceived += recvResult;
    }

    response[totalReceived] = '\0';
    sceNetInetClose(socketId);

    if (sscanf(response, "HTTP/%*s %d", &statusCode) != 1)
    {
        traceInetErrno = -1;
        setHttpError(HTTP_ERROR_STATUS, -1);
        return 0;
    }

    httpResponseCode = statusCode;

    if (statusCode == 200 && strstr(response, "SPOTATUI SERVER ONLINE") != NULL)
    {
        httpStatus = HTTP_OK;
        httpThreadRunning = 0;
        return 0;
    }

    setHttpError(HTTP_ERROR_STATUS, statusCode);
    return 0;
}

void startHttpTest(void)
{
    SceUID threadId;

    if (httpThreadRunning) return;

    httpStatus = HTTP_CONNECTING;
    httpErrorStage = HTTP_ERROR_NONE;
    httpErrorCode = 0;
    httpResponseCode = 0;
    httpThreadRunning = 1;

    threadId = sceKernelCreateThread(
        "SpotatuiServerThread",
        httpThread,
        0x11,
        16 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId < 0)
    {
        setHttpError(HTTP_ERROR_THREAD_CREATE, threadId);
        return;
    }

    {
        int startResult = sceKernelStartThread(threadId, 0, NULL);
        if (startResult < 0)
            setHttpError(HTTP_ERROR_THREAD_START, startResult);
    }
}


/* --------------------------------------------------
   SPOTIFY - BACKEND STATUS / ACCOUNT
-------------------------------------------------- */

const char *getSpotifyStatusText(void)
{
    switch (spotifyStatus)
    {
        case SPOTIFY_CHECKING: return "CHECKING";
        case SPOTIFY_CONNECTED: return "CONNECTED";
        case SPOTIFY_DISCONNECTED: return "DISCONNECTED";
        case SPOTIFY_ERROR: return "ERROR";
        case SPOTIFY_UNKNOWN:
        default: return "UNKNOWN";
    }
}

unsigned int getSpotifyStatusColor(void)
{
    if (spotifyStatus == SPOTIFY_CONNECTED) return COLOR_GREEN;
    if (spotifyStatus == SPOTIFY_CHECKING) return COLOR_TEXT;
    return COLOR_DIM;
}

int backendCommand(
    const char *method,
    const char *path,
    char *response,
    int responseSize,
    int *statusCode
)
{
    int socketId;
    int result;
    int received;
    int totalReceived = 0;
    struct sockaddr_in serverAddress;
    char request[512];

    if (wlanStatus != WLAN_ONLINE) return -1;

    if (!ensureBackend()) return -7;

    socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (socketId < 0) return -2;

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(BACKEND_HTTP_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(backendIp);

    result = connect(
        socketId,
        (struct sockaddr *)&serverAddress,
        sizeof(serverAddress)
    );

    if (result < 0)
    {
        close(socketId);
        return -3;
    }

    snprintf(
        request,
        sizeof(request),
        "%s %s HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: SpotatuiPSP/0.16.5-NATIVE\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        method,
        path,
        backendIp,
        BACKEND_HTTP_PORT
    );

    result = send(socketId, request, strlen(request), 0);
    if (result < 0)
    {
        close(socketId);
        return -4;
    }

    memset(response, 0, responseSize);

    while (totalReceived < responseSize - 1)
    {
        received = recv(
            socketId,
            response + totalReceived,
            responseSize - 1 - totalReceived,
            0
        );

        if (received < 0)
        {
            close(socketId);
            return -5;
        }

        if (received == 0) break;
        totalReceived += received;
    }

    response[totalReceived] = '\0';
    close(socketId);

    if (sscanf(response, "HTTP/%*s %d", statusCode) != 1)
        return -6;

    return 0;
}


int backendGet(const char *path, char *response, int responseSize, int *statusCode)
{
    int socketId;
    int result;
    int received;
    int totalReceived = 0;
    struct sockaddr_in serverAddress;
    char request[384];

    if (wlanStatus != WLAN_ONLINE) return -1;

    if (!ensureBackend()) return -7;

    socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (socketId < 0) return -2;

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(BACKEND_HTTP_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(backendIp);

    result = connect(
        socketId,
        (struct sockaddr *)&serverAddress,
        sizeof(serverAddress)
    );

    if (result < 0)
    {
        close(socketId);
        return -3;
    }

    snprintf(
        request,
        sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: SpotatuiPSP/0.16.5-NATIVE\r\n"
        "Connection: close\r\n"
        "\r\n",
        path,
        backendIp,
        BACKEND_HTTP_PORT
    );

    result = send(socketId, request, strlen(request), 0);
    if (result < 0)
    {
        close(socketId);
        return -4;
    }

    memset(response, 0, responseSize);

    while (totalReceived < responseSize - 1)
    {
        received = recv(
            socketId,
            response + totalReceived,
            responseSize - 1 - totalReceived,
            0
        );

        if (received < 0)
        {
            close(socketId);
            return -5;
        }

        if (received == 0) break;
        totalReceived += received;
    }

    response[totalReceived] = '\0';
    close(socketId);

    if (sscanf(response, "HTTP/%*s %d", statusCode) != 1) return -6;

    return 0;
}

const char *getHttpBody(char *response)
{
    char *body = strstr(response, "\r\n\r\n");
    if (body == NULL) return "";
    return body + 4;
}

int spotifyThread(SceSize args, void *argp)
{
    char response[2048];
    int statusCode = 0;
    int result;
    const char *body;

    spotifyThreadRunning = 1;
    spotifyStatus = SPOTIFY_CHECKING;
    spotifyResponseCode = 0;
    spotifyUser[0] = '\0';

    result = backendGet(
        "/spotify/status",
        response,
        sizeof(response),
        &statusCode
    );

    spotifyResponseCode = statusCode;

    if (result < 0 || statusCode != 200)
    {
        spotifyStatus = SPOTIFY_ERROR;
        spotifyThreadRunning = 0;
        return 0;
    }

    body = getHttpBody(response);

    if (strncmp(body, "CONNECTED", 9) != 0)
    {
        spotifyStatus = SPOTIFY_DISCONNECTED;
        spotifyThreadRunning = 0;
        return 0;
    }

    result = backendGet(
        "/spotify/me",
        response,
        sizeof(response),
        &statusCode
    );

    spotifyResponseCode = statusCode;

    if (result < 0 || statusCode != 200)
    {
        spotifyStatus = SPOTIFY_ERROR;
        spotifyThreadRunning = 0;
        return 0;
    }

    body = getHttpBody(response);

    strncpy(spotifyUser, body, sizeof(spotifyUser) - 1);
    spotifyUser[sizeof(spotifyUser) - 1] = '\0';

    while (
        strlen(spotifyUser) > 0 &&
        (spotifyUser[strlen(spotifyUser) - 1] == '\n' ||
         spotifyUser[strlen(spotifyUser) - 1] == '\r')
    )
    {
        spotifyUser[strlen(spotifyUser) - 1] = '\0';
    }

    spotifyStatus = SPOTIFY_CONNECTED;
    spotifyThreadRunning = 0;
    return 0;
}

void startSpotifyCheck(void)
{
    SceUID threadId;

    if (spotifyThreadRunning) return;

    if (wlanStatus != WLAN_ONLINE)
    {
        spotifyStatus = SPOTIFY_ERROR;
        return;
    }

    spotifyStatus = SPOTIFY_CHECKING;
    spotifyThreadRunning = 1;

    threadId = sceKernelCreateThread(
        "SpotatuiSpotifyThread",
        spotifyThread,
        0x11,
        24 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId < 0)
    {
        spotifyStatus = SPOTIFY_ERROR;
        spotifyThreadRunning = 0;
        return;
    }

    if (sceKernelStartThread(threadId, 0, NULL) < 0)
    {
        spotifyStatus = SPOTIFY_ERROR;
        spotifyThreadRunning = 0;
    }
}



void trimLine(char *text);
void startPlayerRefresh(void);

/* --------------------------------------------------
   SPOTIFY SEARCH - NATIVE PSP OSK
-------------------------------------------------- */

void asciiToUcs2(const char *source, unsigned short *dest, int maxChars)
{
    int i = 0;

    if (maxChars <= 0) return;

    while (source[i] != '\0' && i < maxChars - 1)
    {
        dest[i] = (unsigned short)(unsigned char)source[i];
        i++;
    }

    dest[i] = 0;
}

void ucs2ToAscii(const unsigned short *source, char *dest, int maxChars)
{
    int i = 0;

    if (maxChars <= 0) return;

    while (source[i] != 0 && i < maxChars - 1)
    {
        unsigned short c = source[i];

        if (c >= 'a' && c <= 'z')
            c = (unsigned short)(c - 'a' + 'A');

        if ((c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == ' ' || c == '.' || c == '-' || c == '/')
        {
            dest[i] = (char)c;
        }
        else
        {
            dest[i] = ' ';
        }

        i++;
    }

    dest[i] = '\0';
}

int urlEncode(const char *source, char *dest, int destSize)
{
    const char hex[] = "0123456789ABCDEF";
    int in = 0;
    int out = 0;

    while (source[in] != '\0' && out < destSize - 1)
    {
        unsigned char c = (unsigned char)source[in];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            dest[out++] = (char)c;
        }
        else
        {
            if (out + 3 >= destSize) break;
            dest[out++] = '%';
            dest[out++] = hex[(c >> 4) & 0x0F];
            dest[out++] = hex[c & 0x0F];
        }

        in++;
    }

    dest[out] = '\0';
    return out;
}

int openSearchOsk(void)
{
    SceUtilityOskData data;
    SceUtilityOskParams params;
    unsigned short description[64];
    unsigned short input[64];
    unsigned short output[64];
    int status;

    memset(&data, 0, sizeof(data));
    memset(&params, 0, sizeof(params));
    memset(description, 0, sizeof(description));
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));

    asciiToUcs2("SEARCH SPOTIFY", description, 64);
    asciiToUcs2(searchQuery, input, 64);

    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = description;
    data.intext = input;
    data.outtextlength = 64;
    data.outtextlimit = 42;
    data.outtext = output;

    params.base.size = sizeof(params);
    params.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    params.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;

    if (sceUtilityOskInitStart(&params) < 0)
        return 0;

    while (1)
    {
        status = sceUtilityOskGetStatus();

        if (status == PSP_UTILITY_DIALOG_VISIBLE)
        {
            sceUtilityOskUpdate(1);
        }
        else if (status == PSP_UTILITY_DIALOG_QUIT)
        {
            sceUtilityOskShutdownStart();
        }
        else if (status == PSP_UTILITY_DIALOG_NONE)
        {
            break;
        }

        sceDisplayWaitVblankStart();
    }

    if (data.result == PSP_UTILITY_OSK_RESULT_CHANGED)
    {
        ucs2ToAscii(output, searchQuery, sizeof(searchQuery));

        if (searchQuery[0] != '\0')
            return 1;
    }

    return 0;
}

int searchThread(SceSize args, void *argp)
{
    char encoded[192];
    char path[256];
    char response[4096];
    char *body;
    char *line;
    int statusCode = 0;
    int result;

    searchThreadRunning = 1;
    searchResultCount = 0;
    searchSelected = 0;
    searchStatusCode = 0;
    strcpy(searchMessage, "SEARCHING...");

    urlEncode(searchQuery, encoded, sizeof(encoded));
    snprintf(path, sizeof(path), "/spotify/search?q=%s", encoded);

    result = backendGet(path, response, sizeof(response), &statusCode);
    searchStatusCode = statusCode;

    if (result < 0 || statusCode != 200)
    {
        strcpy(searchMessage, "SEARCH ERROR");
        searchThreadRunning = 0;
        return 0;
    }

    body = (char *)getHttpBody(response);
    trimLine(body);

    if (strncmp(body, "NONE", 4) == 0 || body[0] == '\0')
    {
        strcpy(searchMessage, "NO RESULTS");
        searchThreadRunning = 0;
        return 0;
    }

    line = strtok(body, "\n");

    while (line != NULL && searchResultCount < SEARCH_MAX_RESULTS)
    {
        char *title;
        char *artist;
        char *id;

        trimLine(line);

        title = strtok(line, "|");
        artist = strtok(NULL, "|");
        id = strtok(NULL, "|");

        if (title != NULL && artist != NULL && id != NULL)
        {
            strncpy(searchResults[searchResultCount].title, title,
                    sizeof(searchResults[searchResultCount].title) - 1);
            searchResults[searchResultCount].title[
                sizeof(searchResults[searchResultCount].title) - 1] = '\0';

            strncpy(searchResults[searchResultCount].artist, artist,
                    sizeof(searchResults[searchResultCount].artist) - 1);
            searchResults[searchResultCount].artist[
                sizeof(searchResults[searchResultCount].artist) - 1] = '\0';

            strncpy(searchResults[searchResultCount].id, id,
                    sizeof(searchResults[searchResultCount].id) - 1);
            searchResults[searchResultCount].id[
                sizeof(searchResults[searchResultCount].id) - 1] = '\0';

            searchResultCount++;
        }

        line = strtok(NULL, "\n");
    }

    if (searchResultCount > 0)
        snprintf(searchMessage, sizeof(searchMessage), "%d RESULTS", searchResultCount);
    else
        strcpy(searchMessage, "NO RESULTS");

    searchThreadRunning = 0;
    return 0;
}

void startSearch(void)
{
    SceUID threadId;

    if (searchThreadRunning || wlanStatus != WLAN_ONLINE || searchQuery[0] == '\0')
        return;

    searchThreadRunning = 1;

    threadId = sceKernelCreateThread(
        "SpotatuiSearchThread",
        searchThread,
        0x11,
        24 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId < 0)
    {
        searchThreadRunning = 0;
        strcpy(searchMessage, "THREAD ERROR");
        return;
    }

    if (sceKernelStartThread(threadId, 0, NULL) < 0)
    {
        searchThreadRunning = 0;
        strcpy(searchMessage, "THREAD ERROR");
    }
}

int searchPlayThread(SceSize args, void *argp)
{
    char path[128];
    char response[512];
    int statusCode = 0;
    int result;

    searchPlayRunning = 1;

    snprintf(path, sizeof(path), "/spotify/play-track?id=%s", pendingTrackId);
    result = backendGet(path, response, sizeof(response), &statusCode);

    if (result == 0 && statusCode >= 200 && statusCode < 300)
    {
        strcpy(searchMessage, "PLAYING");
        sceKernelDelayThread(300000);
        startPlayerRefresh();
    }
    else
    {
        strcpy(searchMessage, "PLAY ERROR");
    }

    searchPlayRunning = 0;
    return 0;
}

void playSelectedSearchResult(void)
{
    SceUID threadId;

    if (searchPlayRunning ||
        searchResultCount <= 0 ||
        searchSelected < 0 ||
        searchSelected >= searchResultCount ||
        wlanStatus != WLAN_ONLINE)
    {
        return;
    }

    strncpy(pendingTrackId, searchResults[searchSelected].id,
            sizeof(pendingTrackId) - 1);
    pendingTrackId[sizeof(pendingTrackId) - 1] = '\0';

    searchPlayRunning = 1;
    strcpy(searchMessage, "SENDING...");

    threadId = sceKernelCreateThread(
        "SpotatuiSearchPlay",
        searchPlayThread,
        0x12,
        16 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId < 0)
    {
        searchPlayRunning = 0;
        strcpy(searchMessage, "THREAD ERROR");
        return;
    }

    if (sceKernelStartThread(threadId, 0, NULL) < 0)
    {
        searchPlayRunning = 0;
        strcpy(searchMessage, "THREAD ERROR");
    }
}


/* --------------------------------------------------
   SPOTIFY PLAYER
-------------------------------------------------- */

void trimLine(char *text)
{
    int length = strlen(text);

    while (length > 0 &&
           (text[length - 1] == '\r' || text[length - 1] == '\n' || text[length - 1] == ' '))
    {
        text[length - 1] = '\0';
        length--;
    }
}

int playerThread(SceSize args, void *argp)
{
    char response[2048];
    char *body;
    char *line;
    int statusCode = 0;
    int result;

    playerThreadRunning = 1;

    result = backendGet("/spotify/player", response, sizeof(response), &statusCode);

    if (result < 0 || statusCode != 200)
    {
        /* Keep the last valid snapshot on transient network/API errors. */
        playerThreadRunning = 0;
        return 0;
    }

    body = (char *)getHttpBody(response);
    trimLine(body);

    if (strncmp(body, "NONE", 4) == 0)
    {
        strcpy(playerTrack, "NO TRACK");
        strcpy(playerArtist, "PLAYER STANDBY");
        playerProgressMs = 0;
        playerDurationMs = 0;
        playerIsPlaying = 0;
        playerLoaded = 0;
        playerLastSyncUs = 0;
        playerThreadRunning = 0;
        return 0;
    }

    line = strtok(body, "\n");
    if (line != NULL)
    {
        trimLine(line);
        strncpy(playerTrack, line, sizeof(playerTrack) - 1);
        playerTrack[sizeof(playerTrack) - 1] = '\0';
    }

    line = strtok(NULL, "\n");
    if (line != NULL)
    {
        trimLine(line);
        strncpy(playerArtist, line, sizeof(playerArtist) - 1);
        playerArtist[sizeof(playerArtist) - 1] = '\0';
    }

    line = strtok(NULL, "\n");
    if (line != NULL) playerProgressMs = atoi(line);

    line = strtok(NULL, "\n");
    if (line != NULL) playerDurationMs = atoi(line);

    line = strtok(NULL, "\n");
    if (line != NULL) playerIsPlaying = atoi(line) ? 1 : 0;

    playerLastSyncUs = sceKernelGetSystemTimeLow();
    playerLoaded = 1;
    playerThreadRunning = 0;
    return 0;
}

void startPlayerRefresh(void)
{
    SceUID threadId;

    if (playerThreadRunning || wlanStatus != WLAN_ONLINE) return;

    playerThreadRunning = 1;

    threadId = sceKernelCreateThread(
        "SpotatuiPlayerThread",
        playerThread,
        0x11,
        24 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId < 0)
    {
        playerThreadRunning = 0;
        return;
    }

    if (sceKernelStartThread(threadId, 0, NULL) < 0)
    {
        playerThreadRunning = 0;
    }
}

int playerCommandThread(SceSize args, void *argp)
{
    char response[512];
    int statusCode = 0;
    int result = -1;
    PlayerCommand command = pendingPlayerCommand;

    playerCommandRunning = 1;

    if (command == PLAYER_CMD_PLAY_PAUSE)
    {
        if (playerIsPlaying)
            result = backendCommand("PUT", "/spotify/pause", response, sizeof(response), &statusCode);
        else
            result = backendCommand("PUT", "/spotify/play", response, sizeof(response), &statusCode);
    }
    else if (command == PLAYER_CMD_NEXT)
    {
        result = backendCommand("POST", "/spotify/next", response, sizeof(response), &statusCode);
    }
    else if (command == PLAYER_CMD_PREVIOUS)
    {
        result = backendCommand("POST", "/spotify/previous", response, sizeof(response), &statusCode);
    }

    pendingPlayerCommand = PLAYER_CMD_NONE;
    playerCommandRunning = 0;

    if (result == 0 && statusCode >= 200 && statusCode < 300)
    {
        sceKernelDelayThread(250000);
        startPlayerRefresh();
    }

    return 0;
}

void sendPlayerCommand(PlayerCommand command)
{
    SceUID threadId;

    if (playerCommandRunning || wlanStatus != WLAN_ONLINE)
        return;

    pendingPlayerCommand = command;
    playerCommandRunning = 1;

    threadId = sceKernelCreateThread(
        "SpotatuiPlayerCommand",
        playerCommandThread,
        0x12,
        16 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId < 0)
    {
        playerCommandRunning = 0;
        pendingPlayerCommand = PLAYER_CMD_NONE;
        return;
    }

    if (sceKernelStartThread(threadId, 0, NULL) < 0)
    {
        playerCommandRunning = 0;
        pendingPlayerCommand = PLAYER_CMD_NONE;
    }
}

int playerMonitorThread(SceSize args, void *argp)
{
    playerMonitorRunning = 1;

    while (1)
    {
        if (wlanStatus == WLAN_ONLINE)
        {
            startPlayerRefresh();
        }

        /*
         * NOW PLAYING stays tightly synchronized with changes made from
         * another Spotify device. Other screens use a slower background
         * refresh so the next visit already has a fresh snapshot.
         */
        if (currentScreen == SCREEN_NOW_PLAYING)
            sceKernelDelayThread(2000000);
        else
            sceKernelDelayThread(5000000);
    }

    return 0;
}

void startPlayerMonitor(void)
{
    SceUID threadId;

    if (playerMonitorRunning)
        return;

    threadId = sceKernelCreateThread(
        "SpotatuiPlayerMonitor",
        playerMonitorThread,
        0x13,
        16 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );

    if (threadId >= 0)
        sceKernelStartThread(threadId, 0, NULL);
}


/* --------------------------------------------------
   CRT
-------------------------------------------------- */

void drawScanlines(void)
{
    int y;

    for (y = 6; y < 268; y += 6)
    {
        drawLine(
            4.0f,
            (float)y,
            476.0f,
            (float)y,
            COLOR_SCAN
        );
    }
}


/* --------------------------------------------------
   STRUTTURA COMUNE
-------------------------------------------------- */

void drawFrame(void)
{
    /* v0.11.4: nessuna barra o cornice globale. */
}


/* --------------------------------------------------
   VR BOOT / INTRO
-------------------------------------------------- */

void drawBootScreen(void)
{
    float progress;
    float barWidth;
    const char *stage;

    progress = (float)bootFrame / 150.0f;
    if (progress > 1.0f) progress = 1.0f;
    barWidth = 260.0f * progress;

    drawRectangle(15.0f, 12.0f, 450.0f, 248.0f, COLOR_DIM);

    drawText(28.0f, 24.0f, "VR SYSTEM", 1.0f, COLOR_GREEN);
    drawText(393.0f, 24.0f, "VER 0.12", 0.8f, COLOR_DIM);
    drawLine(15.0f, 42.0f, 465.0f, 42.0f, COLOR_GREEN);

    drawRectangle(124.0f, 72.0f, 232.0f, 54.0f, COLOR_GREEN);
    drawText(158.0f, 86.0f, "SPOTATUI PSP", 1.8f, COLOR_TEXT);
    drawText(171.0f, 109.0f, "VR AUDIO TERMINAL", 0.8f, COLOR_DIM);

    drawText(110.0f, 151.0f, "SYSTEM INITIALIZING", 1.0f, COLOR_GREEN);
    drawRectangle(110.0f, 170.0f, 260.0f, 10.0f, COLOR_DIM);

    if (barWidth > 0.0f)
        drawFilledRectangle(110.0f, 170.0f, barWidth, 10.0f, COLOR_GREEN);

    if (bootFrame < 45)
        stage = "> MEMORY CHECK";
    else if (bootFrame < 90)
        stage = "> VR INTERFACE";
    else if (bootFrame < 130)
        stage = "> AUDIO SYSTEM";
    else
        stage = "> READY";

    drawText(110.0f, 194.0f, stage, 0.9f, COLOR_TEXT);
    drawText(359.0f, 239.0f, "VR MISSION", 0.7f, COLOR_DIM);
}


/* --------------------------------------------------
   UI v0.15.1 - REAL PSP READABILITY PASS
-------------------------------------------------- */

void drawTextLimited(float x, float y, const char *text, int maxChars, float scale, unsigned int color)
{
    char buffer[64];
    int i = 0;
    if (maxChars > 63) maxChars = 63;
    while (text[i] != '\0' && i < maxChars)
    {
        buffer[i] = text[i];
        i++;
    }
    buffer[i] = '\0';
    drawText(x, y, buffer, scale, color);
}

void drawPageHeader(const char *title)
{
    drawText(22.0f, 12.0f, title, 1.55f, COLOR_GREEN);
    drawText(410.0f, 17.0f, "v0.16.5", 1.00f, COLOR_DIM);
    drawLine(22.0f, 40.0f, 458.0f, 40.0f, COLOR_GREEN);
}

void drawPageFooter(const char *left, const char *right)
{
    drawFilledRectangle(0.0f, 247.0f, 480.0f, 25.0f, COLOR_BG);
    drawLine(22.0f, 246.0f, 458.0f, 246.0f, COLOR_DIM);
    drawText(24.0f, 253.0f, left, 1.00f, COLOR_TEXT);
    drawText(350.0f, 253.0f, right, 1.00f, COLOR_TEXT);
}

void drawLargeInfoRow(float y, const char *label, const char *value, int selected)
{
    unsigned int fg = selected ? COLOR_BG : COLOR_TEXT;
    unsigned int sub = selected ? COLOR_BG : COLOR_GREEN;
    if (selected)
        drawFilledRectangle(22.0f, y, 436.0f, 42.0f, COLOR_GREEN);
    else
        drawRectangle(22.0f, y, 436.0f, 42.0f, COLOR_DIM);

    if (selected) drawText(33.0f, y + 12.0f, ">", 1.05f, fg);
    drawText(57.0f, y + 10.0f, label, 1.05f, fg);
    drawTextLimited(276.0f, y + 10.0f, value, 16, 1.00f, sub);
}

void drawHome(void)
{
    const char *items[6] = {
        "NOW PLAYING", "SEARCH", "PLAYLISTS",
        "YOUR LIBRARY", "SETTINGS", "EXIT"
    };
    int firstVisible;
    int i;
    int row;
    float y;

    drawText(24.0f, 14.0f, "SPOTATUI", 1.85f, COLOR_GREEN);
    drawText(26.0f, 42.0f, "PSP // AUDIO TERMINAL", 1.00f, COLOR_TEXT);
    drawText(410.0f, 19.0f, "v0.16.5", 1.00f, COLOR_DIM);
    drawLine(24.0f, 65.0f, 456.0f, 65.0f, COLOR_GREEN);

    if (selectedMenu <= 1) firstVisible = 0;
    else if (selectedMenu >= 4) firstVisible = 2;
    else firstVisible = selectedMenu - 1;

    for (row = 0; row < 4; row++)
    {
        i = firstVisible + row;
        y = 76.0f + (row * 43.0f);
        if (i == selectedMenu)
        {
            drawFilledRectangle(24.0f, y, 432.0f, 36.0f, COLOR_GREEN);
            drawText(36.0f, y + 8.0f, ">", 1.20f, COLOR_BG);
            drawText(67.0f, y + 7.0f, items[i], 1.25f, COLOR_BG);
        }
        else
        {
            drawRectangle(24.0f, y, 432.0f, 36.0f, COLOR_DIM);
            drawText(67.0f, y + 8.0f, items[i], 1.10f, COLOR_TEXT);
        }
    }

    drawFilledRectangle(0.0f, 254.0f, 480.0f, 18.0f, COLOR_BG);
    drawLine(24.0f, 253.0f, 456.0f, 253.0f, COLOR_DIM);
    drawText(27.0f, 258.0f, "UP/DOWN NAV", 1.00f, COLOR_TEXT);
    drawText(399.0f, 258.0f, "X OPEN", 1.00f, COLOR_TEXT);
}

void drawSearchScreen(void)
{
    int firstVisible;
    int row;
    int index;
    float y;

    drawPageHeader("SEARCH");

    if (searchQuery[0] == '\0')
        drawText(24.0f, 54.0f, "X NEW SEARCH", 1.15f, COLOR_GREEN);
    else
    {
        drawTextLimited(24.0f, 52.0f, searchQuery, 34, 1.15f, COLOR_GREEN);
        drawText(24.0f, 72.0f, searchMessage, 1.00f,
                 searchThreadRunning || searchPlayRunning ? COLOR_TEXT : COLOR_DIM);
    }

    if (searchResultCount <= 0)
    {
        drawRectangle(24.0f, 101.0f, 432.0f, 66.0f, COLOR_DIM);
        drawText(39.0f, 119.0f,
                 searchThreadRunning ? "SEARCHING SPOTIFY" :
                 (searchQuery[0] == '\0' ? "PRESS X TO TYPE" : searchMessage),
                 1.20f,
                 searchThreadRunning ? COLOR_GREEN : COLOR_TEXT);

        drawPageFooter("X SEARCH", "O BACK");
        return;
    }

    if (searchSelected <= 0) firstVisible = 0;
    else if (searchSelected >= searchResultCount - 1) firstVisible = searchResultCount - 2;
    else firstVisible = searchSelected - 1;

    if (firstVisible < 0) firstVisible = 0;
    if (firstVisible > searchResultCount - 2) firstVisible = searchResultCount - 2;
    if (firstVisible < 0) firstVisible = 0;

    for (row = 0; row < 2; row++)
    {
        index = firstVisible + row;
        if (index >= searchResultCount) break;

        y = 95.0f + row * 66.0f;

        if (index == searchSelected)
        {
            drawFilledRectangle(24.0f, y, 432.0f, 58.0f, COLOR_GREEN);
            drawTextLimited(38.0f, y + 10.0f,
                            searchResults[index].title, 31, 1.12f, COLOR_BG);
            drawTextLimited(38.0f, y + 33.0f,
                            searchResults[index].artist, 36, 1.00f, COLOR_BG);
        }
        else
        {
            drawRectangle(24.0f, y, 432.0f, 58.0f, COLOR_DIM);
            drawTextLimited(38.0f, y + 10.0f,
                            searchResults[index].title, 31, 1.12f, COLOR_TEXT);
            drawTextLimited(38.0f, y + 33.0f,
                            searchResults[index].artist, 36, 1.00f, COLOR_DIM);
        }
    }

    drawPageFooter("UP/DOWN NAV", "X PLAY  TRI SEARCH");
}

void drawPlaylistsScreen(void)
{
    drawPageHeader("PLAYLISTS");
    drawText(24.0f, 69.0f, "YOUR PLAYLISTS", 1.05f, COLOR_DIM);
    drawLargeInfoRow(101.0f, "SPOTIFY", "SYNC PENDING", 0);
    drawLargeInfoRow(151.0f, "PLAYLISTS", "NO DATA", 0);
    drawPageFooter("O BACK", "START HOME");
}

void drawLibraryScreen(void)
{
    drawPageHeader("YOUR LIBRARY");
    drawText(24.0f, 69.0f, "SAVED MUSIC", 1.05f, COLOR_DIM);
    drawLargeInfoRow(101.0f, "LIBRARY", "SYNC PENDING", 0);
    drawLargeInfoRow(151.0f, "TRACKS", "NO DATA", 0);
    drawPageFooter("O BACK", "START HOME");
}

void drawNowPlayingScreen(void)
{
    float progressWidth = 0.0f;
    int shownProgressMs = playerProgressMs;
    int progressSeconds;
    int durationSeconds;
    char elapsedText[16];
    char durationText[16];
    const char *linkText;
    unsigned int linkColor;

    if (playerLoaded && playerIsPlaying && playerLastSyncUs != 0)
    {
        unsigned int nowUs = sceKernelGetSystemTimeLow();
        unsigned int elapsedUs = nowUs - playerLastSyncUs;
        shownProgressMs += (int)(elapsedUs / 1000);
        if (playerDurationMs > 0 && shownProgressMs > playerDurationMs)
            shownProgressMs = playerDurationMs;
    }

    progressSeconds = shownProgressMs / 1000;
    durationSeconds = playerDurationMs / 1000;

    if (playerThreadRunning) { linkText = "SYNCING"; linkColor = COLOR_TEXT; }
    else if (playerLoaded) { linkText = "CONNECTED"; linkColor = COLOR_GREEN; }
    else { linkText = "STANDBY"; linkColor = COLOR_DIM; }

    drawPageHeader("NOW PLAYING");
    drawText(350.0f, 50.0f, linkText, 1.00f, linkColor);

    drawTextLimited(24.0f, 69.0f,
        playerLoaded ? playerTrack : "NO ACTIVE TRACK", 24, 1.45f,
        playerLoaded ? COLOR_GREEN : COLOR_TEXT);
    drawTextLimited(24.0f, 101.0f,
        playerLoaded ? playerArtist : "WAITING FOR SPOTIFY", 30, 1.10f, COLOR_TEXT);

    drawText(24.0f, 132.0f,
        playerLoaded ? (playerIsPlaying ? "> PLAYING" : "PAUSED") : "STANDBY",
        1.05f, playerLoaded ? COLOR_GREEN : COLOR_DIM);

    if (playerDurationMs > 0)
    {
        progressWidth = 326.0f * ((float)shownProgressMs / (float)playerDurationMs);
        if (progressWidth < 0.0f) progressWidth = 0.0f;
        if (progressWidth > 326.0f) progressWidth = 326.0f;
    }

    snprintf(elapsedText, sizeof(elapsedText), "%d:%02d", progressSeconds / 60, progressSeconds % 60);
    snprintf(durationText, sizeof(durationText), "%d:%02d", durationSeconds / 60, durationSeconds % 60);
    drawText(24.0f, 161.0f, elapsedText, 1.00f, COLOR_TEXT);
    drawRectangle(91.0f, 166.0f, 326.0f, 9.0f, COLOR_DIM);
    if (progressWidth > 0.0f) drawFilledRectangle(92.0f, 167.0f, progressWidth, 7.0f, COLOR_GREEN);
    drawText(425.0f, 161.0f, durationText, 1.00f, COLOR_TEXT);

    drawRectangle(24.0f, 194.0f, 132.0f, 38.0f, COLOR_DIM);
    drawRectangle(174.0f, 194.0f, 132.0f, 38.0f, COLOR_GREEN);
    drawRectangle(324.0f, 194.0f, 132.0f, 38.0f, COLOR_DIM);
    drawText(45.0f, 206.0f, "L PREV", 1.00f, COLOR_TEXT);
    drawText(196.0f, 206.0f, "X PLAY", 1.00f, COLOR_GREEN);
    drawText(365.0f, 206.0f, "R NEXT", 1.00f, COLOR_TEXT);

    if (playerCommandRunning) drawText(348.0f, 181.0f, "COMMAND", 1.00f, COLOR_TEXT);
    drawPageFooter("O BACK", "TRI SYNC");
}

void getSettingValue(int index, char *buffer, int size)
{
    int batteryPercent;
    int batteryMinutes;
    int batteryVoltage;
    int batteryTemp;
    int powerOnline;
    int charging;

    buffer[0] = '\0';
    switch (index)
    {
        case 0:
            snprintf(buffer, size, "%s", getWlanStatusText());
            break;
        case 1:
            snprintf(buffer, size, "%s", getHttpStatusText());
            break;
        case 2:
            snprintf(buffer, size, "%s", getSpotifyStatusText());
            break;
        case 3:
            batteryPercent = scePowerGetBatteryLifePercent();
            if (batteryPercent < 0) snprintf(buffer, size, "--");
            else snprintf(buffer, size, "%d PERCENT", batteryPercent);
            break;
        case 4:
            powerOnline = scePowerIsPowerOnline();
            snprintf(buffer, size, "%s", powerOnline ? "YES" : "NO");
            break;
        case 5:
            charging = scePowerIsBatteryCharging();
            snprintf(buffer, size, "%s", charging ? "YES" : "NO");
            break;
        case 6:
            batteryVoltage = scePowerGetBatteryVolt();
            snprintf(buffer, size, "%d MV", batteryVoltage);
            break;
        case 7:
            batteryTemp = scePowerGetBatteryTemp();
            snprintf(buffer, size, "%d C", batteryTemp);
            break;
        case 8:
            batteryMinutes = scePowerGetBatteryLifeTime();
            if (batteryMinutes < 0) snprintf(buffer, size, "--");
            else snprintf(buffer, size, "%dH %02dM", batteryMinutes / 60, batteryMinutes % 60);
            break;
    }
}

void drawSettingsScreen(void)
{
    const char *labels[9] = {
        "WLAN", "SERVER", "SPOTIFY", "BATTERY",
        "EXT POWER", "CHARGING", "VOLTAGE", "TEMP", "TIME LEFT"
    };

    int firstVisible;
    int row;
    int index;
    char value[48];
    char errorLine[64];

    drawPageHeader("SETTINGS");

    if (selectedSetting <= 1)
        firstVisible = 0;
    else if (selectedSetting >= 7)
        firstVisible = 5;
    else
        firstVisible = selectedSetting - 1;

    settingsScroll = firstVisible;

    for (row = 0; row < 4; row++)
    {
        index = firstVisible + row;

        if (index >= 9)
            break;

        getSettingValue(
            index,
            value,
            sizeof(value)
        );

        drawLargeInfoRow(
            54.0f + row * 47.0f,
            labels[index],
            value,
            index == selectedSetting
        );
    }

    if (firstVisible > 0)
        drawText(
            430.0f,
            45.0f,
            "UP",
            1.00f,
            COLOR_GREEN
        );

    if (firstVisible + 4 < 9)
        drawText(
            446.0f,
            236.0f,
            "V",
            1.00f,
            COLOR_GREEN
        );

    /*
       WLAN DIAGNOSTICS
    */
    if (
        selectedSetting == 0 &&
        wlanStatus == WLAN_ERROR
    )
    {
        drawFilledRectangle(
            0.0f,
            223.0f,
            480.0f,
            49.0f,
            COLOR_BG
        );

        drawLine(
            22.0f,
            222.0f,
            458.0f,
            222.0f,
            COLOR_DIM
        );

        snprintf(
            errorLine,
            sizeof(errorLine),
            "STAGE %s",
            getWlanErrorStageText()
        );

        drawTextLimited(
            24.0f,
            230.0f,
            errorLine,
            40,
            1.00f,
            COLOR_TEXT
        );

        snprintf(
            errorLine,
            sizeof(errorLine),
            "CODE 0X%08X",
            (unsigned int)wlanErrorCode
        );

        drawTextLimited(
            24.0f,
            250.0f,
            errorLine,
            40,
            1.00f,
            COLOR_GREEN
        );
    }

    /*
       SERVER / TCP DIAGNOSTICS - v0.16.4 NETWORK TRACE
    */
    else if (
        selectedSetting == 1 &&
        httpStatus == HTTP_ERROR
    )
    {
        char traceLine[96];

        drawFilledRectangle(
            0.0f,
            170.0f,
            480.0f,
            102.0f,
            COLOR_BG
        );

        drawLine(
            22.0f,
            169.0f,
            458.0f,
            169.0f,
            COLOR_DIM
        );

        snprintf(
            traceLine,
            sizeof(traceLine),
            "BACKEND %s:%d",
            traceBackendIp[0] ? traceBackendIp : "?",
            BACKEND_HTTP_PORT
        );
        drawTextLimited(24.0f, 178.0f, traceLine, 48, 0.72f, COLOR_TEXT);

        snprintf(
            traceLine,
            sizeof(traceLine),
            "SOCKET %d   CONNECT %d",
            traceSocketResult,
            traceConnectResult
        );
        drawTextLimited(24.0f, 194.0f, traceLine, 48, 0.72f, COLOR_TEXT);

        snprintf(
            traceLine,
            sizeof(traceLine),
            "SEND %d / %d",
            traceSendResult,
            traceSendExpected
        );
        drawTextLimited(24.0f, 210.0f, traceLine, 48, 0.72f, COLOR_TEXT);

        snprintf(
            traceLine,
            sizeof(traceLine),
            "RECV %d   INET ERRNO %d",
            traceRecvResult,
            traceInetErrno
        );
        drawTextLimited(24.0f, 226.0f, traceLine, 48, 0.72f, COLOR_TEXT);

        snprintf(
            traceLine,
            sizeof(traceLine),
            "STAGE %s   CODE 0X%08X",
            getHttpErrorStageText(),
            (unsigned int)httpErrorCode
        );
        drawTextLimited(24.0f, 242.0f, traceLine, 48, 0.72f, COLOR_GREEN);
    }

    /*
       NORMAL FOOTER
    */
    else
    {
        drawPageFooter(
            "UP/DOWN NAV",
            selectedSetting < 3
                ? "X ACTION"
                : "O BACK"
        );
    }
}

/* --------------------------------------------------
   RENDER
-------------------------------------------------- */

void drawInterface(void)
{
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(COLOR_BG);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    drawScanlines();

    if (bootActive)
    {
        drawBootScreen();
        bootFrame++;
        if (bootFrame >= 150) bootActive = 0;
    }
    else
    {
        switch (currentScreen)
        {
            case SCREEN_HOME:        drawHome(); break;
            case SCREEN_SEARCH:      drawSearchScreen(); break;
            case SCREEN_PLAYLISTS:   drawPlaylistsScreen(); break;
            case SCREEN_LIBRARY:     drawLibraryScreen(); break;
            case SCREEN_NOW_PLAYING: drawNowPlayingScreen(); break;
            case SCREEN_SETTINGS:    drawSettingsScreen(); break;
        }
    }

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* --------------------------------------------------
   NAVIGAZIONE v0.15.2
-------------------------------------------------- */

void setTopLevelScreenFromMenu(void)
{
    switch (selectedMenu)
    {
        case 0:
            currentScreen = SCREEN_NOW_PLAYING;
            startPlayerRefresh();
            break;
        case 1:
            currentScreen = SCREEN_SEARCH;
            break;
        case 2:
            currentScreen = SCREEN_PLAYLISTS;
            break;
        case 3:
            currentScreen = SCREEN_LIBRARY;
            break;
        case 4:
            currentScreen = SCREEN_SETTINGS;
            selectedSetting = 0;
            settingsScroll = 0;
            break;
        case 5:
            sceKernelExitGame();
            break;
    }
}

void updateController(void)
{
    SceCtrlData pad;
    static unsigned int oldButtons = 0;

    sceCtrlPeekBufferPositive(&pad, 1);

    if (bootActive)
    {
        oldButtons = pad.Buttons;
        return;
    }

    if ((pad.Buttons & PSP_CTRL_START) && !(oldButtons & PSP_CTRL_START))
    {
        currentScreen = SCREEN_HOME;
        oldButtons = pad.Buttons;
        return;
    }

    if (currentScreen == SCREEN_HOME)
    {
        if ((pad.Buttons & PSP_CTRL_DOWN) && !(oldButtons & PSP_CTRL_DOWN))
        {
            selectedMenu++;
            if (selectedMenu > 5) selectedMenu = 0;
        }

        if ((pad.Buttons & PSP_CTRL_UP) && !(oldButtons & PSP_CTRL_UP))
        {
            selectedMenu--;
            if (selectedMenu < 0) selectedMenu = 5;
        }

        if ((pad.Buttons & PSP_CTRL_CROSS) && !(oldButtons & PSP_CTRL_CROSS))
            setTopLevelScreenFromMenu();
    }
    else if (currentScreen == SCREEN_SEARCH)
    {
        if ((pad.Buttons & PSP_CTRL_DOWN) && !(oldButtons & PSP_CTRL_DOWN))
        {
            if (searchResultCount > 0)
            {
                searchSelected++;
                if (searchSelected >= searchResultCount) searchSelected = 0;
            }
        }

        if ((pad.Buttons & PSP_CTRL_UP) && !(oldButtons & PSP_CTRL_UP))
        {
            if (searchResultCount > 0)
            {
                searchSelected--;
                if (searchSelected < 0) searchSelected = searchResultCount - 1;
            }
        }

        if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(oldButtons & PSP_CTRL_TRIANGLE))
        {
            if (!searchThreadRunning && !searchPlayRunning && openSearchOsk())
                startSearch();
        }

        if ((pad.Buttons & PSP_CTRL_CROSS) && !(oldButtons & PSP_CTRL_CROSS))
        {
            if (searchResultCount > 0)
                playSelectedSearchResult();
            else if (!searchThreadRunning && !searchPlayRunning && openSearchOsk())
                startSearch();
        }

        if ((pad.Buttons & PSP_CTRL_CIRCLE) && !(oldButtons & PSP_CTRL_CIRCLE))
            currentScreen = SCREEN_HOME;
    }
    else if (currentScreen == SCREEN_SETTINGS)
    {
        if ((pad.Buttons & PSP_CTRL_DOWN) && !(oldButtons & PSP_CTRL_DOWN))
        {
            selectedSetting++;
            if (selectedSetting > 8) selectedSetting = 0;
        }

        if ((pad.Buttons & PSP_CTRL_UP) && !(oldButtons & PSP_CTRL_UP))
        {
            selectedSetting--;
            if (selectedSetting < 0) selectedSetting = 8;
        }

        if ((pad.Buttons & PSP_CTRL_CROSS) && !(oldButtons & PSP_CTRL_CROSS))
        {
            if (selectedSetting == 0) startWlanConnection();
            else if (selectedSetting == 1) startHttpTest();
            else if (selectedSetting == 2) startSpotifyCheck();
        }

        if ((pad.Buttons & PSP_CTRL_CIRCLE) && !(oldButtons & PSP_CTRL_CIRCLE))
            currentScreen = SCREEN_HOME;
    }
    else
    {
        if (currentScreen == SCREEN_NOW_PLAYING)
        {
            if ((pad.Buttons & PSP_CTRL_CROSS) && !(oldButtons & PSP_CTRL_CROSS))
                sendPlayerCommand(PLAYER_CMD_PLAY_PAUSE);
            if ((pad.Buttons & PSP_CTRL_LEFT) && !(oldButtons & PSP_CTRL_LEFT))
                sendPlayerCommand(PLAYER_CMD_PREVIOUS);
            if ((pad.Buttons & PSP_CTRL_RIGHT) && !(oldButtons & PSP_CTRL_RIGHT))
                sendPlayerCommand(PLAYER_CMD_NEXT);
            if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(oldButtons & PSP_CTRL_TRIANGLE))
                startPlayerRefresh();
        }

        if ((pad.Buttons & PSP_CTRL_CIRCLE) && !(oldButtons & PSP_CTRL_CIRCLE))
            currentScreen = SCREEN_HOME;
    }

    oldButtons = pad.Buttons;
}


/* --------------------------------------------------
   CALLBACK DI USCITA
-------------------------------------------------- */

int exit_callback(
    int arg1,
    int arg2,
    void *common
)
{
    sceKernelExitGame();

    return 0;
}


int CallbackThread(
    SceSize args,
    void *argp
)
{
    int cbid;


    cbid = sceKernelCreateCallback(
        "Exit Callback",
        exit_callback,
        NULL
    );


    sceKernelRegisterExitCallback(
        cbid
    );


    sceKernelSleepThreadCB();

    return 0;
}


/* --------------------------------------------------
   MAIN
-------------------------------------------------- */

int main(void)
{
    int thid;


    thid = sceKernelCreateThread(
        "update_thread",
        CallbackThread,
        0x11,
        0xFA0,
        0,
        NULL
    );


    if (thid >= 0)
    {
        sceKernelStartThread(
            thid,
            0,
            0
        );
    }


    sceCtrlSetSamplingCycle(
        0
    );


    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_ANALOG
    );


    initGraphics();

    startPlayerMonitor();


    while (1)
    {
        updateController();

        drawInterface();
    }


    return 0;
}