/*
 *  sosd.c (daemon)
 *
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netdb.h>

//#include "sos.h"


#define DEBUG    1


#define USAGE "usage:   $ sosd --port <number> --buffer_len <bytes> --listen_backlog <len> [--work_dir <path>]"
#define DAEMON_NAME    "sosd"
#define DEFAULT_DIR    "/tmp"
#define LOCK_FILE      "sosd.lock"
#define LOG_FILE       "sosd.log"
#define DAEMON_LOG     1
#define GET_TIME(now)  { struct timeval t; gettimeofday(&t, NULL); now = t.tv_sec + t.tv_usec/1000000.0; }
#define dlog(msg)                                                       \
    if (DAEMON_LOG) {                                                   \
        GET_TIME(time_now);                                             \
        fprintf(log_fptr, "[%s:%s @ %f] %s",                            \
                DAEMON_NAME, daemon_pid_str, time_now, msg);            \
        fflush(log_fptr);                                               \
    }


void daemon_init();
void signal_handler(int signal);


/*--- Daemon management variables ---*/
char   *WORK_DIR;
int     daemon_running = 0;
int     lock_fptr;
FILE*   log_fptr;
char    daemon_pid_str[256];
double  time_now = 0.0;

/*--- Networking variables ---*/
int     server_socket_fd;
int     client_socket_fd;
int     port_number;
char   *server_port;
int     buffer_len;
int     listen_backlog;
int     client_len;
struct addrinfo           server_hint;
struct addrinfo          *server_addr;
char                     *client_host;
char                     *client_port;
struct addrinfo          *result;
struct sockaddr_storage   peer_addr;
socklen_t                 peer_addr_len;



int main(int argc, char *argv[]) {
    int i, elem, next_elem, byte_count;
    char   *buffer;

    //SOS_init( argc, argv, SOS_ROLE_DAEMON );

    WORK_DIR = &DEFAULT_DIR;

    /*
     *  TODO:{ CHAD, INIT } Consider adding a --interactive mode that doesn't
     *  run as a daemon, displays statistics in real-time, and allows commands.
     */


    /* Process command-line arguments */
    if ( argc < 7 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }
    port_number    = -1;
    buffer_len     = -1;
    listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "%s\n", USAGE); exit(1); }
        if (      strcmp(argv[elem], "--port"            ) == 0) { server_port    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--buffer_len"      ) == 0) { buffer_len     = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--listen_backlog"  ) == 0) { listen_backlog = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { WORK_DIR       = argv[next_elem];       } /* optional */
        else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    port_number = atoi(server_port);
    if ( (port_number < 1)
         || (buffer_len < 1)
         || (listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    memset(&daemon_pid_str, '\0', 256);

    daemon_init();
    daemon_running = 1;
     

    /* System logging initialize */
    setlogmask(LOG_UPTO(LOG_ERR));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon: %s", DAEMON_NAME);
    if (DAEMON_LOG) { log_fptr = fopen(LOG_FILE, "w"); }
    dlog("Daemon logging is on-line.\n");



    /* ----- DAEMON CODE BEGINS HERE --------------------- */

    buffer = (char *) malloc(sizeof(char) * buffer_len);

    memset(&server_hint, '\0', sizeof(struct addrinfo));
    server_hint.ai_family     = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    server_hint.ai_socktype   = SOCK_STREAM;   /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
    server_hint.ai_flags      = AI_PASSIVE;    /* For wildcard IP addresses */
    server_hint.ai_protocol   = 0;             /* Any protocol */
    server_hint.ai_canonname  = NULL;
    server_hint.ai_addr       = NULL;
    server_hint.ai_next       = NULL;

    i = getaddrinfo(NULL, server_port, &server_hint, &result);
    if (i != 0) { dlog("Error!  getaddrinfo() failed. Exiting daemon.\n"); exit(EXIT_FAILURE); }

    for ( server_addr = result ; server_addr != NULL ; server_addr = server_addr->ai_next ) {
        dlog("Trying an address...\n");
        server_socket_fd = socket(server_addr->ai_family, server_addr->ai_socktype, server_addr->ai_protocol );
        if ( server_socket_fd == -1 ) { dlog("   ...failed to get a socket.\n"); continue; }
        if ( bind( server_socket_fd, server_addr->ai_addr, server_addr->ai_addrlen ) == 0 ) break; /* success */
        close( server_socket_fd );
    }

    dlog("   ...got a socket, and bound to it!\n");
    if ( server_socket_fd == NULL ) { dlog("   ...got a socket but could not bind.\n"); exit(EXIT_FAILURE); }
    freeaddrinfo(result);

    listen( server_socket_fd, listen_backlog );

    dlog("Listening on socket.\n");
    dlog("Entering the main loop.   while(daemon_running) { ... }\n");

    while (daemon_running) { /* ========== */
        memset(buffer, '\0', buffer_len);
        
        peer_addr_len = sizeof(peer_addr);
        client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &peer_addr, &peer_addr_len);

        i = getnameinfo((struct sockaddr *) &peer_addr, peer_addr_len, client_host, NI_MAXHOST, client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i == 0) { dlog("Received connection.\n"); } else { dlog("Error calling getnameinfo() on client connection.\n"); exit(1); }

        byte_count = recvfrom( client_socket_fd, (void *) buffer, buffer_len, NULL, (struct sockaddr *) &peer_addr, &peer_addr_len );
        
        dlog(buffer);


        
        /*
         *  TODO: { CHAD, CACHE, DB } This is where we will add the commit to a
         *        local store, and where we will come in later with sync signal
         *        to dirty the store (perhaps).
         *
         *        For now, just commit the recieved message to the log.
         */
        
        i = sendto( client_socket_fd, (void *) buffer, strlen( buffer ), NULL, NULL, NULL); //(struct sockaddr *) &peer_addr, peer_addr_len);
        if (i == -1) { dlog("Error sending a response.\n"); dlog(strerror(errno)); }

        close( client_socket_fd );
        
    } /* ========== */

    free(buffer);
   
    /* -------------------------------------------------- */


    //SOS_finalize();
  
    //[cleanup]
    dlog("Exiting main() beneath the infinite loop.\n");
    closelog();
    if (DAEMON_LOG) { fclose(log_fptr); }
  
    return(EXIT_SUCCESS);
} //end: main()














    


void daemon_init() {
    pid_t pid, sid;

    /* [fork]
     *     split off from the parent process (& terminate parent)
     */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not fork() off parent process.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent

    sprintf(daemon_pid_str, "%d", getpid());
    fprintf(stderr, "Starting daemon (%s) with PID = %s\n", DAEMON_NAME, daemon_pid_str);

    /* [child session]
     *     create/occupy independent session from parent process
     */
    umask(0);
    sid = setsid();
    if (sid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not acquire a session id.\n", DAEMON_NAME); 
        exit(EXIT_FAILURE);
    }
    if ((chdir(WORK_DIR)) < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not change to working directory: %s\n", \
                DAEMON_NAME, WORK_DIR);
        exit(EXIT_FAILURE);
    }

    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    lock_fptr = open(LOCK_FILE, O_RDWR | O_CREAT, 0640);
    if (lock_fptr < 0) { 
        fprintf(stderr, "\nUnable to start daemon (%s): Could not access lock file %s in directory %s\n", \
                DAEMON_NAME, LOCK_FILE, WORK_DIR);
        exit(EXIT_FAILURE);
    }
    if (lockf(lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nUnable to start daemon (%s): An instance is already running.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    write(lock_fptr, daemon_pid_str, strlen(daemon_pid_str));


    /* [file handles]
     *     close unused IO handles
     */
    #ifndef DEBUG
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    #endif

    /* [signals]
     *     register the signals we care to trap
     */

    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);


    return;
} //end: daemon_init



void signal_handler(int signal) {
    switch (signal) {
    case SIGHUP:
        syslog(LOG_DEBUG, "SIGHUP signal caught.");
        break;

    case SIGTERM:
        /* Future-proofing for non-blocking socket accept calls... */
        daemon_running = 0;

        /* [shutdown]
         *     close logs to write them to disk.
         */
        syslog(LOG_DEBUG, "SIGTERM signal caught.");
        syslog(LOG_INFO, "Shutting down.\n");
        closelog();
        dlog("Caught SIGTERM, shutting down.\n");
        if (DAEMON_LOG) { fclose(log_fptr); }
        exit(EXIT_SUCCESS);
        break;

    }

} //end: signal_handler



/* *********************************************************************** */



    /*
     *  Fancy handlers global vars...

    struct sigaction term_act;
    struct sigaction ill_act;
    struct sigaction abrt_act;
    struct sigaction fpe_act;
    struct sigaction segv_act;
    struct sigaction bus_act;

     *
     */



    /*
     *  Fancy handlers registration...
     *
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = signal_handler;
    sigaction( SIGTERM, &act, &term_act);  
    sigaction( SIGILL, &act, &ill_act);  
    sigaction( SIGABRT, &act, &abrt_act);  
    sigaction( SIGFPE, &act, &fpe_act);  
    sigaction( SIGSEGV, &act, &segv_act);  
    sigaction( SIGBUS, &act, &bus_act);  
    */




    /*
     *  Fancy handler...
     *
     *

    printf("\n");
    printf("\n");
    
    void *trace[32];
    size_t size, i;
    char **strings;
    
    size    = backtrace( trace, 64 );
    // overwrite sigaction with caller's address
    //trace[1] = (void *)ctx.eip;
    strings = backtrace_symbols( trace, size );
    
    printf("\n");
    printf("BACKTRACE:");
    printf("\n");
    printf("\n");
    
    char exe[256];
    int len = readlink("/proc/self/exe", exe, 256);
    if (len != -1) {
        exe[len] = '\0';
    }
    
    for( i = 1; i < size; i++ ){
        printf("%s\n", strings[i]);
        char syscom[1024];
        sprintf(syscom,"addr2line -f -e %s %p", exe, trace[i]);
        system(syscom);
    }
    
    printf("\n");
    printf("***************************************");
    printf("\n");
    printf("\n");
    fflush(stdout);
    exit(99);

    */
    



