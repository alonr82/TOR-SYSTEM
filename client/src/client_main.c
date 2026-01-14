#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tor_protocol.h"   // נמצא ב-common, וה-INCLUDES כבר כולל -Icommon

static int send_exact(int fd, const void *buf, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(fd, (const uint8_t*)buf + off, n - off, 0);
        if (w <= 0) return -1;
        off += (size_t)w;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <msg>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    const char *text = argv[3];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Bad IP\n");
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    tor_msg_t m;
    memset(&m, 0, sizeof(m));
    m.header.type = TOR_MSG_DATA;

    size_t n = strlen(text);
    if (n > sizeof(m.payload)) n = sizeof(m.payload);
    memcpy(m.payload, text, n);

    m.header.payload_len = htons((uint16_t)n);

    if (send_exact(fd, &m, sizeof(m)) != 0) {
        fprintf(stderr, "send failed\n");
        close(fd);
        return 1;
    }

    printf("[Client] Sent DATA to %s:%u (%zu bytes)\n", ip, port, n);
    close(fd);
    return 0;
}
