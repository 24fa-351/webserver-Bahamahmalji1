#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

int request_count = 0;
long total_bytes_received = 0;
long total_bytes_sent = 0;
pthread_mutex_t stats_mutex;

void *handle_client(void *client_socket);
void handle_static(int sock, const char *buffer);
void handle_stats(int sock);
void handle_calc(int sock, const char *buffer);

int main(int argc, char *argv[]) {
    int port = 80;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    pthread_mutex_init(&stats_mutex, NULL);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 10);

    printf("Server running on port %d\n", port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        pthread_t thread;
        int *client_sock = malloc(sizeof(int));
        *client_sock = client_socket;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);  // Auto clean-up thread resources
    }

    close(server_socket);
    pthread_mutex_destroy(&stats_mutex);
    return 0;
}

void *handle_client(void *client_socket) {
    int sock = *(int*)client_socket;
    free(client_socket);

    char buffer[1024];
    int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
    total_bytes_received += bytes_received;

    if (strstr(buffer, "GET /static") == buffer) {
        handle_static(sock, buffer);
    } else if (strstr(buffer, "GET /stats") == buffer) {
        handle_stats(sock);
    } else if (strstr(buffer, "GET /calc") == buffer) {
        handle_calc(sock, buffer);
    } else {
        send(sock, "HTTP/1.1 404 Not Found\r\n", 24, 0);
    }

    close(sock);
    return NULL;
}

void handle_static(int sock, const char *buffer) {
    char file_path[256];
    sscanf(buffer, "GET /static/%s", file_path);
    char full_path[300] = "./static/";
    strcat(full_path, file_path);

    FILE *file = fopen(full_path, "rb");
    if (!file) {
        send(sock, "HTTP/1.1 404 Not Found\r\n", 24, 0);
        return;
    }

    send(sock, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n", 56, 0);
    char file_buffer[1024];
    int bytes_read;
    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        send(sock, file_buffer, bytes_read, 0);
        total_bytes_sent += bytes_read;
    }
    fclose(file);
}

void handle_stats(int sock) {
    pthread_mutex_lock(&stats_mutex);
    request_count++;
    pthread_mutex_unlock(&stats_mutex);

    char stats_html[256];
    snprintf(stats_html, sizeof(stats_html),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
             "<html><body><h1>Server Stats</h1>"
             "<p>Requests: %d</p><p>Bytes Received: %ld</p>"
             "<p>Bytes Sent: %ld</p></body></html>",
             request_count, total_bytes_received, total_bytes_sent);

    send(sock, stats_html, strlen(stats_html), 0);
    total_bytes_sent += strlen(stats_html);
}

void handle_calc(int sock, const char *buffer) {
    int a, b;
    sscanf(buffer, "GET /calc?a=%d&b=%d", &a, &b);
    int sum = a + b;

    char calc_result[128];
    snprintf(calc_result, sizeof(calc_result),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
             "<html><body><h1>Calc Result</h1><p>%d + %d = %d</p></body></html>",
             a, b, sum);

    send(sock, calc_result, strlen(calc_result), 0);
    total_bytes_sent += strlen(calc_result);
}
