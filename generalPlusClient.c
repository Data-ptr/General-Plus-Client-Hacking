/*
 * Jane Hacker (2020) - janehacker.com
 * Generalplus camera chip network commands hacking, using MountDog Action cam,
 *  should also apply to the SQXX (SQ8, SQ10, etc.) cube cams, and more...
 */

#include "generalPlusClient.h"
#include "generalPlusMsgLib.c"
#include "generalPlusMsgHelpers.c"
#include "lowLevelMessageXmit.c"


/** Global variables **/
const char tokenRaw[TOK_LEN] = SOCKET_TOK;


/** Main() **/
int main(int argc, char* argv[]) {
    int                conn_s;                    /*  connection socket         */
    short  int         port      = DEFAULT_PORT;  /*  port number               */
    struct sockaddr_in servaddr;                  /*  socket address structure  */
    char              *szAddress = DEFAULT_ADDR;  /*  Holds remote IP address   */
    char              *szPort;                    /*  Holds remote port         */

    struct arguments arguments;

    arguments.remoteAddress = DEFAULT_ADDR;
    arguments.remotePort    = DEFAULT_PORT;
    arguments.relay         = false;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    printf("Creating socket...\n");

    if( (conn_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	       fprintf(stderr, "GPCLIENT: Error creating listening socket.\n");
    }

    if(-1 == conn_s) {
        printf("Checking socket creation error...\n");

        //Socket error
        printf("Error: unable to create socket %d", conn_s);

        switch(errno) {
            case EPROTONOSUPPORT:
                printf("Protocol not supported %d", errno);
            break;
            case EACCES:
                printf("Permission denied %d", errno);
            break;
            default:
                printf("errno = %d", errno);
            break;
        }

        exit(EXIT_FAILURE);
    }

    printf("Setting up server address struct...\n");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);

    printf("Setting server's IP address in socket...\n");

    if(inet_aton(szAddress, &servaddr.sin_addr) <= 0 ) {
    	printf("GPCLIENT: Invalid remote IP address.\n");
    	exit(EXIT_FAILURE);
    }

    printf("Connecting to server...\n");

    if(connect(conn_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
    	printf("GPCLIENT: Error calling connect(); errno = %i; %s\n", errno, inet_ntoa(servaddr.sin_addr));
    	exit(EXIT_FAILURE);
    }

    // Send HELO message
    heloMessage(conn_s);

    // Send Menu message
    return menuMessage(conn_s);
}

//TODO: make msgSingle function that is called from here
int heloMessage(int sock) {
    u_int   endData         = 0;
    ssize_t bytesRead       = 0;
    ssize_t bytesWritten    = 0;
    ssize_t msgOutLen       = 0;
    ssize_t msgInLen        = cpt[HELO_MSG_INDEX].length_rx;
    u_int   bufferInSize    = cpt[HELO_MSG_INDEX].length_rx_max;
    u_int   bufferOutSize   = cpt[HELO_MSG_INDEX].length_tx;

    char    bufferIn[bufferInSize];
    char    bufferOut[bufferOutSize];
    char    heloData[HELO_DATA_SIZ]   = { 0x54, 0x33, 0x2a, 0x33 };

    // null byte out buffer
    memset(bufferIn, '\0', bufferInSize);
    memset(bufferOut, '\0', bufferOutSize);

    // Setup message struct
    Message heloMsg = {
        SOCKET,
        TX,
        HELO,
        HELO_TX,
        heloData,
        HELO_DATA_SIZ
    };

    // Build the message from the message struct above
    msgOutLen       = constructMessage(heloMsg, bufferOut);
    // Write message to camera
    bytesWritten    = writeMessage(sock, bufferOut, msgOutLen);

    if(bytesWritten > 0) {
        // Get response back from camera
        bytesRead   = readMessage(sock, bufferIn, msgInLen, &endData);

        printf("Inspecting message here: A\n\n");
        inspectMessage(bufferOut, msgOutLen);

        if(bytesRead > 0) {
            printf("Inspecting message here: B\n\n");
            inspectMessage(bufferIn, msgInLen);
        } else {
            printf("\n\nNo bytes read!\n\n");
            return EXIT_FAILURE;
        }
    } else {
        printf("\n\nNo bytes written!\n\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int menuMessage(int sock) {
    // Setup message struct
    Message menuMsg = {
        SOCKET,
        TX,
        MENUFILE,
        SUBCMD_UNKNOWN,
        NULL,
        0
    };

    return msgLoop(
        sock,
        menuMsg,
        cpt[MENU_MSG_INDEX].length_tx
    );
}
