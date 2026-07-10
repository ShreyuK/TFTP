#include "common.h"
#include "packet.h"
#include "transfer.h"

int main() {
    struct sockaddr_in server, client;

    uint8_t buffer[TFTP_MAX_PACKET_SIZE];

    TFTPPacket packet;

    // Create a UDP socket for the TFTP server
    int sockfd = create_udp_socket();

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind");
        return 1;
    }

    printf("TFTP Server Started...\n");

    // Main loop to handle incoming requests
    while (1) {
        // Clean up any finished child processes to prevent zombies
        while (waitpid(-1, NULL, WNOHANG) > 0);

        // Receive a packet from a client
        int n = receive_packet(sockfd, &client, buffer, sizeof(buffer));
        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        // Parse the received packet and check for errors
        if (parse_packet(buffer, n, &packet) != 0) {
            printf("Invalid Packet\n");
            continue;
        }

        // Handle the received packet based on its opcode
        if (packet.opcode == OP_RRQ || packet.opcode == OP_WRQ) {
            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                break;
            }

            // Child process
            if (pid == 0) {
                int transfer_sock = create_udp_socket();

                struct sockaddr_in transfer_addr;

                memset(&transfer_addr, 0, sizeof(transfer_addr));
                transfer_addr.sin_family = AF_INET;
                transfer_addr.sin_addr.s_addr = INADDR_ANY;
                transfer_addr.sin_port = htons(0);  // Let OS choose the port

                if (bind(transfer_sock, (struct sockaddr*)&transfer_addr,
                         sizeof(transfer_addr)) < 0) {
                    perror("bind");

                    close(transfer_sock);
                    exit(EXIT_FAILURE);
                }

                socklen_t addrlen = sizeof(transfer_addr);

                getsockname(transfer_sock, (struct sockaddr*)&transfer_addr,
                            &addrlen);

                printf("Child [%d] using transfer port %u\n", getpid(),
                       ntohs(transfer_addr.sin_port));

                if (packet.opcode == OP_RRQ) {
                    printf("\nChild [%d] handling RRQ\n", getpid());
                    handle_rrq(transfer_sock, &client, packet.request.filename);
                } else {
                    printf("\nChild [%d] handling WRQ\n", getpid());
                    handle_wrq(transfer_sock, &client, packet.request.filename);
                }

                close(transfer_sock);
                close(sockfd);

                exit(EXIT_SUCCESS);
            }

            // Parent process
            printf("Spawned child %d\n", pid);

        } else {
            printf("Unsupported opcode %u\n", packet.opcode);
        }
    }

    close(sockfd);

    return 0;
}