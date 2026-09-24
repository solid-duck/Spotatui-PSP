#ifndef SPOTATUI_NETWORK_H
#define SPOTATUI_NETWORK_H

#include <pspkernel.h>

typedef enum {
    WLAN_OFFLINE,
    WLAN_CONNECTING,
    WLAN_ONLINE,
    WLAN_ERROR
} WlanStatus;

typedef enum {
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

typedef enum {
    HTTP_IDLE,
    HTTP_CONNECTING,
    HTTP_OK,
    HTTP_ERROR
} HttpStatus;

typedef enum {
    HTTP_ERROR_NONE,
    HTTP_ERROR_WLAN_OFFLINE,
    HTTP_ERROR_CONNECTION,
    HTTP_ERROR_SEND,
    HTTP_ERROR_STATUS,
    HTTP_ERROR_THREAD_CREATE,
    HTTP_ERROR_THREAD_START
} HttpErrorStage;

#define BACKEND_HTTP_PORT 8080
#define BACKEND_DISCOVERY_PORT 8081
#define BACKEND_DEV_IP "172.20.10.4"

extern volatile WlanStatus wlanStatus;
extern volatile WlanErrorStage wlanErrorStage;
extern volatile int wlanErrorCode;
extern volatile int wlanThreadRunning;
extern char wlanIp[32];

extern char backendIp[32];
extern volatile int backendDiscovered;
extern volatile int backendConnectError;

extern volatile HttpStatus httpStatus;
extern volatile HttpErrorStage httpErrorStage;
extern volatile int httpErrorCode;
extern volatile int httpThreadRunning;
extern volatile int httpResponseCode;

int initNetworkStack(void);
int updateWlanFromApctl(void);
void startWlanConnection(void);

int discoverBackend(void);
int ensureBackend(void);

int backendGet(
    const char *path,
    char *response,
    int responseSize,
    int *statusCode
);

int backendCommand(
    const char *method,
    const char *path,
    char *response,
    int responseSize,
    int *statusCode
);

const char *getHttpBody(char *response);

const char *getWlanStatusText(void);
const char *getWlanErrorStageText(void);
const char *getHttpStatusText(void);
const char *getHttpErrorStageText(void);

void startHttpTest(void);

#endif
