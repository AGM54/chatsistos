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

void remove_user(int index) {
    if (index < 0 || index >= cantidad_clientes) {
        return;
    }
    for (int i = index; i < cantidad_clientes - 1; i++) {
        clients[i] = clients[i + 1];
    }
    cantidad_clientes--;
}

void broadcast_message(char *message, char *sender) {
    for (int i = 0; i < cantidad_clientes; i++) {
        if (strcmp(clients[i].username, sender) != 0) {
            Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT;
            msg.message = message;
            msg.sender = sender;
            msg.recipient = NULL;
            size_t packed_size = chat__message_communication__get_packed_size(&msg);
            uint8_t *buffer = malloc(packed_size);
            chat__message_communication__pack(&msg, buffer);
            send(clients[i].sockfd, buffer, packed_size, 0);
            free(buffer);
        }
    }
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;

    uint8_t buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, 1024, 0);

    if (bytes_received < 0) {
        perror("[SERVER-ERROR]: Recepción de cliente fallida\n");
        exit(EXIT_FAILURE);
    } else {
        Chat__ClientPetition *request = chat__client_petition__unpack(NULL, bytes_received, buffer);
        if (request == NULL) {
            perror("[SERVER-ERROR]: Desempaquetado de solicitud de cliente fallido\n");
            exit(EXIT_FAILURE);
        }

        switch (request->option) {
            case 1: { // User registration
                char *username = request->registration->username;
                char *ip = request->registration->ip;

                pthread_mutex_lock(&mutex);
                int user_exist = 0;
                for (int i = 0; i < cantidad_clientes; i++) {
                    if (strcmp(clients[i].username, username) == 0) {
                        user_exist = 1;
                        break;
                    }
                }

                if (!user_exist) {
                    Client new_client;
                    strcpy(new_client.username, username);
                    strcpy(new_client.user_ip, ip);
                    new_client.status = 1;
                    new_client.sockfd = client_socket;
                    clients[cantidad_clientes++] = new_client;

                    Chat__ServerResponse response = CHAT__SERVER_RESPONSE__INIT;
                    response.option = 1;  // Registration response
                    response.code = 200;  // Success
                    response.servermessage = "Usuario registrado";
                    response.userinforesponse = NULL;  // No se necesita desempaquetar aquí

                    size_t packed_size = chat__server_response__get_packed_size(&response);
                    uint8_t *response_buffer = malloc(packed_size);
                    chat__server_response__pack(&response, response_buffer);
                    send(client_socket, response_buffer, packed_size, 0);
                    free(response_buffer);
                } else {
                    Chat__ServerResponse response = CHAT__SERVER_RESPONSE__INIT;
                    response.option = 1;  // Registration response
                    response.code = 400;  // User already registered
                    response.servermessage = "Usuario ya registrado";
                    response.userinforesponse = NULL;  // No se necesita desempaquetar aquí

                    size_t packed_size = chat__server_response__get_packed_size(&response);
                    uint8_t *response_buffer = malloc(packed_size);
                    chat__server_response__pack(&response, response_buffer);
                    send(client_socket, response_buffer, packed_size, 0);
                    free(response_buffer);
                }
                pthread_mutex_unlock(&mutex);
                break;
            }
            case 2: { // Change status
                char *username = request->change->username;
                char *status = request->change->status;

                pthread_mutex_lock(&mutex);
                for (int i = 0; i < cantidad_clientes; i++) {
                    if (strcmp(clients[i].username, username) == 0) {
                        if (strcmp(status, "En línea") == 0) {
                            clients[i].status = 1;
                        } else if (strcmp(status, "Ocupado") == 0) {
                            clients[i].status = 2;
                        } else if (strcmp(status, "Desconectado") == 0) {
                            clients[i].status = 3;
                            remove_user(i);
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex);
                break;
            }
            case 3: { // Message communication
                char *message = request->messagecommunication->message;
                char *sender = request->messagecommunication->sender;
                char *recipient = request->messagecommunication->recipient;

                pthread_mutex_lock(&mutex);
                if (recipient == NULL) {
                    broadcast_message(message, sender);
                } else {
                    for (int i = 0; i < cantidad_clientes; i++) {
                        if (strcmp(clients[i].username, recipient) == 0) {
                            Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT;
                            msg.message = message;
                            msg.sender = sender;
                            msg.recipient = recipient;
                            size_t packed_size = chat__message_communication__get_packed_size(&msg);
                            uint8_t *buffer = malloc(packed_size);
                            chat__message_communication__pack(&msg, buffer);
                            send(clients[i].sockfd, buffer, packed_size, 0);
                            free(buffer);
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&mutex);
                break;
            }
            default:
                break;
        }

        chat__client_petition__free_unpacked(request, NULL);
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    pthread_t thread_id;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[SERVER-ERROR]: Creación de socket fallida");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("[SERVER-ERROR]: Bind fallido");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("[SERVER-ERROR]: Listen fallido");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER]: Esperando conexiones...\n");

    while (1) {
        socklen_t client_address_length = sizeof(client_address);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length)) < 0) {
            perror("[SERVER-ERROR]: Aceptación de conexión fallida");
            exit(EXIT_FAILURE);
        }

        pthread_create(&thread_id, NULL, handle_client, (void *)&client_socket);
    }

    close(server_socket);
    pthread_mutex_destroy(&mutex);
    return 0;
}
