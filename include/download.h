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

// Function to login to the FTP server
int ftp_login(int sockfd, const char *username, const char *password);

// Create a scoket and connect to it
int create_socket_and_connect(const char *ip, int port);

// Parse the url to a struch
int parse_url(const char *url, URLParts *url_parts);

// Log in FTP
int ftp_login(int sockfd, const char *username, const char *password);

// Enter FTP passive mode 
int enter_passive_mode(int sockfd, char *data_ip, int *data_port);

// Retrieve a file from an FPT server 
int ftp_retrieve_file(int sockfd, const char *file_path, int data_port, int data_sock);