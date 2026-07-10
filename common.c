#include "common.h"

// Common functions for TFTP client and server
int create_udp_socket(void) {
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

// Function to send a packet to the specified destination address
int send_packet(int s, const struct sockaddr_in* d, const uint8_t* b,
                size_t l) {
    return sendto(s, b, l, 0, (const struct sockaddr*)d, sizeof(*d));
}

// Function to receive a packet from the specified source address
int receive_packet(int s, struct sockaddr_in* src, uint8_t* b, size_t sz) {
    socklen_t sl = sizeof(*src);
    return recvfrom(s, b, sz, 0, (struct sockaddr*)src, &sl);
}

// Function to receive a packet with a timeout
int receive_packet_timeout(int sockfd, struct sockaddr_in* src, uint8_t* buffer,
                           size_t size) {
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;

    int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (ret == 0) {
        // Timeout occurred
        return RECV_TIMEOUT;
    }

    if (ret < 0) {
        perror("select");
        return RECV_ERROR;
    }

    socklen_t addrlen = sizeof(*src);

    int n = recvfrom(sockfd, buffer, size, 0, (struct sockaddr*)src, &addrlen);
    if (n < 0) {
        perror("recvfrom");
        return RECV_ERROR;
    }

    return n;
}