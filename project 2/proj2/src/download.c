#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

typedef struct URL {
    char *user;
    char *password;
    char *host;
    char *path;
    char *filename;
    char ip[128];
} URL;

int openSocket(char *ip, int port){
    int sockfd;
    struct sockaddr_in server_addr;
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int serverReply(FILE *readSocket){
    

}

int download(struct URL url){

    int socketA = openSocket(url.ip, 21);
    if(socketA < 0){
        printf("Error opening socket\n");
        exit(-1);
    }

    FILE *readSocket = fdopen(socketA, "r");

    if(serverReply(readSocket) < 0){
        printf("Error reading from socket\n");
        exit(-1);
    }

}

int getIP(char *hostname, struct URL *url) {
    struct hostent *h;

    if ((h = gethostbyname(hostname)) == NULL) {
        herror("gethostbyname()");
        return -1;
    }

    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    printf("IP Address: %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}

int main(int argc, char **argv){
    if(argc != 2){
        printf("Usage: download ftp://<user>:<password>@<host>/<url>\n");
        exit(-1);
    }

    struct URL url;

    char* ftp = strtok(argv[1], "://");
    if(strcmp(ftp, "ftp") != 0){
        printf("Usage: download ftp://<user>:<password>@<host>/<url>\n");
        exit(-1);
    }

    char* credentials = strtok(NULL, "/"); // [<user>:<password>@]<host>
    url.path = strtok(NULL, ""); // <url>
    url.user = strtok(credentials, ":");
    url.password = strtok(NULL, "@");

    if(url.password == NULL){
        url.user = "anonymous";
        url.password = "anonymous";
        url.host = strtok(credentials, "/");
    } else {
        url.host = strtok(NULL, "/");
    }

    //debug
    // printf("user: %s\n", url.user);
    // printf("password: %s\n", url.password);
    // printf("host: %s\n", url.host);
    // printf("path: %s\n", url.path);
    
    if(getIP(url.host, &url) < 0){
        printf("Error getting IP\n");
        exit(-1);
    }

    char *filename = strrchr(url.path, '/');
    if(filename == NULL){
        printf("Error getting filename\n");
        exit(-1);
    }

    url.filename = strtok(filename, "/");

    printf("Filename: %s\n", url.filename);

    if(download(url) < 0){
        printf("Error downloading file\n");
        exit(-1);
    }

    return 0;
}   