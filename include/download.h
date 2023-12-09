#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FTP_PORT 21 
#define BUFFER_SIZE 1024
#define DATA_BUFFER_SIZE 4096

// Struct to store URL parts
typedef struct URLParts {
    char host[256];
    char username[256];
    char password[256];
    char path[256];
    char ip[INET6_ADDRSTRLEN];
} URLParts;
