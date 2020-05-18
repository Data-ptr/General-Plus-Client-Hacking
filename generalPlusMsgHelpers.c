/*
 * Jane Hacker (2020) - janehacker.com
 * Generalplus camera chip network commands hacking, using MountDog Action cam,
 *  should also apply to the SQXX (SQ8, SQ10, etc.) cube cams, and more...
 */


int findAllTokens(const char* pd, size_t pdLen, char** gArray) {
    u_int tokStartCount = 0;
    u_int tokCount      = 0;

    tokStartCount   = countTokStart(pd, pdLen);
    gArray          = calloc(sizeof(char*), tokStartCount);
    tokCount        = getTokStart(pd, pdLen, gArray, tokStartCount);

    // Divisions are the end of the header (after the token) and the next
    //  elements (address - 1) if its the end of the list, use pdLen
    if(tokCount > 1) {
        for(int i = 0; i < tokCount - 1; i++) {
            char*       startChunk  = gArray[i] + 14;
            const char* endChunk    = (i == tokCount - 1 ? (pd + pdLen) : gArray[i + 1] - 1);
            int         chunkLen    = endChunk - startChunk;
            char        outBuffer[chunkLen + 1];

            memset(outBuffer, '\0', chunkLen + 1);

            strncpy(outBuffer, startChunk, chunkLen);

            printf("%s", outBuffer);
        }
    }

    return tokCount;
}


int countTokStart(const char* firstPtr, u_int len) {
    const char* startPtr  = firstPtr;
    const char* currPtr   = startPtr;

    u_int       numGs     = 0;
    u_int       ptrOffset = len - (startPtr - currPtr);

    // First 'G'
    currPtr = memchr(currPtr, TOK_START, len);

    // Count all the 'G's
    while(NULL != currPtr) {
        numGs++;
        ptrOffset   = len - (startPtr - currPtr);
        currPtr     = memchr(currPtr + 1, TOK_START, ptrOffset);
    }

    return numGs;
}


int getTokStart(const char * firstPtr, u_int bufferLen, char** gArray, u_int gCount) {
    const char* startPtr  = firstPtr;
    const char* currPtr   = startPtr;

    u_int       numTok    = 0;
    u_int       ptrOffset = bufferLen - (startPtr - currPtr);

    // First 'G'
    currPtr = memchr(currPtr, TOK_START, bufferLen);

    // Store all the 'G's
    while(NULL != currPtr) {
        if(0 == memcmp(currPtr, SOCKET_TOK, TOK_LEN)) {
            gArray[numTok] = currPtr; //Store address
            numTok++;
        }

        ptrOffset   = bufferLen - (startPtr - currPtr);
        currPtr     = memchr(currPtr + 1, TOK_START, ptrOffset);
    }

/*
    for(int i = 0; i < numTok; i++) {
        printf("Index: %d,\tAddr: %x,\tToken: %s\n", i, (int)gArray[i], gArray[i]);
    }
*/

    return numTok;
}


u_int appendBytes(char* outBuffer, const char* bytesIn, u_int byteLen) {
    memcpy(outBuffer, bytesIn, byteLen);

    return byteLen; //TODO: Check and or return bytes copied from memcpy
}


int checkBytes(char* buffer, msgSects msgSect, char* bytesStart) {
    int     rv              = -1;
    u_int   bufferLength    = 0;
    u_int   byteArrayLen    = 0;
    char*   byteArray;


    switch(msgSect) {
        case TOKEN:
            bufferLength    = TOK_LEN;
            byteArrayLen    = NUM_TOKS;
            byteArray       = (char*)&tokBytes;
        break;
        case XMIT:
            bufferLength    = XMIT_LEN;
            byteArrayLen    = NUM_XMITS;
            byteArray       = (char*)&xmitBytes;
        break;
        case COMMAND:
            bufferLength    = CMD_LEN;
            byteArrayLen    = NUM_CMDS;
            byteArray       = (char*)&cmdBytes;
        break;
        case SUBCOMMAND:
            bufferLength    = SUBCMD_LEN;
            byteArrayLen    = NUM_SUBCMDS;
            byteArray       = (char*)&subCmdBytes;
        break;
        case DATA:
            // TODO: what are you doing here?
        default:
            bufferLength = 2;
        break;
    }

    memcpy(buffer, bytesStart, bufferLength);      // Load buffer

    for(int i = 1; i < byteArrayLen; i++) {
        if(0 == memcmp(buffer, byteArray + (i * bufferLength), bufferLength)) {
            rv = i;
            break;
        }
    }

    return rv;
}


int msgLoop(int sock, Message msg, u_int bufferOutSize) {
    u_int   endData             = 0;
    ssize_t bytesWritten        = 0;
    ssize_t msgOutLen           = 0;

    char    bufferOut[bufferOutSize];

    // null byte out buffer
    memset(bufferOut, '\0', bufferOutSize);

    // Build the message from the message struct above
    msgOutLen       = constructMessage(msg, bufferOut);

    // Write message to camera
    bytesWritten    = writeMessage(sock, bufferOut, msgOutLen);

    if(bytesWritten > 0) {
        printf("Inspecting message here: C\n\n");
        inspectMessage(bufferOut, msgOutLen);
    } else {
        printf("\n\nNo bytes written!\n\n");
        return EXIT_FAILURE;
    }

    // Main loop
	return loop(sock); //return EXIT_SUCCESS;
}


int loop(int sock) {
    u_int   i           = 0;
    u_int   looping     = 1;
    u_int   endData     = 0;
    u_int   maxRxLength = cpt[MENU_MSG_INDEX].length_rx_max;
    //ssize_t bytesRead  = 0;
    int     bytesRead   = 0;

    while(looping) {
        char *buffer = (char*)calloc(maxRxLength, sizeof(char));

        i++;

        // Read in a message
        bytesRead = readMessage(sock, buffer, maxRxLength, &endData);

        printf("\n\nBytes read: %d\n\n", bytesRead);

        if(endData) {
            break;
        }

        parseMessages(bytesRead, buffer);
    }

    close(sock);

    return EXIT_SUCCESS;
}
