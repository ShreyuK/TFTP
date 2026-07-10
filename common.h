#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERVER_PORT 5000
#define SERVER_IP "127.0.0.1"

#define TIMEOUT_SEC 5
#define MAX_RETRIES 5

#define RECV_ERROR -1
#define RECV_TIMEOUT -2

#define CLIENT_DIR "client_files/"
#define SERVER_DIR "server_files/"

int create_udp_socket(void);

int send_packet(int, const struct sockaddr_in*, const uint8_t*, size_t);
int receive_packet(int, struct sockaddr_in*, uint8_t*, size_t);

/* Waits TIMEOUT_SEC seconds.
 * Returns:
 *      bytes received
 *      RECV_TIMEOUT
 *      RECV_ERROR
 */
int receive_packet_timeout(int, struct sockaddr_in*, uint8_t*, size_t);

#endif