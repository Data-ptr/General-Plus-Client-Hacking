struct arguments {
    bool        relay;
    char       *remoteAddress;
    short int   remotePort;
    char       *localAddress;
    short int   localPort;
};

void    inspectArguments(struct arguments *args);

/** Arguments stuff **/
const   char   *argp_program_version        = "Generalplus Client v0.0.1";
const   char   *argp_program_bug_address    = "<janehacker1@gmail.com>";
static  char    doc[]                       = "Jane Hacker (2020) - janehacker.com\nGeneralplus camera chip network commands hacking, using MountDog Action cam,\nshould also apply to the SQXX (SQ8, SQ10, etc.) cube cams, and more....";
static  char    args_doc[]                  = "[FILENAME]...";
static  struct  argp_option options[]       = {
    { "relay",      'r',    0,      0, "Become a relay between the app and the camera"},
    { "rmtaddr",    'h',    "ADDR", 0, "Remote (camera) IP address. *default DEFAULT_ADDR"},
    { "rmtport",    'p',    "PORT", 0, "Remote (camera) port *default DEFAULT_PORT."},
    { "rlyaddr",    'x',    "ADDR", 0, "Local (relay) IP address. *default DEFAULT_ADDR"},
    { "rlyport",    'z',    "PORT", 0, "Local (relay) port *default DEFAULT_PORT."},
    { 0 }
};


void inspectArguments(struct arguments *args) {
    printf("Relay?:\t\t%s\n",       args->relay ? "true" : "false");
    printf("Remote addr:\t%s\n",    args->remoteAddress);
    printf("Remote port:\t%d\n\n",  args->remotePort);
    printf("Remote addr:\t%s\n",    args->localAddress);
    printf("Remote port:\t%d\n\n",  args->localPort);
}


static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    short int           si          = 0;
    struct arguments   *arguments   = state->input;

    switch(key) {
        case 'r':
            printf("Relay mode on...\n");
            arguments->relay = true;
        break;
        case 'h':
            arguments->remoteAddress = arg;
        break;
        case 'p':
            if(NULL != arg) {
                if(MIN_PORT < (si = atoi(arg))) {
                    arguments->remotePort = si;
                } else {
                    printf(
                        "ERROR: '%s' invalid port or port number too low \
(below %d) using default port: %d\n",
                        arg,
                        MIN_PORT,
                        DEFAULT_PORT
                    );
                }
            } else {
                printf(
                    "ERROR: argument missing for -p (port), using default \
port: %d\n",
                    DEFAULT_PORT
                );
            }
        break;
        case 'x':
            arguments->localAddress = arg;
        break;
        case 'z':
        if(NULL != arg) {
            if(MIN_PORT < (si = atoi(arg))) {
                arguments->remotePort = si;
            } else {
                printf(
                    "ERROR: '%s' invalid local port or local port number too \
                    low (below %d) using default port: %d\n",
                    arg,
                    MIN_PORT,
                    DEFAULT_PORT
                );
            }
        } else {
            printf(
                "ERROR: argument missing for -z (rlyport), using default \
port: %d\n",
                DEFAULT_PORT
            );
        }
    break;
        break;
        case ARGP_KEY_ARG:
            return 0;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };
