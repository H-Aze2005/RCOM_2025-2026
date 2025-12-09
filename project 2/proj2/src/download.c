// #define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

typedef struct URL
{
    char *user;
    char *password;
    char *host;
    char *path;
    char *filename;
    char ip[128];
} URL;

// basic function to open a Socket connection!
int openSocket(char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;
    // char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);          /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int readResponse(int socket, char *buffer, size_t buffer_size)
{
    int sock_dup = dup(socket);
    FILE *fp = fdopen(sock_dup, "r");
    if (fp == NULL)
    {
        perror("fdopen failed");
        return -1;
    }

    int code = 0;

    // Read line by line until we get the final response line
    while (fgets(buffer, buffer_size, fp) != NULL)
    {
        printf("S: %s", buffer); // Debug to show what the server sent

        // Check if line starts with 3 digits followed by a space
        if (sscanf(buffer, "%d ", &code) == 1 && buffer[3] == ' ')
        {
            break; // Found the final response line
        }
    }

    fclose(fp);
    return code;
}

int parsePasvMode(char *response)
{
    int ip[4], p[2];
    // need to skip text until we find the next '('
    char *start = strchr(response, '(');
    if (start == NULL)
        return -1;

    sscanf(start, "(%d,%d,%d,%d,%d,%d)", &ip[0], &ip[1], &ip[2], &ip[3], &p[0], &p[1]);

    // calculate the port (p1 * 256) + p2
    return (p[0] * 256) + p[1];
}

// download will function with 2 sockets, control and data channels!
// first handle Control connection, then Data Connection
int download(struct URL url)
{
    char buffer[1024];  // buffer for server responses
    char command[1024]; // buffer for sending commands
    int data_port;

    // 1. Open control socket (port 21)
    int sockfd_control = openSocket(url.ip, 21);
    printf("> Connected to Control Socket\n");
    if (sockfd_control < 0)
    {
        printf("Error opening socket\n");
        exit(-1);
    }

    // 2. Read Greeting
    if (readResponse(sockfd_control, buffer, sizeof(buffer)) != 220)
    {
        fprintf(stderr, "Error reading FTP greeting\n");
        return -1;
    }

    // 3. Authenicate (USER -> 331 -> PASS -> 230)
    sprintf(command, "USER %s\r\n", url.user);
    write(sockfd_control, command, strlen(command));
    if (readResponse(sockfd_control, buffer, sizeof(buffer)) != 331)
    {
        fprintf(stderr, "Error sending USER\n");
        return -1;
    }

    // 3.2 To see if the USER passes or not
    sprintf(command, "PASS %s\r\n", url.password);
    write(sockfd_control, command, strlen(command));
    if (readResponse(sockfd_control, buffer, sizeof(buffer)) != 230)
    {
        fprintf(stderr, "Error authentication failed\n");
        return -1;
    }

    // 4. Enter passive mode
    sprintf(command, "PASV\r\n");
    write(sockfd_control, command, strlen(command));
    if (readResponse(sockfd_control, buffer, sizeof(buffer)) != 227)
    {
        fprintf(stderr, "Error entering passive mode\n");
        return -1;
    }

    // 5. Parse the data from the 227 response
    data_port = parsePasvMode(buffer);
    printf("> Data Port Calculated: %d\n", data_port);

    // 6. Open DATA Socket (New connection, now using calculated port)
    int sockfd_data = openSocket(url.ip, data_port);
    printf("> Connected to Data Socket\n");

    // 7. Request File (RETR -> 150 or 125) - SENT on Control packet!
    // ensure path starts with '/'

    if (url.path[0] != '/')
    {
        sprintf(command, "RETR /%s\r\n", url.path);
    }
    else
    {
        sprintf(command, "RETR %s\r\n", url.path);
    }
    write(sockfd_control, command, strlen(command));
    int retr_code = readResponse(sockfd_control, buffer, sizeof(buffer));

    // 150 - file status ok; opening data connection
    // 125 - data connection already open; transfer starting
    if (retr_code != 150 && retr_code != 125)
    {
        fprintf(stderr, "Error requesting file (got code %d\n)", retr_code);
        return -1;
    }

    // 8. Transfer the data - Now we read from the DATA socket
    FILE *file = fopen(url.filename, "wb");
    int bytes_read;
    char data_buffer[1024];
    while ((bytes_read = read(sockfd_data, data_buffer, sizeof(data_buffer))) > 0)
    {
        fwrite(data_buffer, 1, bytes_read, file);
    }
    fclose(file);
    close(sockfd_data); // Close the DATA socket, since we dont need it anymore

    // 9. Final Confirmation (Expect 226) - Read from Control Socket
    if (readResponse(sockfd_control, buffer, sizeof(buffer)) != 226)
    {
        fprintf(stderr, "Error finalizing transfer\n");
        return -1;
    }

    // 10. Exit
    sprintf(command, "QUIT\r\n");
    write(sockfd_control, command, strlen(command));
    close(sockfd_control);

    return 0;
}

int getIP(char *hostname, struct URL *url)
{
    struct hostent *h;

    if ((h = gethostbyname(hostname)) == NULL)
    {
        herror("gethostbyname()");
        return -1;
    }

    strcpy(url->ip, inet_ntoa(*((struct in_addr *)h->h_addr)));

    printf("IP Address: %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: download ftp://<user>:<password>@<host>/<url>\n");
        exit(-1);
    }

    struct URL url;

    char *ftp = strtok(argv[1], "://");
    if (strcmp(ftp, "ftp") != 0)
    {
        printf("Usage: download ftp://<user>:<password>@<host>/<url>\n");
        exit(-1);
    }

    char *credentials = strtok(NULL, "/"); // [<user>:<password>@]<host>
    url.path = strtok(NULL, "");           // <url>
    url.user = strtok(credentials, ":");
    url.password = strtok(NULL, "@");

    if (url.password == NULL)
    {
        url.host = url.user;
        url.user = "anonymous";
        url.password = "anonymous";
        // url.host = strtok(credentials, "/");
    }
    else
    {
        url.host = strtok(NULL, "/");
    }

    printf("DEBUG: Host to resolve: '%s'\n", url.host);

    // debug
    //  printf("user: %s\n", url.user);
    //  printf("password: %s\n", url.password);
    //  printf("host: %s\n", url.host);
    //  printf("path: %s\n", url.path);

    if (getIP(url.host, &url) < 0)
    {
        printf("Error getting IP\n");
        exit(-1);
    }

    char *filename = strrchr(url.path, '/');
    if (filename != NULL)
    {
        // path contains '/', extract the filename after the last '/'
        url.filename = filename + 1; // skip the '/' character
    }
    else
    {
        url.filename = url.path;
    }

    // validate that we have a filename
    if (url.filename == NULL || strlen(url.filename) == 0)
    {
        printf("Error: Invalid filename\n");
        exit(-1);
    }

    printf("Filename: %s\n", url.filename);

    if (download(url) < 0)
    {
        printf("Error downloading file\n");
        exit(-1);
    }

    printf("File Downloaded successfully: %s\n", url.filename);

    return 0;
}