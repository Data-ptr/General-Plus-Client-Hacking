/*
 * Jane Hacker (2020) - janehacker.com
 * Generalplus camera chip network commands hacking, using MountDog Action cam,
 *  should also apply to the SQXX (SQ8, SQ10, etc.) cube cams, and more...
 */


/* Produce a message byte array from a filled in message struct */
ssize_t constructMessage(Message msg, char* outBuffer){
    u_int msgSize     = 0;

    MsgSections ms[NUM_SECTIONS] = {
        {   tokBytes[msg.token],        TOK_LEN     },  // Tok
        {   xmitBytes[msg.xmit],        XMIT_LEN    },  // Xmit
        {   cmdBytes[msg.cmd],          CMD_LEN     },  // Cmd
        {   subCmdBytes[msg.subCmd],    SUBCMD_LEN  },  // Subcmd
        {   msg.data,                   msg.dataLen }   // Data
    };

    for(int i = 0; i < NUM_SECTIONS; i++) {
        if(3 > i || (3 == i && ms[i].bytesIn) || (4 == i && ms[i].bytesIn)) {
            msgSize += appendBytes(
                (outBuffer + msgSize),
                ms[i].bytesIn,
                ms[i].byteLen
            );
        }
    }

    return (ssize_t)msgSize;
}

/* Pretty prints message struct data */
void inspectMessage(const char * pd, size_t pdLen) {
    char        currByte        = '\0';
    // Make empty Message struct, decode packet data into it
    Message    *decodedMessage  = decodeMessage(pd, pdLen);

    if(NULL != decodedMessage) {
        printf("\n\
Token:\t%s\n\
xmit:\t%s\n\
cmd:\t%s\t{ 0x%x, 0x%x }\n\
subCmd:\t%s\t{ 0x%x, 0x%x }\n\
datalen:\t%d\n",
            tokStrings[decodedMessage->token],
            xmitStrings[decodedMessage->xmit],

            cmdStrings[decodedMessage->cmd],
            decodedMessage->cmdRaw[0], decodedMessage->cmdRaw[1],

            subCmdStrings[decodedMessage->subCmd],
            decodedMessage->subCmdRaw[0], decodedMessage->subCmdRaw[1],

            decodedMessage->dataLen
        );

        if(decodedMessage->dataLen > 0) {
            printf("Data:\n{\n");

            for(int i = 0; i < decodedMessage->dataLen; i++) {
                currByte = decodedMessage->data[i];

                char asciiChar[3] = { '"', currByte, '"' };

                printf("0x%x %s %c", currByte, (currByte > 32 && currByte < 126 ? asciiChar : "   "), (!i || i % 5 ? '\t' : '\n'));
            }

            printf("\n}\n\n");
        } else {
            printf("\n");
        }

        if(NULL != decodedMessage->data) {
            free(decodedMessage->data);
        }

        free(decodedMessage);
    } else {
        //printf("decodeMessage returned NULL\n\n");
    }
}


/* Produce a filled in message struct from a message byte array */
Message* decodeMessage(const char * pd, size_t pdLen) {
    // Create message struct to return
    Message* sp = malloc(sizeof(Message));
    memset(sp, 0, sizeof(Message));

    // Create and initilize holding buffers
    char    tokBuf[TOK_LEN] = "\0";
    char    xmitBuf[2]      = "\0\0";
    char    cmdBuf[2]       = "\0\0";
    char    subCmdBuf[2]    = "\0\0";
    char**  tokArray        = NULL;
    int     tokCount        = 0;
    u_int   dataLen         = 0;

    // Find all the 'G's in the entire message
    //  Check all of them to see it they are a token
    tokCount = findAllTokens(pd, pdLen, tokArray);

    // Find the start of the first (or only) token
    //  (assuming all tokens with a 'G')
    char*   tokenStart      = strchr(pd, TOK_START);


    if(NULL == tokenStart) {
        //printf("No token in message\n");
        return NULL;
    } else {
        // Does the message contain enough data for a token and data bytes?
        if(pdLen >= MSG_LEN_MIN) {
            // Check token type
            int messageStart    = tokenStart - pd;
            char* xmitStart     = tokenStart + TOK_LEN;     // Get start of xmit bytes
            char* cmdStart      = xmitStart + XMIT_LEN;     // Get start of cmd bytes
            char* subCmdStart   = cmdStart + XMIT_LEN;      // Get start of subCmd bytes
            char* dataStart     = subCmdStart + SUBCMD_LEN; // Get start of data bytes

            // Check token
            int token = checkBytes(tokBuf, TOKEN, tokenStart);

            if(0 <= token) {
                sp->token = token;
            } else {
                printf("ERROR: Message token type returned: %d", token);
            }

            // Check is rx or tx
            int xmit = checkBytes(xmitBuf, XMIT, xmitStart);

            if(0 <= xmit) {
                sp->xmit = xmit;
            } else {
                printf("ERROR: Message xmit type returned: %d", xmit);
            }

            // Check type of command
            int cmd = checkBytes(cmdBuf, COMMAND, cmdStart);

            if(0 <= cmd) {
                sp->cmd = cmd;
            } else {
                printf("ERROR: Message command type returned: %d", cmd);
            }

            memcpy(sp->cmdRaw, cmdStart, XMIT_LEN);

            // Is there subcmd
            if(pdLen >= MSG_LEN_MIN + CMD_LEN + SUBCMD_LEN) {
                // Check type of subcommand (response)
                int subcmd = checkBytes(subCmdBuf, SUBCOMMAND, subCmdStart);

                if(0 <= subcmd) {
                    sp->subCmd = subcmd;
                } else {
                    printf("ERROR: Message sub-command type returned: %d", subcmd);
                    sp->subCmd = SUBCMD_UNKNOWN;
                }

                memcpy(sp->subCmdRaw, subCmdStart, SUBCMD_LEN);
            }

            if(pdLen > MSG_LEN_MIN + CMD_LEN + SUBCMD_LEN) {
                sp->dataLen = pdLen - (MSG_LEN_MIN + CMD_LEN + SUBCMD_LEN); //TODO: Recheck
                sp->data    = calloc(sp->dataLen, sizeof(char));    // Allocate buffer

                // Store the data
                memcpy(sp->data, dataStart, sp->dataLen);
            }

            return sp;

        } else {
            printf("message length less than minimum: %d\n", MSG_LEN_MIN);
            // Default or error
            return NULL;
        }
    }
}


void parseMessages(int bytesRead, char * buffer) {
    u_int   headerLen   = 15; //TODO: Magic number here
    u_int   maxRxLength = cpt[MENU_MSG_INDEX].length_rx_max;

    char   *tokStart    = NULL;
    char   *firstToken  = NULL;
    char   *nextToken   = NULL;
    char   *dataStart   = NULL;
    char    dataBuffer[maxRxLength]; // Buffer for "data" (everything past the "header")


    // Set null bytes in the data buffer
    memset(dataBuffer, '\0', maxRxLength);

    if(bytesRead > 1) {
        // Find the next "header" if there are multiple in the message
        tokStart = memchr(buffer, 'G', bytesRead); //Set first 'G' we see at [0]
        firstToken = strstr(tokStart, SOCKET_TOK); //Check for the token

        // No token found at first 'G'
        if(firstToken != NULL) {
            tokStart  = memchr(tokStart + 1, 'G', buffer - tokStart);

            if(tokStart) {
                nextToken = strstr(tokStart, SOCKET_TOK);
            }

            // If there is only one token, copy everything
            if(NULL == nextToken) {
                // Inspect message here
                //printf("Inspecting message here: D\n\n");
                //inspectMessage(firstToken, bytesRead);

                strncpy(dataBuffer, firstToken + headerLen, (int)bytesRead - headerLen);
            } else { // There are multiple headers
                int nxtTkn = nextToken - buffer;
                // Inspect message here
                //printf("Inspecting message here: E\n\n");
                //inspectMessage(firstToken, bytesRead);

                strncpy(dataBuffer, firstToken + headerLen, (nxtTkn - 1) - headerLen);

                //printf("Next token at %d\n", nxtTkn);

                while(NULL != nextToken) {
                    // Store the strating point of this chunk
                    char* thisToken = nextToken;
                    u_int dataLen = 0;

                    // See if there is another header beyond this one
                    if(thisToken + 1 < buffer + bytesRead) {
                        tokStart = memchr(thisToken, 'G', bytesRead - (thisToken - buffer));

                        if(tokStart != NULL) {
                            nextToken =  strstr(thisToken + 1, SOCKET_TOK);
                        }
                    } else {
                        nextToken = NULL;
                    }

                    if(NULL == nextToken) {
                        //From thisToken to bytesRead
                        dataLen = bytesRead - (thisToken - buffer);
                    } else {
                        //From thisToken to nextToken
                        dataLen = thisToken - nextToken;
                    }

                    // Inspect message here
                    //printf("Inspecting message here: F\n\n");
                    //inspectMessage(thisToken, dataLen);
                }
            }
        } else {
            printf("No token in message\n\n");
        }

        //printf("%s", dataBuffer);

        free(buffer);
    }
}
