#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000
#define CHARS "abcdefghijklmnopqrstuvwxyz0123456789"

volatile bool stop_signal = false;

void* escuchar_stop(void* arg) {
    int s = *(int*)arg;
    char buffer[128];
    while (1) {
        int bytes = recv(s, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        if (strcmp(buffer, "STOP") == 0) {
            printf("Servidor ordenó detener la minería.\n");
            stop_signal = true;
            break;
        }
    }
    return NULL;
}

void sha256_string(const char* str, char* outputBuffer) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)str, strlen(str), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    outputBuffer[64] = 0;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[256];
    char base_text[128], condition[32];
    int inicio, fin;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }

    recv(sock, buffer, sizeof(buffer) - 1, 0);
    buffer[strcspn(buffer, "\n")] = 0;

    sscanf(buffer, "%[^:]:%d:%d:%s", base_text, &inicio, &fin, condition);
    printf("Miner trabajando rango %d-%d con condición '%s'...\n", inicio, fin, condition);

    pthread_t stop_thread;
    pthread_create(&stop_thread, NULL, escuchar_stop, &sock);

    int len_chars = strlen(CHARS);
    int total = 0;
    char combo[6];
    combo[5] = '\0';

    for (int i1 = 0; i1 < len_chars && !stop_signal; i1++) {
        for (int i2 = 0; i2 < len_chars && !stop_signal; i2++) {
            for (int i3 = 0; i3 < len_chars && !stop_signal; i3++) {
                for (int i4 = 0; i4 < len_chars && !stop_signal; i4++) {
                    for (int i5 = 0; i5 < len_chars && !stop_signal; i5++) {
                        if (total < inicio) {
                            total++;
                            continue;
                        }
                        if (total >= fin) {
                            stop_signal = true;
                            break;
                        }

                        combo[0] = CHARS[i1];
                        combo[1] = CHARS[i2];
                        combo[2] = CHARS[i3];
                        combo[3] = CHARS[i4];
                        combo[4] = CHARS[i5];

                        char texto[256], hash_str[65];
                        snprintf(texto, sizeof(texto), "%s%s", base_text, combo);
                        sha256_string(texto, hash_str);

                        if (strstr(hash_str + strlen(hash_str) - strlen(condition), condition)) {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "FOUND:%s:%s", combo, hash_str);
                            send(sock, msg, strlen(msg), 0);
                            printf("Hash encontrado! %s\n", hash_str);
                            stop_signal = true;
                            break;
                        }
                        total++;
                    }
                }
            }
        }
    }

    if (!stop_signal) {
        send(sock, "NOT_FOUND", 9, 0);
        printf("No se encontró hash en este rango.\n");
    }

    pthread_join(stop_thread, NULL);
    close(sock);
    return 0;
}