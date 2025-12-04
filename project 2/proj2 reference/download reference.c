#include "download.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf(
            "Incorrect number of arguments.\n"
            "Usage authenticated mode: ./download ftp://[<user>:<password>@]<host>/<url-path>\n"
            "Usage anonymous mode: ./download ftp://<host>/<url-path>\n"
            );
        return 1;
    }

    // inicializar as variáveis que vão guardar os campos do url
    char username [URL_COMPONENT_MAX_LENGTH];
    char password [URL_COMPONENT_MAX_LENGTH];
    char hostname [URL_COMPONENT_MAX_LENGTH];
    char file_path [URL_COMPONENT_MAX_LENGTH];
    char file_name [URL_COMPONENT_MAX_LENGTH];
    int port;

    // extrair os campos do url
    if (get_username_password_hostname_file_path_file_name_from_url(argv[1], username, password, hostname, &port, file_path, file_name) != 0) {
        printf("Error while extracting the url fields from the given url.\n");
        return 1;
    }

    // PRINTS PARA DEBUG
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    printf("Hostname: %s\n", hostname);
    printf("Port: %d\n", port);
    printf("File path: %s\n", file_path);
    printf("File name: %s\n", file_name);


    // obter o IP do hostname
    char ip_address_string[URL_COMPONENT_MAX_LENGTH];
    if (get_IP_address_from_hostname(hostname, ip_address_string) != 0) {
        printf("Error while getting the IP address from the hostname.\n");
        return 1;
    }

    // PRINT PARA DEBUG
    printf("IP address: %s\n", ip_address_string);

    // cria conexão de controlo ao criar um socket ligado ao IP do host 
    int control_socket_fd;
    if (create_socket_and_connect(ip_address_string, port, &control_socket_fd) != 0) {
        printf("Error while creating control connection.\n");
        return 1;
    }

    // trata da autenticação com o servidor ao passar o username e password
    if (authenticate_user(control_socket_fd, username, password) != 0) {
        printf("Error while authenticating the user.\n");
        close(control_socket_fd);
        return 1;
    }

    // enviar o código passv para pôr o servidor em modo passivo
    int data_port;
    if (make_server_go_into_passive_mode(control_socket_fd, ip_address_string, &data_port) != 0) {
        printf("Error while sending passv to the server so it goes into passive mode.\n");
        close(control_socket_fd);
        return 1;
    }

    // cria a conexão de dados ao criar um outro socket ligado ao IP do host
    int data_socket_fd;
    if (create_socket_and_connect(ip_address_string, data_port, &data_socket_fd) != 0) {
        printf("Error while creating data connection.\n");
        close(control_socket_fd);
        return 1;
    }

    // envia o commando retr que indica ao servidor que queremos receber um ficheiro e indica qual é o ficheiro
    if (send_RETR_command_to_specify_file_path(control_socket_fd, file_path) != 0) {
        printf("Error while sending RETR command, telling which file we want, by specifying the file path.\n");
        close(control_socket_fd);
        close(data_socket_fd);
        return 1;
    }

    // recebe o ficheiro
    if (receive_and_save_file(data_socket_fd, file_name) != 0) {
        printf("Error while receiving and saving file.\n");
        close(control_socket_fd);
        close(data_socket_fd);
        return 1;
    }

    // fecha as conexões com o control socket e data socket (ao fazer os dois sockets mandarem o comando quit)
    close(control_socket_fd);
    close(data_socket_fd);

    return 0;
}


// função template dada dentro do getip.c adaptada
int get_IP_address_from_hostname(char *hostname, char *ip_address_string) {
    struct hostent *h;
/**
 * The struct hostent (host entry) with its terms documented

    struct hostent {
        char *h_name;    // Official name of the host.
        char **h_aliases;    // A NULL-terminated array of alternate names for the host.
        int h_addrtype;    // The type of address being returned; usually AF_INET.
        int h_length;    // The length of the address in bytes.
        char **h_addr_list;    // A zero-terminated array of network addresses for the host.
        // Host addresses are in Network Byte Order.
    };

    #define h_addr h_addr_list[0]	The first address in h_addr_list.
*/
    if ((h = gethostbyname(hostname)) == NULL) {
        printf("Error while doing gethostbyname()\n");
        return -1;
    }

    // printf("Host name  : %s\n", h->h_name);
    // printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));
    strcpy(ip_address_string, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}


// função template dada dentro do clientTCP.c adaptada
int establish_TCP_connection_using_sockets_to_a_server_and_send_a_string(int server_port, char *server_address_string, char *message_to_send) {
    int sockfd;
    struct sockaddr_in server_addr;
    // char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n"; // isto era só a de teste no código original
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_address_string);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(server_port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error while doing socket()\n");
        return 1;
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        printf("Error while doing connect()");
        return 1;
    }
    /*send a string to the server*/
    bytes = write(sockfd, message_to_send, strlen(message_to_send));
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        printf("Error while doing write()\n");
        return 1;
    }

    if (close(sockfd)<0) {
        printf("Error while doing close()");
        return 1;
    }
    return 0;
}


int get_username_password_hostname_file_path_file_name_from_url(char *url_received, char *username, char *password, char *hostname, int *port, char *file_path, char *file_name) {
    // pôr o username e a password para os valores default caso o user não os tenha especificado o que significa que é para estar em modo anónimo
    strcpy(username, DEFAULT_USERNAME);
    strcpy(password, DEFAULT_PASSWORD);
    *port = 21;
    // regex para extrair os campos do url
    regex_t regex;
    regmatch_t matches[9]; // stores the position of the matches
    // match 0 é tudo
    // ^ftp://
    // (                  // username e password são opcionais // match 1 que tem o user e a password
    //     ([^:@]*)       // username (não pode ter ':' ou '@') // match 2
    //     (              // password é opcional
    //         :          // ':' para separar o username e a password // match 3 que tem : e a password
    //         ([^@]*)    // password (não pode ter '@') // match 4
    //     )?
    //     @              // '@' para o username e a password do hostname
    //     )?
    // )?
    // ([^:/]+)           // hostname (não pode ter ':' ou '/') // match 5
    // (                  // port é opcional (acho que nem é preciso fazer mas pronto)
    // :                  // ':' para separar o hostname e o port // match 6 que tem : e o port
    // ([0-9]+)           // port (número de 0 a 9) // match 7
    // )?
    // (/.*)?             // file path que começa com '/' e é opcional // match 8

    const char *pattern = "^ftp://(([^:@]*)(:([^@]*))?@)?([^:/]+)(:([0-9]+))?(/.*)?$";

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        printf("Failed to compile regex.\n");
        return 1;
    }

    if (regexec(&regex, url_received, 9, matches, 0) != 0) {
        printf("Invalid URL format.\n");
        regfree(&regex);
        return 1;
    }

    // se o user especificou o username então extrair e guardar
    if (matches[2].rm_so != -1 && matches[2].rm_eo > matches[2].rm_so) {
        strncpy(username, url_received + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
        username[matches[2].rm_eo - matches[2].rm_so] = '\0';
    }

    // se o user especificou a password então extrair e guardar
    if (matches[4].rm_so != -1 && matches[4].rm_eo > matches[4].rm_so) {
        strncpy(password, url_received + matches[4].rm_so, matches[4].rm_eo - matches[4].rm_so);
        password[matches[4].rm_eo - matches[4].rm_so] = '\0';
    }

    // extrair hostname
    if (matches[5].rm_so != -1 && matches[5].rm_eo > matches[5].rm_so) {
        int len = matches[5].rm_eo - matches[5].rm_so;
        strncpy(hostname, url_received + matches[5].rm_so, len);
        hostname[len] = '\0';
    } else {
        printf("Hostname is missing in the URL.\n");
        regfree(&regex);
        return 1;
    }

    // se o user especificou o port então extrair port e guardar, embora pronto nos slides do professor isto não aparecia, só aparece o host e pronto o port é sempre o 21 pelo o que percebo, mas fica aqui só para o caso
    if (matches[7].rm_so != -1 && matches[7].rm_eo > matches[7].rm_so) {
        char port_str[6];
        int len = matches[7].rm_eo - matches[7].rm_so;
        if (len >= sizeof(port_str)) {
            len = sizeof(port_str) - 1;
        }
        strncpy(port_str, url_received + matches[7].rm_so, len);
        port_str[len] = '\0';
        *port = atoi(port_str);
    }

    // extrair file path (se não tiver então é o root)
    if (matches[8].rm_so != -1 && matches[8].rm_eo > matches[8].rm_so) {
        strncpy(file_path, url_received + matches[8].rm_so, matches[8].rm_eo - matches[8].rm_so);
        file_path[matches[8].rm_eo - matches[8].rm_so] = '\0';
    } else {
        strcpy(file_path, "/");
    }

    // extrair file name
    char *last_slash = strrchr(file_path, '/'); // procura o último / que vai ser o que está antes do nome do ficheiro
    if (last_slash != NULL && *(last_slash + 1) != '\0') {
        strcpy(file_name, last_slash + 1);
    } else {
        strcpy(file_name, file_path); // se não houver nome então pôr o nome o caminho do ficheiro? deviamos se calhar por algum default ou assim?
    }

    regfree(&regex);
    return 0;
}

int create_socket_and_connect(char *ip_address, int port, int *socket_fd) {
    struct sockaddr_in server_addr;

    // server address handling
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address); /* 32 bit Internet address network byte ordered */
    server_addr.sin_port = htons(port); /* Server TCP port must be network byte ordered */

    // open a TCP socket
    if ((*socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error while doing socket()\n");
        return -1;
    }

    // connect to the server
    if (connect(*socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("Error while connect()\n");
        close(*socket_fd);
        return -1;
    }

    return 0;
}


int authenticate_user(int socket_fd, char *username, char *password) {
    char buffer[MAX_RECEIVED_RESPONSE_FROM_SERVER_BUFFER_SIZE];
    int number_of_bytes;

    // ler resposta do server inicial
    number_of_bytes = read_server_reply(socket_fd, buffer, sizeof(buffer));
    if (number_of_bytes < 0) {
        printf("Error while receiving server response\n");
        return -1;
    }
    printf("Response before sending the USER command: %s", buffer);

    // enviar o comando USER, onde se especifica o username
    snprintf(buffer, sizeof(buffer), "USER %s\r\n", username);
    number_of_bytes = send(socket_fd, buffer, strlen(buffer), 0);
    if (number_of_bytes < 0) {
        printf("Error while doing send() to send the USER command\n");
        return -1;
    }

    // ler a resposta ao comando USER
    number_of_bytes = read_server_reply(socket_fd, buffer, sizeof(buffer));
    if (number_of_bytes < 0) {
        printf("Error while receiving USER response\n");
        return -1;
    }
    printf("Response to USER command: %s", buffer);

    // enviar o comando PASS, onde se especifica a password
    snprintf(buffer, sizeof(buffer), "PASS %s\r\n", password);
    number_of_bytes = send(socket_fd, buffer, strlen(buffer), 0);
    if (number_of_bytes < 0) {
        printf("Error while doing send() to send the PASS command\n");
        return -1;
    }

    // receber a resposta ao comando PASS
    number_of_bytes = read_server_reply(socket_fd, buffer, sizeof(buffer));
    if (number_of_bytes < 0) {
        printf("Error while receiving PASS response\n");
        return -1;
    }
    printf("Response to PASS command: %s", buffer);

    return 0;
}


int make_server_go_into_passive_mode(int socket_fd, char *ip_address, int *port) {
    char buffer[MAX_RECEIVED_RESPONSE_FROM_SERVER_BUFFER_SIZE];
    int number_of_bytes;
    // criar estas variáveis para guardar o ip e a porta do servidor da resposta ao comando PASV
    int ip1, ip2, ip3, ip4, p1, p2;

    // enviar o comando PASV
    snprintf(buffer, sizeof(buffer), "PASV\r\n");
    number_of_bytes = send(socket_fd, buffer, strlen(buffer), 0);
    if (number_of_bytes < 0) {
        printf("Error while doing send()\n");
        return -1;
    }

    // receber resposta do comando PASV
    number_of_bytes = read_server_reply(socket_fd, buffer, sizeof(buffer));
    if (number_of_bytes < 0) {
        printf("Error while receiving PASV response\n");
        return -1;
    }
    printf("Response to PASV command: %s", buffer);

    // extrair o IP e a porta do servidor da resposta do servidor ao comando PASV
    char *ptr = strchr(buffer, '(');
    if (!ptr) {
        printf("Failed to parse PASV response.\n");
        return -1;
    }
    // pôr já nesta forma para depois poder usar diretamente quando for para criar o socket para transferir o ficheiro mesmo
    if (sscanf(ptr + 1, "%d,%d,%d,%d,%d,%d", &ip1, &ip2, &ip3, &ip4, &p1, &p2) != 6) {
        printf("Failed to parse PASV response.\n");
        return -1;
    }

    // criar o ip e a porta
    snprintf(ip_address, URL_COMPONENT_MAX_LENGTH, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    *port = (p1 * 256) + p2;

    return 0;
}


int send_RETR_command_to_specify_file_path(int socket_fd, char *file_path) {
    char buffer[MAX_RECEIVED_RESPONSE_FROM_SERVER_BUFFER_SIZE];
    int number_of_bytes;

    // enviar o comando RETR, onde se especifica o caminho do ficheiro
    snprintf(buffer, sizeof(buffer), "RETR %s\r\n", file_path);
    number_of_bytes = send(socket_fd, buffer, strlen(buffer), 0);
    if (number_of_bytes < 0) {
        printf("Error while doing send()\n");
        return -1;
    }

    // receber resposta do comando RETR
    number_of_bytes = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (number_of_bytes < 0) {
        printf("Error while doing recv()\n");
        return -1;
    }
    buffer[number_of_bytes] = '\0';
    // printf("Response from the server to the RETR command specifying the file path: %s\n", buffer);

    return 0;
}


int receive_and_save_file(int data_socket_fd, char *file_name) {
    char buffer[MAX_RECEIVED_RESPONSE_FROM_SERVER_BUFFER_SIZE];
    int number_of_bytes;
    // abrir o ficheiro para escrita
    FILE *file = fopen(file_name, "wb");
    if (!file) {
        printf("Error while doing fopen()\n");
        return -1;
    }
    // receber o ficheiro (não é preciso usar read_server_reply(), isto já trata bem, pode se fazer diretamente o recv() e passar o buffer no fwrite())
    while ((number_of_bytes = recv(data_socket_fd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, number_of_bytes, file);
    }
    // verificar se houve erro ao receber o ficheiro
    if (number_of_bytes < 0) {
        printf("Error while doing recv()\n");
        fclose(file);
        return -1;
    }
    // fechar o ficheiro
    fclose(file);
    return 0;
}

int read_server_reply(int socket_fd, char *response_buffer, size_t size) {
    int total_bytes = 0;
    char reply_code[4] = {0};
    int is_multiline = 0;

    while (1) {
        int number_of_bytes = recv(socket_fd, response_buffer + total_bytes, size - total_bytes - 1, 0);
        if (number_of_bytes <= 0) {
            printf("Error while doing recv()\n");
            return -1;
        }
        total_bytes += number_of_bytes;
        response_buffer[total_bytes] = '\0';

        char *line_start = response_buffer;
        char *line_end;

        // verificar se temos alguma linha ao verificar se há um fim
        while ((line_end = strstr(line_start, "\r\n")) != NULL) {
            *line_end = '\0'; // temporário

            // ter o código da reply se ainda não o tivermos o que também significa que é a primeira linha que estamos a ler mas pronto
            if (reply_code[0] == '\0') {
                strncpy(reply_code, line_start, 3);
                reply_code[3] = '\0';
                is_multiline = (line_start[3] == '-');
            }

            // quando chegamos ao fim de uma reply com várias linhas ou então se a reply era só de uma linha
            if (!is_multiline || (strncmp(line_start, reply_code, 3) == 0 && line_start[3] == ' ')) {
                *line_end = '\r'; // voltar a pôr o \r
                return total_bytes;
            }

            *line_end = '\r'; // voltar a pôr o \r

            // ir para a próxima linha
            line_start = line_end + 2; // +2 para passar o \r\n
        }

        // verificar se o response buffer está cheio
        if (total_bytes >= size - 1) {
            fprintf(stderr, "Response too large, if you want to receive larger responses you need to change the constant MAX_RECEIVED_RESPONSE_FROM_SERVER_BUFFER_SIZE inside download.h .\n");
            return -1;
        }
    }
}


