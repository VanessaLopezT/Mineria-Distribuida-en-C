#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>

#define PORT 5000
#define HOST "0.0.0.0"
#define NUM_WORKERS 2
#define CONDITION_SUFFIX "000000"
#define CHARS "abcdefghijklmnopqrstuvwxyz0123456789"

typedef struct {
    int conn;
    int inicio;
    int fin;
    int idx;
    char base_text[128];
} WorkerData;

volatile bool stop_signal = false;

void* escuchar_worker(void* arg) {
    WorkerData* data = (WorkerData*)arg;
    char buffer[2048];
    int bytes = recv(data->conn, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return NULL;

    buffer[bytes] = '\0';

    if (strncmp(buffer, "FOUND", 5) == 0 && !stop_signal) {
        stop_signal = true;
        char relleno[128], hash_val[128];
        sscanf(buffer, "FOUND:%[^:]:%s", relleno, hash_val);
        printf("\n[+] Hash encontrado por worker %d\n", data->idx + 1);
        printf("Relleno: %s\nHash: %s\nRango: %d - %d\n", relleno, hash_val, data->inicio, data->fin);
    } else if (strcmp(buffer, "NOT_FOUND") == 0 && !stop_signal) {
        printf("Worker %d no encontró hash en su rango.\n", data->idx + 1);
    }

    close(data->conn);
    return NULL;
}

void dividir_rangos(int total, int partes, int rangos[][2]) {
    int paso = (int)ceil((double)total / partes);
    for (int i = 0; i < partes; i++) {
        rangos[i][0] = i * paso;
        rangos[i][1] = (i + 1) * paso;
        if (rangos[i][1] > total) rangos[i][1] = total;
    }
}

int main() {
    int server_fd, conn;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cli_len = sizeof(cliaddr);
    pthread_t threads[NUM_WORKERS];
    WorkerData workers[NUM_WORKERS];

    char base_text[128];
    printf("Ingrese texto base (máx 100 caracteres): ");
    fgets(base_text, sizeof(base_text), stdin);
    base_text[strcspn(base_text, "\n")] = 0;

    printf("Condición: hash SHA256 termina en '%s'\n", CONDITION_SUFFIX);
    printf("Esperando %d workers...\n", NUM_WORKERS);

    int total_combos = pow(strlen(CHARS), 5);
    int rangos[NUM_WORKERS][2];
    dividir_rangos(total_combos, NUM_WORKERS, rangos);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
	    perror("setsockopt");
	    close(server_fd);
	    exit(EXIT_FAILURE);
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);

	if (bind(server_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
	    perror("bind");
	    exit(EXIT_FAILURE);
	}


    listen(server_fd, NUM_WORKERS);

    int connected_workers = 0;

    while (connected_workers < NUM_WORKERS && !stop_signal) {
    // usa select con timeout para no quedarse en el accept()
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    timeout.tv_sec = 1;   // Espera max 1 seg
    timeout.tv_usec = 0;

    int ready = select(server_fd + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) {
        perror("select");
        continue;
    }
    if (ready == 0) {
        //stop_signal
        continue;
    }

	conn = accept(server_fd, (struct sockaddr*)&cliaddr, &cli_len);
	if (conn < 0) {
	    perror("accept");
	    continue;
	}

	// Rechaza conexiones extra del numero de workers
	if (connected_workers >= NUM_WORKERS || stop_signal) {
	    char deny_msg[128];
	    snprintf(deny_msg, sizeof(deny_msg), "STOP");
	    send(conn, deny_msg, strlen(deny_msg) + 1, 0);
	    close(conn);
	    continue;  // vuelve al loop sin contar ese worker
	}


    if (stop_signal) { // si ya encontró el hash, se cierra
        char deny_msg[128];
        snprintf(deny_msg, sizeof(deny_msg), "%s:%d:%d:%s", " ", 0, 0, "STOP");
        send(conn, deny_msg, strlen(deny_msg) + 1, 0);
        close(conn);
        break;
    }

    int i = connected_workers;
    printf("Worker %d conectado desde %s:%d, rango %d-%d\n",
           i + 1, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port),
           rangos[i][0], rangos[i][1]);

    char msg[256];
    snprintf(msg, sizeof(msg), "%s:%d:%d:%s",
             base_text, rangos[i][0], rangos[i][1], CONDITION_SUFFIX);
    send(conn, msg, strlen(msg) + 1, 0);


    workers[i].conn = conn;
    workers[i].inicio = rangos[i][0];
    workers[i].fin = rangos[i][1];
    workers[i].idx = i;
    strcpy(workers[i].base_text, base_text);

    pthread_create(&threads[i], NULL, escuchar_worker, &workers[i]);
    connected_workers++;
}



       while (!stop_signal) {
        usleep(10000);
    }

    // Cierra el socket principal para bloquear conexiones
    if (stop_signal || connected_workers >= NUM_WORKERS) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
    }

    // Envia señal STOP a todos los workers conectados
    for (int i = 0; i < connected_workers; i++) {
        send(workers[i].conn, "STOP", 4, 0);
    }

    if (stop_signal) {
        printf("\n[!] Servidor cerró recepción de conexiones: hash encontrado.\n");
    }

    if (connected_workers == NUM_WORKERS) {
    printf("\n[!] Se envió señal de stop a los demás workers.\n");
    }

    for (int i = 0; i < connected_workers; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

