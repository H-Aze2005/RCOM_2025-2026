/**
 * Example code for getting the IP address from hostname.
 * tidy up includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// fim dos headers para o getip.c


// includes from example code for clientTCP.c
// #include <stdio.h>
#include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <stdlib.h>
#include <unistd.h>

#include <string.h>
// fim dos headers para o clientTCP.c

// headers nossos
#include <regex.h>

// constantes declaradas
#define URL_COMPONENT_MAX_LENGTH 500
#define MAX_RECEIVED_RESPONSE_FROM_SERVER_BUFFER_SIZE 1024

#define DEFAULT_USERNAME "anonymous"
#define DEFAULT_PASSWORD "podeserqualquercena" // acho que não interessa o que pões porque o servidor nem vai verificar se puseres o username anonymous, supostamente é para se pôr o email mas o servidor não verifica

// declarar as funções

// funções template que foram só adaptadas
int get_IP_address_from_hostname(char *hostname, char *ip_address_string);
int establish_TCP_connection_using_sockets_to_a_server_and_send_a_string(int server_port, char *server_address_string, char *message_to_send);

// funções que foram criadas por nós

// função principal que chama as outras, recebe como argumento o url do ficheiro que se quer fazer download
// retorna 0 se for bem sucedido, 1 se não for bem sucedido
int main(int argc, char *argv[]);

// extrai do url sempre o hostname, opcionalmente o username e a password, o port e na mesma opcinalmente para o file path e o file name, mas neste caso põe que é /
// retorna 0 se for bem sucedido, 1 se não for bem sucedido, pode fazer modo anónimo automaticamente ao definir o username e a password para o default
int get_username_password_hostname_file_path_file_name_from_url(char *url_received, char *username, char *password, char *hostname, int *port, char *file_path, char *file_name);

// cria um socket e liga-se ao ip e port que são passados como argumentos
// retorna 0 se for bem sucedido, 1 se não for bem sucedido
int create_socket_and_connect(char *ip_address, int port, int *socket_fd);

// autentica o user com o servidor ao passar o username e a password
// retorna 0 se for bem sucedido, 1 se não for bem sucedido
int authenticate_user(int socket_fd, char *username, char *password);

// manda ao servidor o comando PASV e extrai o ip e a porta do servidor da resposta a esse comando
// retorna 0 se for bem sucedido, 1 se não for bem sucedido
int make_server_go_into_passive_mode(int socket_fd, char *ip_address, int *port);

// manda ao servidor o comando RETR para especificar o ficheiro que se quer receber
// retorna 0 se for bem sucedido, 1 se não for bem sucedido
int send_RETR_command_to_specify_file_path(int socket_fd, char *file_path);

// recebe o ficheiro e guarda-o
// retorna 0 se for bem sucedido, 1 se não for bem sucedido
int receive_and_save_file(int data_socket_fd, char *file_name);

// lê a resposta do servidor, que pode ser de várias linhas e guarda-a no buffer
// retorna o número de bytes lidos se for bem sucedido, -1 se não for bem sucedido
int read_server_reply(int socket_fd, char *buffer, size_t size);

