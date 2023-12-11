#define  _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/select.h>
#include<assert.h>

#include<sys/socket.h>
#include<arpa/inet.h>

#define exit(N) {fflush(stdout); fflush(stderr); _exit(N); }
#define bool int
#define false 0
#define true 1

// Forward Declaration
int Socket(int namespace, int style, int protocol);
void Bind(int sockfd, struct sockaddr *server, socklen_t length);
void Listen(int sockfd, int qlen);
int Accept(int sockfd, struct sockaddr *addr, socklen_t *length_ptr);
ssize_t Recv(int socket, void *buffer, size_t size, int flags);
ssize_t Send(int socket, const void *buffer, size_t size, int flag);
void Connect(int socket, struct sockaddr *addr, socklen_t length);


#define RMAX 4096
#define HMAX 1024
#define BMAX 1024

// Header
static int HSIZE = 0;
static char header[HMAX+1];

// Body
static int BSIZE = 0;
static char body[BMAX+1];

// Request
static int RSIZE = 0;
static char request[RMAX];


// Ping Requests
static const char ping_request[] = "GET /ping HTTP/1.1\r\n\r\n";
static const char ping_body[] = "pong";

// 200 OK Header
static const char OK200[] = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n";

// Echo Request
static const char echo_request[] = "GET /echo HTTP/1.1\r\n";

// Write and Read Requests
static const char write_request[] = "POST /write HTTP/1.1\r\n";
static const char read_request[] = "GET /read HTTP/1.1\r\n";

static int BUFSIZE = 7;
static char buffer[BMAX+1] = "<empty>";

// Error Messages
static const char BR400[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
static const char NF404[] = "HTTP/1.1 404 Not Found\r\n\r\n";

// Stat Requests
static const char stat_request[] = "GET /stats HTTP/1.1\r\n\r\n";

// Variable for Statistics
static int NREQUESTS = 0;
static int NHEADERS = 0;
static int NBODYS = 0;
static int NERRORS = 0;
static int NERROR_BYTES = 0;

static const char stat_body[] = "Requests: %d\nHeader bytes: %d\nBody bytes: %d\nErrors: %d\nError bytes: %d";

// For I/O
#define FDMAX 16
#define PMAX 1024

typedef struct pool{
    int rd[FDMAX];
    int wr_socket[FDMAX];
    int wr_file[FDMAX];
} Pool;

Pool CPOOL; //Client Pool

static int POOLSIZE = 0;
static char pool_buf[PMAX];


static int find_free_idx(int pool[]){
    for (int i = 0; i < FDMAX; i++){
        if (pool[i] == 0){
            return i;
        }
    }
    assert(0);
}

static void send_data(int connfd, char buf[], int size){
    ssize_t amt, total = 0;
    do {
        amt = Send(connfd, buf + total, size - total, 0);
        total += amt;
    } while (total < size);

}

static int get_port(void)
{
    int fd = open("port.txt", O_RDONLY);
    if (fd < 0) {
        perror("Could not open port.txt");
        exit(1);
    }

    char buffer[32];
    int r = read(fd, buffer, sizeof(buffer));
    if (r < 0) {
        perror("Could not read port.txt");
        exit(1);
    }

    return atoi(buffer);
}


// static: only relevant inside this code
static int open_listenfd(int port){
     // Create a socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("Failed to create socket");
        exit(1);
    }

    // Bind the socket to a specific port
    struct sockaddr_in server;
    server.sin_family = AF_INET;    //Specify server host address type
    server.sin_port = htons(port); //Specify server port number and htons(PORT_NUM) generates network encoding for server port number form host encoding
    inet_pton( AF_INET ,"127.0.0.1", &( server.sin_addr)); //Convert dotted decimal string to network byte order

     
    //Error check
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    //Asking kernel to associate the server's socket address with the socket descriptor
    //In this struct we are specifying the socket address for the server process on the local host
    //If -1 is returned there is an error
    Bind(listenfd, (struct sockaddr*)&server, sizeof(server));

    
    //Listen for incoming connections
    //Change active socket to listening socket
    //Queue list is 10 and after 10 can't accept anymore requests
    Listen(listenfd, 10);
       
    return listenfd;
}


void handle_partial_get(int idx){
    int connfd = CPOOL.wr_socket[idx];
    int fd = CPOOL.wr_file[idx];
    ssize_t amt = read(fd, pool_buf, PMAX);

    if (amt < 0){
        perror("read");
        exit(1);
    }

    if (amt == 0){
        close(connfd);
        close(fd);
        CPOOL.wr_socket[idx] = 0;
        return;
    }

    send_data(connfd, pool_buf, amt);
    NBODYS += amt;
   
}

static void send_response(int connfd){
    send_data(connfd, header, HSIZE);
    send_data(connfd, body, BSIZE);

    NHEADERS += HSIZE;
    NBODYS += BSIZE;
    NREQUESTS += 1;
}

static void send_error(int connfd, const char error[]){
    HSIZE = strlen(error);
    memcpy(header, error, HSIZE);
    send_data(connfd, header, HSIZE);
    NERRORS += 1;
    NERROR_BYTES += HSIZE;
    close(connfd);
}


static void handle_ping(int connfd){
    // Prepare header and body
    BSIZE = strlen(ping_body);
    HSIZE =snprintf(header, HMAX, OK200, BSIZE);
    memcpy(body, ping_body, BSIZE);
    send_response(connfd);
    close(connfd);
}

void handle_echo(int connfd){
    // Extract headers
    char *start = strstr(request, "\r\n");
    start += 2;
    char *end = strstr(start, "\r\n\r\n");
    if (end == NULL)
        end = request + BMAX;
    
    if (end - start > BMAX)
        end = request + BMAX;

    *end = '\0';

    BSIZE = strlen(start);
    memcpy(body, start, BSIZE);
    HSIZE = snprintf(header, HMAX, OK200, BSIZE);

    send_response(connfd);
    close(connfd);
}

static void handle_read(int connfd){
    HSIZE = snprintf(header, HMAX, OK200, BUFSIZE);

    BSIZE = BUFSIZE;
    memcpy(body, buffer, BUFSIZE);

    send_response(connfd);
    close(connfd);
}

static void handle_write(int connfd){
    char *body_start = strstr(request, "\r\n\r\n");
    body_start += 4;

    char *start = strstr(request, "Content-Length: ");
    start += 16;
    char *end = strstr(start, "\r\n");
    *end = '\0';

    BUFSIZE = atoi(start);
    BUFSIZE = BUFSIZE > BMAX ? BMAX : BUFSIZE;
    memcpy(buffer, body_start, BUFSIZE);

    handle_read(connfd);

}

static void handle_file(int connfd){
    // Open file
    char *filename = strstr(request, "/");
    assert(filename != NULL);
    filename += 1;
    filename = strtok(filename, " ");
    int fd = open(filename, O_RDONLY, 0);
    if(fd < 0){
        send_error(connfd, NF404);
        close(connfd);
        return;
    }

    // Determine file size
    struct stat buf;
    if(fstat(fd, &buf) < 0){
        perror("fstat");
        exit(1);
    }

    // Send header
    const int fsize = buf.st_size;
    HSIZE = snprintf(header, HMAX, OK200, fsize);
    send_data(connfd, header, HSIZE);
    NHEADERS += HSIZE;

    // Add to wr_pool
    int idx = find_free_idx(CPOOL.wr_socket);
    CPOOL.wr_socket[idx] = connfd;
    CPOOL.wr_file[idx] = fd;
    NREQUESTS += 1;
}

static void handle_stat(int connfd){
    BSIZE = snprintf(body, BMAX, stat_body, NREQUESTS, NHEADERS, NBODYS, NERRORS, NERROR_BYTES);

    HSIZE = snprintf(header, HMAX, OK200, BSIZE);
    
    send_response(connfd);
    close(connfd);
}

void handle_client(int connfd){

    memset(&request, 0, sizeof(request));
    ssize_t amt =Recv(connfd, request, RMAX-1, 0);
    //request[amt] = '\0';

    if (amt == 0)
        return;

    // Process the request
    if (!strncmp(request, ping_request, strlen(ping_request))){
        handle_ping(connfd);
    } else if (!strncmp(request, echo_request, strlen(echo_request))){
        handle_echo(connfd);
    } else if (!strncmp(request, write_request, strlen(write_request))){
        handle_write(connfd);
    } else if (!strncmp(request, read_request, strlen(read_request))){
        handle_read(connfd);
    }else if (!strncmp(request, stat_request, strlen(stat_request))){
        handle_stat(connfd);
    }else if (!strncmp(request, "GET ", 4)){
        handle_file(connfd);
    }else{
        send_error(connfd, BR400);
    } 
}

static int accept_client(int listenfd){
    static struct sockaddr_in client;
    static socklen_t csize = sizeof(client);
  

    fd_set readfds, writefds;
    int connfd = -1;
    while (connfd < 0){

        // Initialize the fd_set
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(listenfd, &readfds);

        // Add to readfds and writefds
        for (int i = 0; i < FDMAX; i++){
            if (CPOOL.rd[i] > 0){
                FD_SET(CPOOL.rd[i], &readfds);
            }

            if (CPOOL.wr_socket[i] > 0){
                FD_SET(CPOOL.wr_socket[i], &writefds);
            }
        }

        int nready = select(FD_SETSIZE, &readfds, &writefds, NULL, NULL);
        if (nready < 0) {
            perror("select");
            exit(1);
        }

        // Check if there is a connection request
        if (FD_ISSET(listenfd, &readfds)) {
            int sockfd =  Accept(listenfd, (struct sockaddr*)&client, &csize);
            int idx = find_free_idx(CPOOL.rd);
            CPOOL.rd[idx] = sockfd;
            continue;
        }

        // Check sockets in read/write pool
        for (int i = 0; i < FDMAX; i++){
            if (CPOOL.rd[i] > 0 && FD_ISSET(CPOOL.rd[i], &readfds)){
                handle_client(CPOOL.rd[i]);
                CPOOL.rd[i] = 0;
            }

            if (CPOOL.wr_socket[i] && FD_ISSET(CPOOL.wr_socket[i], &writefds)){
                handle_partial_get(i);
            }
        }
    }
    return connfd;

}



int main(int argc, char** argv)
{
    int port = get_port();

    printf("Using port %d\n", port);
    printf("PID: %d\n", getpid());

    // Creating Socket
    int listenfd = open_listenfd(port);

    
    while (1) {
        
        int connfd = accept_client(listenfd);
        handle_client(connfd);
        close(connfd);
    }  

    return 0;
}


int Socket(int namespace, int style, int protocol){
    int sockfd = socket(namespace, style, protocol);

    if (sockfd < 0){
        perror("socket");
        exit(1);
    }
    return sockfd;
}

void Bind(int sockfd, struct sockaddr *server, socklen_t length){
    if (bind(sockfd, server, length) <0){
        perror("bind");
        exit(1);
    }
}

void Listen(int sockfd, int qlen){
    if (listen(sockfd, qlen)< 0){
        perror("listen");
        exit(1);
    }
}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *length_ptr){
    int newfd = accept(sockfd, addr, length_ptr);
    if (newfd < 0){
        perror("accept");
        exit(1);
    }
    return newfd;
}

ssize_t Recv(int socket, void *buffer, size_t size, int flags){
    ssize_t ret_size = recv(socket, buffer, size, flags);
    if (ret_size < 0){
        perror("recv");
        exit(1);
    }
    return ret_size;
}

ssize_t Send(int socket, const void *buffer, size_t size, int flags){
    ssize_t ret_size = send(socket, buffer, size, flags);
    if (ret_size < 0){
        perror("send");
        exit(1);
    }
    return ret_size;
}