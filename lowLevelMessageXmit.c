ssize_t readMessage(int sockd, void *vptr, size_t maxlen, u_int *endData) {
    ssize_t n, rc;
    char    c, *buffer;

    int numFfs  = 0;

    struct pollfd pfd = { sockd, POLLIN, 0 };

    buffer = vptr;

    for ( n = 1; n < maxlen; n++ ) {
        // Check id there is data in socket (poll)
        int pollRet = poll(&pfd, 1, 500);
        int pollIn  = pfd.revents & POLLIN;

        if(1 != pollIn) {
            break;
        }

    	if(1 == (rc = read(sockd, &c, 1))) {
            //printf("Recv byte: #%i\t 0x%2x\n", (int) n, c);

    	    *buffer++ = c;
    	} else if( rc == 0 ) { // If return from read 0
    	    if( n == 1 ) {     // and we are only on the first byte
    		      return 0;    // return a length of 0 (not 1)
    	    } else {           // But, if we aren't on the first byte
    		    break;         // Break out of this loop and return n
            }
    	} else {
    	    if( errno == EINTR ) {
    		      continue;
            }

    	    return -1;
    	}
    }

    if(1 == n)
        *endData = 1;

    *buffer = 0;
    return n;
}


ssize_t writeMessage(int sockd, const void *vptr, size_t n) {
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while ( nleft > 0 ) {
    	if ( (nwritten = write(sockd, buffer, nleft)) <= 0 ) {
    	    if ( errno == EINTR )
    		      nwritten = 0;
    	    else
    		      return -1;
    	}
    	nleft  -= nwritten;
    	buffer += nwritten;
    }

    return n;
}
