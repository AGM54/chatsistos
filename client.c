#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "sistos.pb-c.h"

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    char username[75];
} Client;

void *receive_messages(void *client_ptr) {
    Client *client = (Client *)client_ptr;
    uint8_t buffer[BUFFER_SIZE];

    while (1) {
        ssize_t bytes_received = recv(client->sockfd, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0) {
            // Error en la recepción o servidor cerró la conexión
            break;
        }

        // Aquí iría la lógica para manejar los mensajes entrantes,
        // como imprimir mensajes de chat o manejar actualizaciones de estado.
        // Por ahora, solo imprimiremos los datos brutos recibidos.
        printf("Mensaje recibido: %s\n", buffer);
    }
    return NULL;
}

void send_registration(Client *client) {
    // Aquí iría el código para enviar un mensaje de registro al servidor
    // utilizando Protocol Buffers.
}

int main() {
    Client client;
    strcpy(client.username, "UsuarioEjemplo");

    // Crear socket
    client.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client.sockfd == -1) {
        perror("No se pudo crear el socket");
        return 1;
    }

    // Configurar la dirección del servidor
    client.server_addr.sin_family = AF_INET;
    client.server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &client.server_addr.sin_addr);

    // Conectar al servidor
    if (connect(client.sockfd, (struct sockaddr *)&client.server_addr, sizeof(client.server_addr)) == -1) {
        perror("No se pudo conectar con el servidor");
        close(client.sockfd);
        return 1;
    }

    printf("Conectado al servidor.\n");

    // Enviar mensaje de registro
    send_registration(&client);

    // Crear hilo para recibir mensajes del servidor
    pthread_t recv_thread;
    if(pthread_create(&recv_thread, NULL, receive_messages, (void*)&client) != 0) {
        perror("No se pudo crear el hilo de recepción");
        close(client.sockfd);
        return 1;
    }

    // Aquí podrías permitir que el usuario ingrese comandos o mensajes

    // Unir hilo y limpiar antes de terminar
    pthread_join(recv_thread, NULL);
    close(client.sockfd);
    return 0;
}
