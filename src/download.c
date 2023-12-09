#include "../include/download.h"

int parse_url(const char *url, URLParts *url_parts) {
  char temp_url[1024];
    char *temp_ptr;

    // Initialize the URL parts
    memset(url_parts, 0, sizeof(URLParts));

    // Copy the URL to a temporary buffer
    strncpy(temp_url, url, sizeof(temp_url));
    temp_url[sizeof(temp_url) - 1] = '\0';

    // Skip the protocol
    char *protocol_end = strstr(temp_url, "://");
    if (protocol_end) {
        temp_ptr = protocol_end + 3; // Move past the "://"
    } else {
        temp_ptr = temp_url; // No protocol specified
    }

    // Extract the username and password if they are present
    char *userinfo = strstr(temp_ptr, "@");
    if (userinfo) {
        *userinfo = '\0'; // Split the string at the '@'
        char *colon = strstr(temp_ptr, ":");
        if (colon) {
            *colon = '\0'; // Split the string at the ':'
            strncpy(url_parts->username, temp_ptr, sizeof(url_parts->username) - 1);
            strncpy(url_parts->password, colon + 1, sizeof(url_parts->password) - 1);
        } else {
            // No colon found, entire part is the username
            strncpy(url_parts->username, temp_ptr, sizeof(url_parts->username) - 1);
        }
        temp_ptr = userinfo + 1; // Move past the '@'
    }

    // Extract the host and path
    char *path = strstr(temp_ptr, "/");
    if (path) {
        *path = '\0'; // Split the string at the '/'
        strncpy(url_parts->host, temp_ptr, sizeof(url_parts->host) - 1);
        strncpy(url_parts->path, path + 1, sizeof(url_parts->path) - 1);
    } else {
        // No path found, entire part is the host
        strncpy(url_parts->host, temp_ptr, sizeof(url_parts->host) - 1);
    }

    // Resolve the hostname to an IP address
    struct addrinfo hints, *res;
    int status;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(url_parts->host, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    void *addr;
    if (res->ai_family == AF_INET) { // IPv4
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        addr = &(ipv4->sin_addr);
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
        addr = &(ipv6->sin6_addr);
    }

    // Convert the IP to a string and store it
    inet_ntop(res->ai_family, addr, url_parts->ip, sizeof(url_parts->ip));

    freeaddrinfo(res); // Free the linked list

    return 0;
}

int create_socket_and_connect(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Create a socket (IPv4, TCP)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        return -1;
    }

    // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        return -1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        close(sockfd);
        return -1;
    }

    printf("Connected to %s on port %d\n", ip, port);

    return sockfd; // Return the socket file descriptor
}

int send_ftp_command(int sockfd, const char *cmd, char *response, size_t resp_size) {
    char buffer[BUFFER_SIZE];
    int bytes_sent, bytes_received;

    // Send the command
    bytes_sent = send(sockfd, cmd, strlen(cmd), 0);
    if (bytes_sent < 0) {
        perror("Error sending command to the server");
        return -1;
    }

    // Read the server's response
    bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("Error receiving response from server");
        return -1;
    }

    // Null-terminate the response and copy it to the provided buffer
    buffer[bytes_received] = '\0';
    if (response != NULL && resp_size > 0) {
        strncpy(response, buffer, resp_size - 1);
        response[resp_size - 1] = '\0';
    }

    return bytes_received;
}

// Function to login to the FTP server
int ftp_login(int sockfd, const char *username, const char *password) {
    char response[BUFFER_SIZE];

    // Send username
    char user_cmd[BUFFER_SIZE];
    snprintf(user_cmd, BUFFER_SIZE, "USER %s\r\n", username);
    if (send_ftp_command(sockfd, user_cmd, response, sizeof(response)) < 0) {
        return -1;
    }
    printf("USER command response: %s", response);

    // Send password
    char pass_cmd[BUFFER_SIZE];
    snprintf(pass_cmd, BUFFER_SIZE, "PASS %s\r\n", password);
    if (send_ftp_command(sockfd, pass_cmd, response, sizeof(response)) < 0) {
        return -1;
    }
    printf("PASS command response: %s", response);

    return 0; // Return 0 on success
}

int enter_passive_mode(int sockfd, char *data_ip, int *data_port) {
    char buffer[BUFFER_SIZE];
    char *response;
    int count, ip_part1, ip_part2, ip_part3, ip_part4, port_part1, port_part2;

    // Send the PASV command to the server
    char *pasv_cmd = "PASV\r\n";
    if (send(sockfd, pasv_cmd, strlen(pasv_cmd), 0) < 0) {
        perror("Error sending PASV command");
        return -1;
    }

    // Read the server's response
    response = buffer;
    if (recv(sockfd, response, BUFFER_SIZE, 0) < 0) {
        perror("Error receiving PASV response");
        return -1;
    }

    printf("PASV command response: %s", response);

    // Parse the response for the IP address and port number
    // The response should be in the format: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
    count = sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
                   &ip_part1, &ip_part2, &ip_part3, &ip_part4, &port_part1, &port_part2);
    if (count != 6) {
        fprintf(stderr, "Error parsing PASV response: %s\n", response);
        return -1;
    }

    // Recreate the IP address in string form
    sprintf(data_ip, "%d.%d.%d.%d", ip_part1, ip_part2, ip_part3, ip_part4);

    // Calculate the port number
    *data_port = (port_part1 * 256) + port_part2;

    return 0; // Success
}

// Function to request a resource
int ftp_retrieve_file(int sockfd, const char *file_path, int data_port, int data_sock) {
    char response[BUFFER_SIZE];
    // char data_ip[INET_ADDRSTRLEN];
    // int data_port;
    // int data_sockfd;
    int bytes_received;
    FILE *file;

    // Send the RETR command to retrieve the file
    char retr_cmd[BUFFER_SIZE];
    snprintf(retr_cmd, BUFFER_SIZE, "RETR %s\r\n", file_path);

    printf("%s\n", retr_cmd);
    
    if (send_ftp_command(sockfd, retr_cmd, response, sizeof(response)) < 0) {
        return -1;
    }
    printf("RETR command response: %s", response);

    // Open a file to save the incoming data
    file = fopen(file_path, "wb");
    if (!file) {
        perror("Failed to open file on local system");
        close(data_sock);
        return -1;
    }

    // Receive the file data over the data connection
    char data_buffer[DATA_BUFFER_SIZE];
    while ((bytes_received = recv(data_sock, data_buffer, DATA_BUFFER_SIZE, 0)) > 0) {
        fwrite(data_buffer, 1, bytes_received, file);
    }

    if (bytes_received < 0) {
        perror("Error receiving file data");
        fclose(file);
        close(data_sock);
        return -1;
    }

    // Close the file and the data socket
    fclose(file);

    printf("File transfer completed successfully.\n");

    // Read the final response from the control connection
    if (recv(sockfd, response, BUFFER_SIZE, 0) < 0) {
        perror("Error reading final response from control connection");
        return -1;
    }
    printf("Final response after file transfer: %s\n", response);

    return 0; // Success
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ftp://user:password@host/path/to/file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    URLParts url_parts;

    if (parse_url(argv[1], &url_parts) == -1) {
        printf("Error parsing URL\n");
        exit(EXIT_FAILURE);
    }

    printf("Username: %s\nPassword: %s\nHost: %s\nPath: %s\nIP: %s\n",
           url_parts.username, url_parts.password, url_parts.host, url_parts.path, url_parts.ip);

    int sock = create_socket_and_connect(url_parts.ip, FTP_PORT);
    if (sock == -1) {
        printf("Error creating and connecting to socket\n");
        exit(EXIT_FAILURE);
    }

    // Read the initial greeting from the server
    char server_greeting[BUFFER_SIZE];
    if (recv(sock, server_greeting, sizeof(server_greeting), 0) == -1) {
        perror("recv");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Server Greeting: %s\n", server_greeting);

    // Now check if the greeting is a proper FTP response
    if (strncmp(server_greeting, "220", 3) != 0) {
        printf("Did not receive proper greeting from FTP server\n");
        close(sock);
        exit(EXIT_FAILURE);
    }


    // Login to the FTP server
    if (ftp_login(sock, url_parts.username, url_parts.password) == -1) {
        printf("Error logging into FTP server\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Enter passive mode and retrieve the data port from the server's response
    int data_port;
    char data_ip[INET_ADDRSTRLEN];
    if (enter_passive_mode(sock, data_ip, &data_port) == -1) {
     printf("Error entering passive mode\n");
     close(sock);
     exit(EXIT_FAILURE);
    }

    printf("Data connection IP: %s\nData connection Port: %d\n", data_ip, data_port);

    int sock_data = create_socket_and_connect(data_ip, data_port);
    if (sock_data == -1) {
        printf("Error creating and connecting to socket\n");
        exit(EXIT_FAILURE);
    }

    // Retrieve the file using RETR command
    // You would need to handle the data connection in ftp_retrieve_file function
    if (ftp_retrieve_file(sock, url_parts.path, data_port, sock_data) == -1) {
        printf("Error retrieving file from FTP server\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Close the command connection
    close(sock);
    close(sock_data);
    
    return 0;
}
