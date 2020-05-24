/*
 * Jane Hacker (2020) - janehacker.com
 * Generalplus camera chip network commands hacking, using MountDog Action cam,
 *  should also apply to the SQXX (SQ8, SQ10, etc.) cube cams, and more...
 */

 /* Extra notes
 Port 8080 is a web server, seeming for streaming video at the url:
 http://192.168.25.1:8080/?action=stream
You can use VLC to play/record/etc. with it
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <argp.h>

#include <poll.h>
#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */


/* Message layout */
/*
    GPSOCKETXXCCRRDD...

    XX - xmit - See: CLIENT_.._BYTE? defines
    CC - command - See: ...._CMD defines
    RR - Response or subcommand - See: MENU_...._RES defines
    DD... - Data, can be many bytes
*/


/* These are the first 8 bytes of the messages sent to/from the camera */
// Regular command and data messages
#define SOCKET_TOK  "GPSOCKET"
// Stream of (video?) data from the camera
#define STREAM_TOK  "GPSTREAM"
// Image? data from the encoder?
#define ENCODER_TOK "GPEncode"

#define TOK_START   'G'

/* These are the 9 and 10th bytes in a message */
 // Data to camera
#define CLIENT_TX_BYTES 0x01, 0x00
// Data from camera
#define CLIENT_RX_BYTES 0x02, 0x00
// Error from camera
#define CLIENT_ER_BYTES 0x03, 0x00
// The "r" in GPEncoder
#define CLIENT_EN_BYTE  0x72


/* These are the 11 and 12th bytes in a message (Called "commands" here) */
// Alternate heartbeat for the camera (maybe)
#define RESET_CMD       0x00, 0x00
// Heartbeat to camera
#define HRTB_CMD        0x00, 0x01
// Second command sent to camera
#define MENU_CMD        0x00, 0x02
// First command sent to camera
#define HELO_CMD        0x00, 0x05
// Encoded image request
#define ENCD_CMD        0x03, 0x04
// Used to set the mode of the camera, see: cmdT
#define SETMODE_CMD     0x04, 0x00

/*
// Configuration request, 0x04, 0x00, the next two bytes select the attribute
//  0x00, 0x00 - 0x01, 0x03 (0x10, 0x02)
#define CONF_GET_CMD    0x04, 0x00
// Confiruration set, the next two bytes are the option to be set
//  0x00, 0x00 - 0x04, 0x02, next: 0x00, 0x00, 0x01, then the index byte
#define CONF_SET_CMD    0x04, 0x01
*/

/*
    Setting SSID config option
    0x04, 0x01, 0x00, 0x03, 0x00, 0x00, 0x08, 0x53, 0x70, 0x6f, 0x72, 0x74, 0x73, 0x44, 0x41
    [conf_set]  [Set SSID]  [ padding]  [  ]  S     p     o     r     t     s     D     A
                            byte count =|
*/


/* These are the 13 and 14th bytes in messages of the MENU_CMD types */
// Expect another message with menu file data.
#define SUB_CMD_MORE    0xF2, 0x00
// The data included in this message is the
//  last to be sent, this message should
//  include MENU_EOF at the end of it,
//  expect one last message(MENU_DONE_RES)
#define SUB_CMD_LAST    0xD8, 0x00
// A final message, with no data to signify
// that the camera is done sending menudata
// A final message, with no data to signify
// that the camera is done sending menudata
#define SUB_CMD_DONE    0x00, 0x00

/* Server defines */
#define MIN_PORT            (1)
#define MAX_SERV_CONS       (1)
#define DEFAULT_LOCAL_ADDR  "0.0.0.0"

/* Client defines */
#define DEFAULT_PORT        (8081)
#define DEFAULT_ADDR        "192.168.25.1"

#define HELO_DATA_SIZ       (4)
//(strlen(STREAM_TOK) + 2 command bytes)
#define MSG_LEN_MIN         (8 + 2)

// Used in generalPlusMsgLib:constructMessage
#define NUM_SECTIONS    (5)


/** Enumerations and Strings **/

/* Messages (one or more packets) */
typedef enum messageSections {
    TOKEN,
    XMIT,
    COMMAND,
    SUBCOMMAND,
    DATA
} msgSects;

/* Sockets (Network) */
typedef enum socketType {
    CLIENT,
    SERVER
} SocketType;

/* Tokens */
#define TOK_LEN         (8)
#define NUM_TOKS        (1 + 3)
#define TOK_STR_LEN     (7 + 1)

typedef enum tokTypes {
    TOK_UNKNOWN,
    SOCKET,
    STREAM,
    ENCODER
} tokTypes;

char tokBytes[NUM_TOKS][TOK_LEN] = {
    "\0",
    SOCKET_TOK,
    STREAM_TOK,
    ENCODER_TOK
};

char tokStrings[NUM_TOKS][TOK_STR_LEN] = {
    "UNKNOWN",
    "Socket",
    "Stream",
    "Encoder"
};

/* Xmits */
#define XMIT_LEN        (2)
#define NUM_XMITS       (4 + 1)
#define XMIT_STR_LEN    (8 + 1)

typedef enum xmitTypes {
    XMIT_UNKNOWN,
    TX,
    RX,
    ERROR,
    ENCODE
} xmitTypes;

char xmitBytes[NUM_XMITS][XMIT_LEN] = {
    "\0",
    { CLIENT_TX_BYTES },
    { CLIENT_RX_BYTES },
    { CLIENT_ER_BYTES },
    { CLIENT_EN_BYTE }     // The "r" in GPEncoder
};

char xmitStrings[NUM_XMITS][XMIT_STR_LEN] = {
    "UNKNOWN",
    "Transmit",
    "Recieve",
    "Error",
    "Encoder"
};

/* Commands */
#define CMD_LEN         (2)
#define NUM_CMDS        (8)
#define NUM_KNOWN_CMDS  (8)
#define CMD_STR_LEN     (13 + 1)

typedef enum cmdTypes {
    CMD_UNKNOWN,
    RESET,     // 0x00, 0x00 data: 0x00 = video, 0x01 = still image
                //  Response : 47 50 53 4f 43 4b 45 54 02 00 00 00 00 00
                //             G  P  S  T  R  E  A  M  {RES} {CMD} {DATA}

    HEARTBEAT,  // HRTB_CMD - 0x00, 0x01
    MENUFILE,   // MENU_CMD - Request or response for menu.xml from the camera
    RESRV3,     // 0x00, 0x03
    RESRV4,     // 0x00, 0x04
    HELO,       // HELO_CMD - Command sent on initial connection to the camera
    SETMODE
} cmdTypes;

char cmdBytes[NUM_CMDS][CMD_LEN] = {
    "\0",
    { RESET_CMD },
    { HRTB_CMD },
    { MENU_CMD },
    { 0x00, 0x03 },
    { 0x00, 0x04 },
    { HELO_CMD },
    { SETMODE_CMD }
};

char cmdStrings[NUM_CMDS][CMD_STR_LEN] = {
    "UNKNOWN",
    "Reset",
    "Heartbeat",
    "MENU File",
    "Reserved 3",
    "Reserved 4",
    "Hello",
    "SetMode"
};

/* Subcommands */
typedef enum subCmdTypes {
    SUBCMD_UNKNOWN,
    MENU_MORE,  // 0xF2, 0x00
    MENU_LAST,  // 0xD8, 0x00
    MENU_DONE,  // 0x00, 0x00
    HELO_TX     // 0x00, 0x4d
} subCmdTypes;

#define NUM_SUBCMDS     (6)
#define SUBCMD_LEN      (2)

char subCmdBytes[NUM_SUBCMDS][SUBCMD_LEN] = {
    "\0",
    { SUB_CMD_MORE },
    { SUB_CMD_LAST },
    { SUB_CMD_DONE },
    { 0x00, 0x4d   },
    { 0xe,  0x0     }
};

#define SUBCMD_STR_LEN  (16 + 1)

char subCmdStrings[NUM_SUBCMDS][SUBCMD_STR_LEN] = {
    "UNKNOWN",
    "MENU More",
    "MENU Last",
    "Snake eyes",
    "Helo TX",
    "Heartbreak"
};

/** Struct definitions **/
#define CMD_MSG_TYPES_NAME_LEN  (15 + 1)

typedef struct cmdMessageTypes {
    char    name[CMD_MSG_TYPES_NAME_LEN];
    u_int   length_tx;
    u_int   length_rx;
    u_int   length_rx_max;
    char    cmd_bytes[2];
} CmdMessageTypes;

typedef struct message {
    tokTypes    token;
    xmitTypes   xmit;
    cmdTypes    cmd;
    char        cmdRaw[2];
    subCmdTypes subCmd;
    char        subCmdRaw[2];
    char*       data;
    u_int       dataLen;
} Message;

typedef struct msgSections {
    char* bytesIn;
    u_int byteLen;
} MsgSections;


/** Global structure arrays **/

/* Command Message Types */
#define HELO_MSG_INDEX  (4)
#define MENU_MSG_INDEX  (3)

// 16 bytes
#define HRTB_RES        0x0e, 0x00, 0x00, 0x02, 0x04, 0x01, 0x00, 0x2c, 0x4d, 0x00, 0x00, 0x00, 0x1f, 0x99, 0x00, 0x00

CmdMessageTypes cpt[NUM_KNOWN_CMDS] = {
    {
        "Reset",
        13,
        14,
        14,
        { RESET_CMD }
    },
    {
        "Heartbeat",
        13,
        14,
        14,
        { HRTB_CMD }
    },
    {
        "Menu.xml req",
        12,
        256,
        20000,
        { MENU_CMD }
    },
    {
        "HELO req",
        18,
        20,
        20,
        { HELO_CMD }
    },
    {
        "Encoded jpeg",
        14,
        1460,
        1460,
        { ENCD_CMD }
    }
    /*,
    {
        "Config Get",
        16,
        15, // 0x00, 0x01, 0x00, 0x00 - for binary options?
        22, //For the wifi SSID and passphrase
        { CONF_GET_CMD }
    },
    {
        "Config Set",
        18,
        14,
        14,
        { CONF_SET_CMD }
    }
    */
};


/** Function definitions **/
int         main(int argc, char* argv[]);
int         heloMessage(int sock);
int         menuMessage(int sock);

ssize_t     readMessage(int sockd, void *vptr, size_t maxlen, u_int *endData);
ssize_t     writeMessage(int sockd, const void *vptr, size_t n);

Message*    decodeMessage(const char * pd, size_t pdLen);
void        inspectMessage(const char * pd, size_t pdLen);
void        parseMessages(int bytesRead, char * buffer);

int         getTokStart(const char * firstPtr, u_int len, char** gArray, u_int count);
int         countTokStart(const char * firstPtr, u_int len);
int         findAllTokens(const char * pd, size_t pdLen, char** gArray);

u_int       appendBytes(char* outBuffer, const char* bytesIn, u_int byteLen);
int         checkBytes(char* buffer, msgSects msgSect, char* bytesStart);
int         msgLoop(int sock, Message msg, u_int bufferOutSize);
int         loop(int sock);
int         setupSocket(int conn, char *szAddress, short int port, SocketType st);
int         relay(int connS, int connC);
