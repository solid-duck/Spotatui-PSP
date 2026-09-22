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
#include <pspsdk.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
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

volatile WlanStatus wlanStatus = WLAN_OFFLINE;
volatile int wlanThreadRunning = 0;
volatile WlanErrorStage wlanErrorStage = WLAN_ERROR_NONE;
volatile int wlanErrorCode = 0;

char wlanIp[32] = "";


volatile HttpStatus httpStatus = HTTP_IDLE;
volatile HttpErrorStage httpErrorStage = HTTP_ERROR_NONE;
volatile int httpErrorCode = 0;
volatile int httpThreadRunning = 0;
volatile int httpResponseCode = 0;

volatile SpotifyStatus spotifyStatus = SPOTIFY_UNKNOWN;
volatile int spotifyThreadRunning = 0;
volatile int spotifyResponseCode = 0;
char spotifyUser[64] = "";

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

    fbp0 = guGetStaticVramBuffer(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        GU_PSM_8888
    );

    fbp1 = guGetStaticVramBuffer(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        GU_PSM_8888
    );

    zbp = guGetStaticVramBuffer(
        SCREEN_WIDTH,
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
int wlanThread(
    SceSize args,
    void *argp
)
{
    int result;
    int state;
    int attempts = 0;

    wlanThreadRunning = 1;
    wlanStatus = WLAN_CONNECTING;
    wlanErrorStage = WLAN_ERROR_NONE;
    wlanErrorCode = 0;

    wlanIp[0] = '\0';


    /*
       Carichiamo i moduli di rete della PSP.
    */

    result = sceUtilityLoadNetModule(
        PSP_NET_MODULE_COMMON
    );

    if (result < 0)
    {
        setWlanError(
            WLAN_ERROR_COMMON_MODULE,
            result
        );

        return 0;
    }


    result = sceUtilityLoadNetModule(
        PSP_NET_MODULE_INET
    );

    if (result < 0)
    {
        setWlanError(
            WLAN_ERROR_INET_MODULE,
            result
        );

        return 0;
    }


    /*
       v0.5.4:
       inizializzazione esplicita dello stack di rete, seguendo
       la sequenza dell'esempio htmlviewer del PSPSDK.
    */

    result = sceNetInit(
        0x20000,
        0x2A,
        0,
        0x2A,
        0
    );

    if (result < 0)
    {
        setWlanError(
            WLAN_ERROR_NET_INIT,
            result
        );

        return 0;
    }


    result = sceNetInetInit();

    if (result < 0)
    {
        setWlanError(
            WLAN_ERROR_INET_INIT,
            result
        );

        return 0;
    }


    result = sceNetResolverInit();

    if (result < 0)
    {
        setWlanError(
            WLAN_ERROR_RESOLVER_INIT,
            result
        );

        return 0;
    }


    result = sceNetApctlInit(
        0x1800,
        0x30
    );

    if (result < 0)
    {
        setWlanError(
            WLAN_ERROR_APCTL_INIT,
            result
        );

        return 0;
    }


    /*
       Utilizziamo il profilo di rete numero 1
       salvato nelle impostazioni della PSP.
    */

    result = sceNetApctlConnect(1);

    if (result != 0)
    {
        setWlanError(
            WLAN_ERROR_AP_CONNECT,
            result
        );

        return 0;
    }


    /*
       Aspettiamo che APCTL raggiunga lo stato 4.

       Stato 4 = connessione completata
       e indirizzo IP disponibile.

       Mettiamo anche un limite ai tentativi
       per evitare un'attesa infinita.
    */

    while (attempts < 400)
    {
        state = 0;

        result = sceNetApctlGetState(
            &state
        );

        if (result != 0)
        {
            wlanStatus = WLAN_ERROR;
            wlanThreadRunning = 0;

            return 0;
        }


        if (state == 4)
        {
            union SceNetApctlInfo info;

            wlanStatus = WLAN_ONLINE;

            /*
               Il codice 8 corrisponde
               all'indirizzo IP.
            */

            if (
                sceNetApctlGetInfo(
                    8,
                    &info
                ) == 0
            )
            {
                strncpy(
                    wlanIp,
                    info.ip,
                    sizeof(wlanIp) - 1
                );

                wlanIp[
                    sizeof(wlanIp) - 1
                ] = '\0';
            }

            wlanThreadRunning = 0;

            return 0;
        }


        sceKernelDelayThread(
            50 * 1000
        );

        attempts++;
    }


    /*
       Se arriviamo qui abbiamo superato
       il tempo massimo di connessione.
    */

    setWlanError(
        WLAN_ERROR_TIMEOUT,
        0
    );

    return 0;
}


void startWlanConnection(void)
{
    SceUID threadId;

    /*
       Evitiamo di creare più thread
       contemporaneamente.
    */

    if (wlanThreadRunning)
    {
        return;
    }


    /*
       Se siamo già online non dobbiamo
       riconnetterci.
    */

    if (wlanStatus == WLAN_ONLINE)
    {
        return;
    }


    wlanStatus = WLAN_CONNECTING;
    wlanErrorStage = WLAN_ERROR_NONE;
    wlanErrorCode = 0;
    wlanIp[0] = '\0';
    wlanThreadRunning = 1;


    threadId = sceKernelCreateThread(
        "SpotatuiWlanThread",
        wlanThread,
        0x11,
        32 * 1024,
        PSP_THREAD_ATTR_USER,
        NULL
    );


    if (threadId < 0)
    {
        setWlanError(
            WLAN_ERROR_THREAD_CREATE,
            threadId
        );

        return;
    }


    {
        int startResult;

        startResult = sceKernelStartThread(
            threadId,
            0,
            NULL
        );

        if (startResult < 0)
        {
            setWlanError(
                WLAN_ERROR_THREAD_START,
                startResult
            );
        }
    }
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
    int result;
    int received;
    int totalReceived = 0;
    int statusCode = 0;
    struct sockaddr_in serverAddress;
    char response[1024];
    const char request[] =
        "GET /ping HTTP/1.0\r\n"
        "Host: 192.168.1.20:8080\r\n"
        "User-Agent: SpotatuiPSP/0.10\r\n"
        "Connection: close\r\n"
        "\r\n";

    httpThreadRunning = 1;
    httpStatus = HTTP_CONNECTING;
    httpErrorStage = HTTP_ERROR_NONE;
    httpErrorCode = 0;
    httpResponseCode = 0;

    if (wlanStatus != WLAN_ONLINE)
    {
        setHttpError(HTTP_ERROR_WLAN_OFFLINE, 0);
        return 0;
    }

    socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (socketId < 0)
    {
        setHttpError(HTTP_ERROR_CONNECTION, errno);
        return 0;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr("192.168.1.20");

    result = connect(
        socketId,
        (struct sockaddr *)&serverAddress,
        sizeof(serverAddress)
    );

    if (result < 0)
    {
        close(socketId);
        setHttpError(HTTP_ERROR_CONNECTION, errno);
        return 0;
    }

    result = send(
        socketId,
        request,
        strlen(request),
        0
    );

    if (result < 0)
    {
        close(socketId);
        setHttpError(HTTP_ERROR_SEND, errno);
        return 0;
    }

    memset(response, 0, sizeof(response));

    while (totalReceived < (int)sizeof(response) - 1)
    {
        received = recv(
            socketId,
            response + totalReceived,
            sizeof(response) - 1 - totalReceived,
            0
        );

        if (received < 0)
        {
            close(socketId);
            setHttpError(HTTP_ERROR_STATUS, errno);
            return 0;
        }

        if (received == 0) break;
        totalReceived += received;
    }

    response[totalReceived] = '\0';
    close(socketId);

    if (sscanf(response, "HTTP/%*s %d", &statusCode) != 1)
    {
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

    socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (socketId < 0) return -2;

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr("192.168.1.20");

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
        "Host: 192.168.1.20:8080\r\n"
        "User-Agent: SpotatuiPSP/0.10\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        method,
        path
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

    socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (socketId < 0) return -2;

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr("192.168.1.20");

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
        "Host: 192.168.1.20:8080\r\n"
        "User-Agent: SpotatuiPSP/0.10\r\n"
        "Connection: close\r\n"
        "\r\n",
        path
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

    for (y = 48; y < 218; y += 6)
    {
        drawLine(
            17.0f,
            (float)y,
            463.0f,
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
    drawText(393.0f, 24.0f, "VER 0.11", 0.8f, COLOR_DIM);
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
   UI v0.11.4 - LAYOUT DEFINITIVO 480x272
-------------------------------------------------- */

void drawAppSidebar(int activeItem)
{
    const char *items[5] = {
        "NOW PLAYING",
        "SEARCH",
        "PLAYLISTS",
        "YOUR LIBRARY",
        "SETTINGS"
    };
    int i;

    /* Una sola divisione verticale: niente box annidati. */
    drawLine(132.0f, 14.0f, 132.0f, 258.0f, COLOR_GREEN);
    drawText(15.0f, 20.0f, "SPOTATUI", 1.12f, COLOR_GREEN);
    drawText(15.0f, 39.0f, "PSP", 1.12f, COLOR_GREEN);
    drawText(15.0f, 60.0f, "v0.11.4", 0.82f, COLOR_DIM);

    for (i = 0; i < 5; i++)
    {
        float y = 91.0f + (i * 27.0f);
        if (i == activeItem)
        {
            drawFilledRectangle(10.0f, y - 6.0f, 116.0f, 22.0f, COLOR_GREEN);
            drawText(15.0f, y, ">", 0.86f, COLOR_BG);
            drawText(31.0f, y, items[i], 0.74f, COLOR_BG);
        }
        else
        {
            drawText(15.0f, y, "|-", 0.68f, COLOR_GREEN);
            drawText(31.0f, y, items[i], 0.74f, COLOR_TEXT);
        }
    }

    drawLine(14.0f, 231.0f, 122.0f, 231.0f, COLOR_DIM);
    drawText(15.0f, 240.0f, "VR MUSIC", 0.66f, COLOR_DIM);
    drawText(15.0f, 252.0f, "TERMINAL", 0.66f, COLOR_DIM);
}

void drawContentTitle(const char *title)
{
    drawText(148.0f, 20.0f, title, 1.30f, COLOR_GREEN);
    drawLine(146.0f, 48.0f, 466.0f, 48.0f, COLOR_DIM);
}

void drawHome(void)
{
    /* HOME usa lo stesso design system; la selezione mantiene la logica del menu. */
    int visualItem;
    if (selectedMenu == 3) visualItem = 0;
    else if (selectedMenu == 0) visualItem = 1;
    else if (selectedMenu == 1) visualItem = 2;
    else if (selectedMenu == 2) visualItem = 3;
    else visualItem = 4;

    drawAppSidebar(visualItem);
    drawContentTitle("VR AUDIO TERMINAL");

    drawText(151.0f, 72.0f, "SYSTEM STATUS", 0.92f, COLOR_DIM);
    drawText(151.0f, 102.0f, "WLAN", 0.88f, COLOR_TEXT);
    drawText(350.0f, 102.0f, getWlanStatusText(), 0.88f, getWlanStatusColor());
    drawLine(151.0f, 121.0f, 454.0f, 121.0f, COLOR_DIM);

    drawText(151.0f, 137.0f, "SERVER", 0.88f, COLOR_TEXT);
    drawText(350.0f, 137.0f, httpStatus == HTTP_OK ? "OK" : "STANDBY", 0.88f,
             httpStatus == HTTP_OK ? COLOR_GREEN : COLOR_DIM);
    drawLine(151.0f, 156.0f, 454.0f, 156.0f, COLOR_DIM);

    drawText(151.0f, 172.0f, "SPOTIFY", 0.88f, COLOR_TEXT);
    drawText(350.0f, 172.0f, getSpotifyStatusText(), 0.82f, getSpotifyStatusColor());

    drawText(151.0f, 213.0f, "TACTICAL MUSIC INTERFACE", 0.78f, COLOR_DIM);
    drawText(151.0f, 234.0f, "X SELECT", 0.76f, COLOR_GREEN);
}

void drawSearchScreen(void)
{
    drawAppSidebar(1);
    drawContentTitle("SEARCH");
    drawText(151.0f, 85.0f, "SEARCH TERMINAL", 1.12f, COLOR_GREEN);
    drawLine(151.0f, 119.0f, 451.0f, 119.0f, COLOR_DIM);
    drawText(157.0f, 132.0f, "> WAITING FOR INPUT", 0.90f, COLOR_DIM);
    drawText(382.0f, 245.0f, "O BACK", 0.72f, COLOR_DIM);
}

void drawPlaylistsScreen(void)
{
    drawAppSidebar(2);
    drawContentTitle("PLAYLISTS");
    drawText(151.0f, 91.0f, "NO PLAYLIST DATA", 1.12f, COLOR_DIM);
    drawText(151.0f, 121.0f, "SPOTIFY LINK REQUIRED", 0.86f, COLOR_DIM);
    drawText(382.0f, 245.0f, "O BACK", 0.72f, COLOR_DIM);
}

void drawLibraryScreen(void)
{
    drawAppSidebar(3);
    drawContentTitle("YOUR LIBRARY");
    drawText(151.0f, 91.0f, "LIBRARY OFFLINE", 1.12f, COLOR_DIM);
    drawText(151.0f, 121.0f, "NO LOCAL CACHE", 0.86f, COLOR_DIM);
    drawText(382.0f, 245.0f, "O BACK", 0.72f, COLOR_DIM);
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

    drawAppSidebar(0);
    drawContentTitle("NOW PLAYING");

    drawText(392.0f, 17.0f, "SPOTIFY", 0.70f, COLOR_GREEN);
    drawText(392.0f, 32.0f, linkText, 0.66f, linkColor);

    /* Cover placeholder: un solo riquadro funzionale. */
    drawRectangle(150.0f, 67.0f, 86.0f, 86.0f, COLOR_GREEN);
    drawText(168.0f, 91.0f, "VR", 1.70f, COLOR_GREEN);
    drawText(164.0f, 120.0f, "AUDIO", 0.82f, COLOR_TEXT);

    drawText(250.0f, 72.0f,
             playerLoaded ? playerTrack : "NO ACTIVE TRACK",
             1.12f,
             playerLoaded ? COLOR_GREEN : COLOR_TEXT);
    drawText(250.0f, 100.0f,
             playerLoaded ? playerArtist : "WAITING FOR SPOTIFY",
             0.78f, COLOR_TEXT);
    drawText(250.0f, 130.0f,
             playerLoaded ? (playerIsPlaying ? "> PLAYING" : "|| PAUSED") : "- STANDBY",
             0.84f,
             playerLoaded ? COLOR_GREEN : COLOR_DIM);

    if (playerDurationMs > 0)
    {
        progressWidth = 196.0f * ((float)shownProgressMs / (float)playerDurationMs);
        if (progressWidth < 0.0f) progressWidth = 0.0f;
        if (progressWidth > 196.0f) progressWidth = 196.0f;
    }

    snprintf(elapsedText, sizeof(elapsedText), "%d:%02d", progressSeconds / 60, progressSeconds % 60);
    snprintf(durationText, sizeof(durationText), "%d:%02d", durationSeconds / 60, durationSeconds % 60);

    drawText(150.0f, 169.0f, elapsedText, 0.78f, COLOR_TEXT);
    drawRectangle(195.0f, 171.0f, 196.0f, 8.0f, COLOR_DIM);
    if (progressWidth > 0.0f)
        drawFilledRectangle(195.0f, 171.0f, progressWidth, 8.0f, COLOR_GREEN);
    drawText(407.0f, 169.0f, durationText, 0.78f, COLOR_TEXT);

    drawLine(150.0f, 195.0f, 458.0f, 195.0f, COLOR_DIM);

    /* Controlli grandi e leggibili, senza footer globale. */
    drawRectangle(150.0f, 207.0f, 70.0f, 42.0f, COLOR_GREEN);
    drawRectangle(226.0f, 207.0f, 91.0f, 42.0f, COLOR_GREEN);
    drawRectangle(323.0f, 207.0f, 64.0f, 42.0f, COLOR_GREEN);
    drawRectangle(393.0f, 207.0f, 65.0f, 42.0f, COLOR_GREEN);
    drawText(163.0f, 217.0f, "<|", 1.00f, COLOR_GREEN);
    drawText(158.0f, 237.0f, "PREV", 0.66f, COLOR_TEXT);
    drawText(260.0f, 216.0f, "||", 1.05f, COLOR_GREEN);
    drawText(237.0f, 237.0f, "X PLAY", 0.66f, COLOR_TEXT);
    drawText(342.0f, 217.0f, "|>", 1.00f, COLOR_GREEN);
    drawText(335.0f, 237.0f, "NEXT", 0.66f, COLOR_TEXT);
    drawText(411.0f, 217.0f, "<> ", 0.92f, COLOR_GREEN);
    drawText(402.0f, 237.0f, "SYNC", 0.66f, COLOR_TEXT);

    if (playerCommandRunning)
        drawText(360.0f, 190.0f, "COMMAND...", 0.58f, COLOR_TEXT);
}

void drawSettingsSelector(void)
{
    /* Selettore integrato nel pannello Settings. */
}

void drawSettingsScreen(void)
{
    char detailText[32];
    unsigned int c0 = selectedSetting == 0 ? COLOR_GREEN : COLOR_TEXT;
    unsigned int c1 = selectedSetting == 1 ? COLOR_GREEN : COLOR_TEXT;
    unsigned int c2 = selectedSetting == 2 ? COLOR_GREEN : COLOR_TEXT;

    drawAppSidebar(4);
    drawContentTitle("SETTINGS");

    if (selectedSetting == 0) drawFilledRectangle(148.0f, 72.0f, 300.0f, 25.0f, COLOR_DIM);
    drawText(157.0f, 79.0f, "WLAN", 0.90f, c0);
    drawText(350.0f, 79.0f, getWlanStatusText(), 0.84f, getWlanStatusColor());

    if (selectedSetting == 1) drawFilledRectangle(148.0f, 107.0f, 300.0f, 25.0f, COLOR_DIM);
    drawText(157.0f, 114.0f, "SERVER TEST", 0.90f, c1);
    drawText(350.0f, 114.0f, getHttpStatusText(), 0.84f, getHttpStatusColor());

    if (selectedSetting == 2) drawFilledRectangle(148.0f, 142.0f, 300.0f, 25.0f, COLOR_DIM);
    drawText(157.0f, 149.0f, "SPOTIFY", 0.90f, c2);
    drawText(350.0f, 149.0f, getSpotifyStatusText(), 0.78f, getSpotifyStatusColor());

    drawLine(151.0f, 181.0f, 451.0f, 181.0f, COLOR_DIM);
    drawText(157.0f, 194.0f, "CRT MODE", 0.78f, COLOR_DIM);
    drawText(350.0f, 194.0f, "ACTIVE", 0.78f, COLOR_GREEN);

    detailText[0] = '\0';
    if (spotifyStatus == SPOTIFY_CONNECTED && spotifyUser[0] != '\0')
        snprintf(detailText, sizeof(detailText), "%s", spotifyUser);
    else if (wlanStatus == WLAN_ONLINE && wlanIp[0] != '\0')
        snprintf(detailText, sizeof(detailText), "%s", wlanIp);

    if (detailText[0] != '\0')
    {
        drawText(157.0f, 220.0f, "INFO", 0.70f, COLOR_DIM);
        drawText(201.0f, 220.0f, detailText, 0.70f, COLOR_GREEN);
    }
    drawText(382.0f, 245.0f, "O BACK", 0.72f, COLOR_DIM);
}

/* --------------------------------------------------
   RENDER
-------------------------------------------------- */

void drawInterface(void)
{
    sceGuStart(
        GU_DIRECT,
        list
    );

    sceGuClearColor(
        COLOR_BG
    );

    sceGuClear(
        GU_COLOR_BUFFER_BIT
    );


    drawScanlines();

    if (bootActive)
    {
        drawBootScreen();
        bootFrame++;

        if (bootFrame >= 150)
        {
            bootActive = 0;
        }
    }
    else
    {
        drawFrame();

        switch (currentScreen)
    {
        case SCREEN_HOME:
            drawHome();
            break;

        case SCREEN_SEARCH:
            drawSearchScreen();
            break;

        case SCREEN_PLAYLISTS:
            drawPlaylistsScreen();
            break;

        case SCREEN_LIBRARY:
            drawLibraryScreen();
            break;

        case SCREEN_NOW_PLAYING:
            drawNowPlayingScreen();
            break;

        case SCREEN_SETTINGS:
            drawSettingsScreen();
            break;
        }
    }


    sceGuFinish();

    sceGuSync(
        GU_SYNC_FINISH,
        GU_SYNC_WHAT_DONE
    );

    sceDisplayWaitVblankStart();

    sceGuSwapBuffers();
}


/* --------------------------------------------------
   NAVIGAZIONE
-------------------------------------------------- */

void openSelectedMenu(void)
{
    switch (selectedMenu)
    {
        case 0:
            currentScreen = SCREEN_SEARCH;
            break;

        case 1:
            currentScreen = SCREEN_PLAYLISTS;
            break;

        case 2:
            currentScreen = SCREEN_LIBRARY;
            break;

        case 3:
            currentScreen = SCREEN_NOW_PLAYING;
            startPlayerRefresh();
            break;

        case 4:
            currentScreen = SCREEN_SETTINGS;
            selectedSetting = 0;
            break;
    }
}


/* --------------------------------------------------
   INPUT
-------------------------------------------------- */

void updateController(void)
{
    SceCtrlData pad;

    static unsigned int oldButtons = 0;


    sceCtrlPeekBufferPositive(
        &pad,
        1
    );

    if (bootActive)
    {
        oldButtons = pad.Buttons;
        return;
    }


    /*
       HOME
    */

    if (currentScreen == SCREEN_HOME)
    {
        if (
            (pad.Buttons & PSP_CTRL_DOWN) &&
            !(oldButtons & PSP_CTRL_DOWN)
        )
        {
            selectedMenu++;

            if (selectedMenu > 4)
            {
                selectedMenu = 0;
            }
        }


        if (
            (pad.Buttons & PSP_CTRL_UP) &&
            !(oldButtons & PSP_CTRL_UP)
        )
        {
            selectedMenu--;

            if (selectedMenu < 0)
            {
                selectedMenu = 4;
            }
        }


        if (
            (pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldButtons & PSP_CTRL_CROSS)
        )
        {
            openSelectedMenu();
        }
    }


    /*
       SETTINGS
    */

    else if (currentScreen == SCREEN_SETTINGS)
    {
        if (
            (pad.Buttons & PSP_CTRL_DOWN) &&
            !(oldButtons & PSP_CTRL_DOWN)
        )
        {
            selectedSetting++;

            if (selectedSetting > 3)
            {
                selectedSetting = 0;
            }
        }


        if (
            (pad.Buttons & PSP_CTRL_UP) &&
            !(oldButtons & PSP_CTRL_UP)
        )
        {
            selectedSetting--;

            if (selectedSetting < 0)
            {
                selectedSetting = 3;
            }
        }


        /*
           Se WLAN è selezionato,
           X avvia la connessione.
        */

        if (
            selectedSetting == 0 &&
            (pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldButtons & PSP_CTRL_CROSS)
        )
        {
            startWlanConnection();
        }


        /*
           Se SERVER TEST è selezionato,
           X contatta il backend locale via socket TCP.
        */

        if (
            selectedSetting == 1 &&
            (pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldButtons & PSP_CTRL_CROSS)
        )
        {
            startHttpTest();
        }


        if (
            selectedSetting == 2 &&
            (pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldButtons & PSP_CTRL_CROSS)
        )
        {
            startSpotifyCheck();
        }


        if (
            (pad.Buttons & PSP_CTRL_CIRCLE) &&
            !(oldButtons & PSP_CTRL_CIRCLE)
        )
        {
            currentScreen = SCREEN_HOME;
        }
    }


    /*
       ALTRE SCHERMATE
    */

    else
    {
        if (currentScreen == SCREEN_NOW_PLAYING)
        {
            if (
                (pad.Buttons & PSP_CTRL_CROSS) &&
                !(oldButtons & PSP_CTRL_CROSS)
            )
            {
                sendPlayerCommand(PLAYER_CMD_PLAY_PAUSE);
            }

            if (
                (pad.Buttons & PSP_CTRL_LEFT) &&
                !(oldButtons & PSP_CTRL_LEFT)
            )
            {
                sendPlayerCommand(PLAYER_CMD_PREVIOUS);
            }

            if (
                (pad.Buttons & PSP_CTRL_RIGHT) &&
                !(oldButtons & PSP_CTRL_RIGHT)
            )
            {
                sendPlayerCommand(PLAYER_CMD_NEXT);
            }

            if (
                (pad.Buttons & PSP_CTRL_TRIANGLE) &&
                !(oldButtons & PSP_CTRL_TRIANGLE)
            )
            {
                startPlayerRefresh();
            }
        }

        if (
            (pad.Buttons & PSP_CTRL_CIRCLE) &&
            !(oldButtons & PSP_CTRL_CIRCLE)
        )
        {
            currentScreen = SCREEN_HOME;
        }
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