/*
 * Jane Hacker (2020) - janehacker.com
 * Generalplus camera chip network commands hacking, using MountDog Action cam,
 *  should also apply to the SQXX (SQ8, SQ10, etc.) cube cams, and more...
 */

#include "generalPlusClient.h"
#include "generalPlusAppHelpers.c"
#include "generalPlusMsgLib.c"
#include "generalPlusMsgHelpers.c"
#include "lowLevelMessageXmit.c"


/** Global variables **/
const char tokenRaw[TOK_LEN] = SOCKET_TOK;


/** Main() **/
int main(int argc, char* argv[]) {
    int                 conn_c;                 /*  connection socket (client)*/
    int                 conn_s;                 /*  connection socket (server)*/
    struct arguments    arguments;

    /* Default arguments */
    arguments.relay         = false;
    arguments.remoteAddress = DEFAULT_ADDR;
    arguments.remotePort    = DEFAULT_PORT;
    arguments.localAddress  = DEFAULT_LOCAL_ADDR;
    arguments.localPort     = DEFAULT_PORT;

    printf("\n\nDEFAULT Arguments:\n");
    inspectArguments(&arguments);

    /* Set arguments */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    printf("\n\nUsing Arguments:\n");
    inspectArguments(&arguments);

    /* If a relay, wait for app to connect */
    if(arguments.relay) {
        conn_s = setupSocket(conn_s, arguments.localAddress, arguments.localPort, SERVER);

        /* Setup socket for client */
        conn_c = setupSocket(conn_c, arguments.remoteAddress, arguments.remotePort, CLIENT);

        /* Tie conn_s to conn_c while inspectig messages */
        return relay(conn_s, conn_c);
    } else {
        /* Setup socket for client */
        conn_c = setupSocket(conn_c, arguments.remoteAddress, arguments.remotePort, CLIENT);

        // Send HELO message
        heloMessage(conn_c);

        // Send Menu message
        return menuMessage(conn_c);
    }
}

int relay(int connS, int connC) {
    u_int   endData         = 0;
    ssize_t bytesRead       = 0;
    ssize_t bytesWritten    = 0;
    ssize_t msgInLen        = 256;
    u_int   bufferInSize    = 256;

    char    bufferIn[bufferInSize];

    int     rXtX = 0;

    //Check
    if(0 < connS && 0 < connC) {
        while(true) {
            // null byte out buffer
            memset(bufferIn, '\0', bufferInSize);

            //Read
            bytesRead   = readMessage(rXtX ? connC : connS, bufferIn, msgInLen, &endData);

            //Inspect
            if(bytesRead > 0) {
                printf("Incomming message:\n");
                inspectMessage(bufferIn, bytesRead - 1);

                bytesWritten    = writeMessage(rXtX ? connS : connC, bufferIn, bytesRead - 1);


                if(bytesWritten > 0) {
                    printf("Relayed message\n");
                    inspectMessage(bufferIn, bytesWritten);
                } else {
                    printf("\n\nNo bytes written!\n\n");
                    //return EXIT_FAILURE;
                }

            } else {
                printf("\n\nNo bytes read!\n\n");
                return EXIT_FAILURE;
            }

            rXtX = !rXtX;
        }
        //Write
        exit(EXIT_SUCCESS);
    } else {
        exit(EXIT_FAILURE);
    }
}

int setupSocket(int conn, char *szAddress, short int port, SocketType st) {
    struct sockaddr_in  addr;                     /*  socket address structure*/
    struct sockaddr     retAddr;                  /*  socket address structure*/
    socklen_t           addrLen     = sizeof(addr);
    socklen_t           retAddrLen  = sizeof(retAddr);
    char               *szPort;                   /*  Holds remote port       */
    bool                isClient    = (CLIENT == st);

    printf("Setting up address struct...\t");

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);

    printf("Done\n");

    printf("Setting IP address in socket...\t");

    if(inet_aton(szAddress, &addr.sin_addr) <= 0 ) {
        printf("GPCLIENT: Invalid remote IP address.\n");
        exit(EXIT_FAILURE);
    }

    printf("Done\n");

    printf("Creating %s socket...\t", isClient ? "client" : "server" );

    if( (conn = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
           fprintf(stderr, "GPCLIENT: Error creating listening socket.\n");
    }

    if(-1 == conn) {
        printf("Checking socket creation error...\n");

        //Socket error
        printf("Error: unable to create socket %d", conn);

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

    printf("Done\n");

    if(isClient) {
        /* Client */
        printf("Connecting to camera...\t\t");

        if(connect(conn, (struct sockaddr *) &addr, addrLen) < 0) {
            printf("GPCLIENT: Error calling connect(); errno = %i; %s\n", errno, inet_ntoa(addr.sin_addr));
            exit(EXIT_FAILURE);
        }

        printf("Done\n");
    } else {
        /* Server */
        printf("BINDing server to address...\t");

        while(-1 == bind(conn, (struct sockaddr *) &addr, addrLen)) {
            if(48 == errno) {
                printf("\n Trying again...\t");
                sleep(10);
            } else {
                // Error: unable to bind...
                printf("GPCLIENT: Error calling bind(); errno = %i; %s\n", errno, inet_ntoa(addr.sin_addr));
                exit(EXIT_FAILURE);
            }
        }

        printf("Done\n");

        if(listen(conn, MAX_SERV_CONS) == -1) {
            // Error...
            printf("GPCLIENT: Error calling listen()\n");
            exit(EXIT_FAILURE);
        }

        printf("Waiting for app to connect...\twaiting...\t");

        conn = accept(conn, &retAddr, &retAddrLen);

        if (conn == -1) {
            // Error...
            printf("GPCLIENT: Error calling accept()\n");
            exit(EXIT_FAILURE);
        }

        printf("Done\n");
    }

    return conn;
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
        "\0\0",
        HELO_TX,
        "\0\0",
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
        "\0\0",
        SUBCMD_UNKNOWN,
        "\0\0",
        NULL,
        0
    };

    return msgLoop(
        sock,
        menuMsg,
        cpt[MENU_MSG_INDEX].length_tx
    );
}
