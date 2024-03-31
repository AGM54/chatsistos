#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "sistos.pb-c.h"

#define MAX_CLIENTS 50

typedef struct {
    char username[75];
    char user_ip[20];
    int status;
    int sockfd;
} Client;

Client clients[MAX_CLIENTS];
int cantidad_clientes = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void listado_clientes() {
    printf("Lista de clientes conectados:\n");
    for (int i = 0; i < cantidad_clientes; i++) {
        char status[25] = "Estado desconocido";
        if (clients[i].status == 1) {
            strcpy(status, "En línea");
        } else if (clients[i].status == 2) {
            strcpy(status, "Ocupado");
        } else if (clients[i].status == 3) {
            strcpy(status, "Desconectado");
        }
        printf("- %s (%s) -> estado: (%s)\n", clients[i].username, clients[i].user_ip, status);
    }
}

char *getStatusString(int status) {
    char *result;
    switch (status) {
        case 1:
            result = "En línea";
            break;
        case 2:
            result = "Ocupado";
            break;
        case 3:
            result = "Desconectado";
            break;
        default:
            result = "Estado desconocido";
            break;
    }
    return result;
}

Chat__UserInfo *find_user(char *username) {
    for (int i = 0; i < cantidad_clientes; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            Chat__UserInfo *user_find_response = malloc(sizeof(Chat__UserInfo));
            chat__user_info__init(user_find_response);
            user_find_response->username = strdup(clients[i].username);
            user_find_response->ip = strdup(clients[i].user_ip);
            user_find_response->status = getStatusString(clients[i].status);
            return user_find_response;
        }
    }
    return NULL;
}

void update_user_status(char *username, char *ip, Client cliente) {
    for (int i = 0; i < cantidad_clientes; i++) {
        if (strcmp(clients[i].username, username) == 0 && strcmp(clients[i].user_ip, ip) == 0) {
            clients[i] = cliente;
        }
    }
}

int search_user(char *username, char *ip) {
    for (int i = 0; i < cantidad_clientes; i++) {
        if (strcmp(clients[i].username, username) == 0 && strcmp(clients[i].user_ip, ip) == 0) {
            return i;
        }
    }
    return -1;
}

int remove_users(int index) {
    if (index < 0 || index >= cantidad_clientes) {
        return -1;
    }
    for (int i = index; i < cantidad_clientes - 1; i++) {
        clients[i] = clients[i + 1];
    }
    cantidad_clientes--;
}

int check_users(char *username, char *ip) {
    for (int i = 0; i < cantidad_clientes; i++) {
        if (strcmp(clients[i].username, username) == 0 && strcmp(clients[i].user_ip, ip) == 0) {
            return 1;
        }
    }
    return 0;
}

void ERRORMensaje(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void handle_incoming_message(uint8_t *buffer, size_t len) {
    Chat__MessageCommunication *received_message = chat__message_communication__unpack(NULL, len, buffer);
    if (received_message == NULL) {
        printf("[CLIENT-ERROR]: Failed to unpack message\n");
        return;
    }
    if (received_message->recipient == NULL) {
        printf("[BROADCAST] %s: %s\n", received_message->sender, received_message->message);
    } else {
        printf("[DM] %s: %s\n", received_message->sender, received_message->message);
    }
    chat__message_communication__free_unpacked(received_message, NULL);
}

void *handle_newclient(void *arg) {
    int client_socket = *(int *)arg;

    uint8_t buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, 1024, 0);

    if (bytes_received < 0) {
        perror("[SERVER-ERROR]: Recepción de cliente fallida\n");
        exit(EXIT_FAILURE);
    } else {
        Chat__UserRegistration *new_client = chat__user_registration__unpack(NULL, bytes_received, buffer);
        if (new_client == NULL) {
            perror("[SERVER-ERROR]: Registro de usuario fallido\n");
            exit(EXIT_FAILURE);
        }
        printf("Ingreso usuario: %s | IP: (%s)\n", new_client->username, new_client->ip);
        Client user;
        strcpy(user.username, new_client->username);
        strcpy(user.user_ip, new_client->ip);
        user.status = 1;
        user.sockfd = client_socket;
        Chat__UserInfo user_response;
        chat__user_info__init(&user_response);
        user_response.username = user.username;
        user_response.ip = user.user_ip;
        user_response.status = getStatusString(user.status);
        int user_exist = check_users(new_client->username, new_client->ip);
        if (user_exist == 0) {
            clients[cantidad_clientes] = user;
            cantidad_clientes++;
            Chat__ServerResponse answer;
            chat__server_response__init(&answer);
            answer.option = 1;  // Registration response
            answer.code = 200;  // Success
            answer.servermessage = "Usuario registrado";
            answer.userinforesponse = &user_response;
            printf("[SERVER (200)]->[CLIENT %s]: %s\n", answer.userinforesponse->username, answer.servermessage);
            size_t package_size = chat__server_response__get_packed_size(&answer);
            uint8_t *buffer_envioA = malloc(package_size);
            chat__server_response__pack(&answer, buffer_envioA);
            if (send(client_socket, buffer_envioA, package_size, 0) < 0) {
                perror("[SERVER-ERROR]: Envío de respuesta fallido\n");
                exit(EXIT_FAILURE);
            }
            free(buffer_envioA);
            // Handling client options here...
        } else {
            Chat__ServerResponse answer;
            chat__server_response__init(&answer);
            answer.option = 1;  // Registration response
            answer.code = 400;  // User already registered
            answer.servermessage = "Usuario ya registrado";
            answer.userinforesponse = &user_response;
            printf("[SERVER (400)]->[CLIENT %s]: %s\n", answer.userinforesponse->username, answer.servermessage);
            size_t package_size = chat__server_response__get_packed_size(&answer);
            uint8_t *buffer_envioA = malloc(package_size);
            chat__server_response__pack(&answer, buffer_envioA);
            if (send(client_socket, buffer_envioA, package_size, 0) < 0) {
                perror("[SERVER-ERROR]: Envío de respuesta fallido\n");
                exit(EXIT_FAILURE);
            }
            free(buffer_envioA);
        }
        chat__user_registration__free_unpacked(new_client, NULL);
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    pthread_t thread_id;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        ERRORMensaje("[SERVER-ERROR]: Creación de socket fallida");
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        ERRORMensaje("[SERVER-ERROR]: Bind fallido");
    }

    if (listen(server_socket, 5) < 0) {
        ERRORMensaje("[SERVER-ERROR]: Listen fallido");
    }

    printf("[SERVER]: Esperando conexiones...\n");

    while (1) {
        socklen_t client_address_length = sizeof(client_address);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length)) < 0) {
            ERRORMensaje("[SERVER-ERROR]: Aceptación de conexión fallida");
        }

        pthread_create(&thread_id, NULL, handle_newclient, (void *)&client_socket);
    }

    close(server_socket);
    pthread_mutex_destroy(&mutex);
    return 0;
}
