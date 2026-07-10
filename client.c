#include "common.h"
#include "packet.h"

int tftp_get(const char* server_ip, const char* filename, const char* mode);
int tftp_put(const char* server_ip, const char* filename, const char* mode);

int main(void) {
    char command[256];
    char server_ip[32] = SERVER_IP;
    char transfer_mode[16] = MODE_OCTET;

    printf("Simple TFTP Client\n");
    printf("Type 'help' for available commands.\n");

    while (1) {
        printf("tftp> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
            break;

        /* Remove newline */
        command[strcspn(command, "\n")] = '\0';
        char* cmd = strtok(command, " ");
        char* arg = strtok(NULL, " ");

        if (cmd == NULL) {
            continue;
        } else if (strcmp(cmd, "help") == 0) {  // Display help
            printf("\nAvailable commands:\n");
            printf("connect <ip-address>\n");
            printf("get <filename>\n");
            printf("put <filename>\n");
            printf("mode <octet|netascii>\n");
            printf("bye\n");
            printf("quit\n\n");
        } else if (strcmp(cmd, "connect") == 0) {  // Connect to a server
            if (arg == NULL) {
                printf("Usage: connect <ip-address>\n");
                continue;
            }

            struct sockaddr_in temp;
            if (inet_pton(AF_INET, arg, &temp.sin_addr) != 1) {
                printf("Invalid IP address.\n");
                continue;
            }

            strncpy(server_ip, arg, sizeof(server_ip) - 1);
            server_ip[sizeof(server_ip) - 1] = '\0';

            printf("Connected to %s\n", server_ip);
        } else if (strcmp(cmd, "mode") == 0) {  // Set transfer mode
            if (arg == NULL) {
                printf("Current mode: %s\n", transfer_mode);
                continue;
            }

            if (strcmp(arg, MODE_OCTET) == 0 ||
                strcmp(arg, MODE_NETASCII) == 0) {
                strcpy(transfer_mode, arg);
                printf("Transfer mode set to %s\n", transfer_mode);
            } else {
                printf("Invalid mode.\n");
                printf("Supported modes: octet, netascii\n");
            }
        } else if (strcmp(cmd, "get") == 0) {  // Download a file
            if (arg == NULL) {
                printf("Usage: get <filename>\n");
                continue;
            }
            printf("Downloading '%s'...\n", arg);

            if (tftp_get(server_ip, arg, transfer_mode) == 0)
                printf("Download successful.\n");
            else
                printf("Download failed.\n");

        } else if (strcmp(cmd, "put") == 0) {  // Upload a file
            if (arg == NULL) {
                printf("Usage: put <filename>\n");
                continue;
            }
            printf("Uploading '%s'...\n", arg);

            if (tftp_put(server_ip, arg, transfer_mode) == 0)
                printf("Upload successful.\n");
            else
                printf("Upload failed.\n");

        } else if (strcmp(cmd, "bye") == 0 ||
                   strcmp(cmd, "quit") == 0) {  // Exit the client
            printf("Closing TFTP client...\n");
            break;
        } else {
            printf("Unknown command '%s'\n", cmd);
            printf("Type 'help' for available commands.\n");
        }
    }

    return 0;
}

// Function to download a file from the TFTP server
int tftp_get(const char* server_ip, const char* filename, const char* mode) {
    struct sockaddr_in server;
    TFTPPacket packet;

    uint8_t buffer[TFTP_MAX_PACKET_SIZE];

    uint16_t expected_block = 1;
    uint16_t last_ack = 0;
    int retries = 0;

    // Create a UDP socket for communication with the TFTP server
    int sockfd = create_udp_socket();

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server.sin_addr);

    int len = build_rrq_packet(buffer, filename, mode);
    send_packet(sockfd, &server, buffer, len);

    printf("RRQ Sent\n");

    char path[512];
    snprintf(path, sizeof(path), "%s%s", CLIENT_DIR, filename);

    // Open the file to write the downloaded data
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        close(sockfd);
        return 1;
    }

    // Receive DATA packets and send ACKs
    while (1) {
        int n = receive_packet_timeout(sockfd, &server, buffer, sizeof(buffer));

        // Handle timeout and retries
        if (n == RECV_TIMEOUT) {
            retries++;

            // If maximum retries reached, abort the transfer
            if (retries >= MAX_RETRIES) {
                printf("Server not responding.\n");
                break;
            }

            printf(
                "Timeout waiting for DATA "
                "(Retry %d/%d)\n",
                retries, MAX_RETRIES);

            // Resend the last ACK to prompt the server to resend the DATA
            // packet
            len = build_ack_packet(buffer, last_ack);
            send_packet(sockfd, &server, buffer, len);

            continue;
        }

        // If an error occurred while receiving the packet, break the loop
        if (n == RECV_ERROR) {
            break;
        }

        retries = 0;

        // Parse the received packet and check for errors
        if (parse_packet(buffer, n, &packet) != 0) {
            break;
        }

        // Handle the received packet based on its opcode
        if (packet.opcode == OP_DATA) {
            // Check for duplicate or unexpected DATA packets
            if (packet.data.block < expected_block) {
                printf("Duplicate DATA block %u\n", packet.data.block);

                len = build_ack_packet(buffer, packet.data.block);
                send_packet(sockfd, &server, buffer, len);

                continue;
            }

            // Check if the received DATA block is the expected one
            if (packet.data.block != expected_block) {
                printf("Unexpected DATA block %u\n", packet.data.block);

                continue;
            }

            size_t written =
                fwrite(packet.data.data, 1, packet.data.data_length, fp);

            if (written != packet.data.data_length) {
                perror("fwrite");
                break;
            }

            len = build_ack_packet(buffer, packet.data.block);
            send_packet(sockfd, &server, buffer, len);

            last_ack = packet.data.block;
            expected_block++;

            // If the received DATA packet is smaller than the maximum block
            // size, it indicates the end of the file transfer
            if (packet.data.data_length < TFTP_BLOCK_SIZE) {
                printf("File transfer completed\n");
                break;
            }

        } else if (packet.opcode == OP_ERROR) {  // Handle error packet
            printf("Error: %s\n", packet.error.message);
            break;
        }
    }

    fclose(fp);
    close(sockfd);

    return 0;
}

// Function to upload a file to the TFTP server
int tftp_put(const char* server_ip, const char* filename, const char* mode) {
    struct sockaddr_in server;
    TFTPPacket packet;

    uint8_t buffer[TFTP_MAX_PACKET_SIZE];
    uint8_t data[TFTP_BLOCK_SIZE];
    uint8_t last_packet[TFTP_MAX_PACKET_SIZE];

    // Create a UDP socket for communication with the TFTP server
    int sockfd = create_udp_socket();

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET, server_ip, &server.sin_addr);

    char path[512];
    snprintf(path, sizeof(path), "%s%s", CLIENT_DIR, filename);

    // Open the file to be uploaded
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        perror("fopen");
        close(sockfd);
        return -1;
    }

    // Build and send the WRQ packet to initiate the file upload
    uint8_t wrq_packet[TFTP_MAX_PACKET_SIZE];
    int wrq_len = build_wrq_packet(wrq_packet, filename, mode);

    if (send_packet(sockfd, &server, wrq_packet, wrq_len) < 0) {
        perror("sendto");
        fclose(fp);
        close(sockfd);
        return -1;
    }

    printf("WRQ Sent\n");

    // Wait for ACK 0
    int retries = 0;

    // Receive ACK 0 from the server before starting the file upload
    while (1) {
        int n = receive_packet_timeout(sockfd, &server, buffer, sizeof(buffer));

        // Handle timeout and retries
        if (n == RECV_TIMEOUT) {
            retries++;

            if (retries >= MAX_RETRIES) {
                printf("Server not responding.\n");

                fclose(fp);
                close(sockfd);

                return -1;
            }

            printf(
                "Timeout waiting for ACK 0 "
                "(Retry %d/%d)\n",
                retries, MAX_RETRIES);

            send_packet(sockfd, &server, wrq_packet, wrq_len);

            continue;
        }

        // If an error occurred while receiving the packet, abort the transfer
        if (n == RECV_ERROR) {
            fclose(fp);
            close(sockfd);
            return -1;
        }

        retries = 0;

        // Parse the received packet and check for errors
        if (parse_packet(buffer, n, &packet) != 0)
            continue;

        // Handle the received packet based on its opcode
        if (packet.opcode == OP_ERROR) {
            printf("Server Error: %s\n", packet.error.message);

            fclose(fp);
            close(sockfd);
            return -1;
        }

        // Check if the received packet is an ACK packet with block number 0
        if (packet.opcode != OP_ACK)
            continue;

        // If the received ACK block number is not 0, continue waiting for the
        // correct ACK
        if (packet.ack.block != 0)
            continue;

        printf("Received ACK 0\n");

        break;
    }

    uint16_t block = 1;

    // Start sending DATA packets in a loop until the entire file is uploaded
    while (1) {
        size_t bytes_read = fread(data, 1, TFTP_BLOCK_SIZE, fp);
        if (ferror(fp)) {
            perror("fread");
            fclose(fp);
            close(sockfd);
            return -1;
        }

        int last_packet_len =
            build_data_packet(last_packet, block, data, bytes_read);

        retries = 0;

        // Send the DATA packet to the server
        send_packet(sockfd, &server, last_packet, last_packet_len);

        printf("Sent DATA block %u\n", block);

        // Wait for the corresponding ACK packet from the server
        while (1) {
            int n =
                receive_packet_timeout(sockfd, &server, buffer, sizeof(buffer));

            // Handle timeout and retries
            if (n == RECV_TIMEOUT) {
                retries++;

                // If maximum retries reached, abort the transfer
                if (retries >= MAX_RETRIES) {
                    printf("Transfer aborted.\n");

                    fclose(fp);
                    close(sockfd);

                    return -1;
                }

                printf(
                    "Timeout waiting for ACK %u "
                    "(Retry %d/%d)\n",
                    block, retries, MAX_RETRIES);

                // Resend the last DATA packet to prompt the server to resend
                // the ACK
                send_packet(sockfd, &server, last_packet, last_packet_len);

                continue;
            }

            // If an error occurred while receiving the packet, abort the
            // transfer
            if (n == RECV_ERROR) {
                fclose(fp);
                close(sockfd);
                return -1;
            }

            // Parse the received packet and check for errors
            if (parse_packet(buffer, n, &packet) != 0)
                continue;

            // Handle the received packet based on its opcode
            if (packet.opcode == OP_ERROR) {
                printf("Server Error: %s\n", packet.error.message);

                fclose(fp);
                close(sockfd);

                return -1;
            }

            // Check if the received packet is an ACK packet
            if (packet.opcode != OP_ACK)
                continue;

            // Check for duplicate or unexpected ACK packets
            if (packet.ack.block < block) {
                printf("Duplicate ACK %u\n", packet.ack.block);

                send_packet(sockfd, &server, last_packet, last_packet_len);

                continue;
            }

            // Check if the received ACK block number is greater than the
            // expected block
            if (packet.ack.block > block) {
                printf("Unexpected ACK %u\n", packet.ack.block);

                continue;
            }

            printf("Received ACK %u\n", block);

            break;
        }

        // If the last DATA packet sent was smaller than the maximum block size,
        // it indicates the end of the file transfer
        if (bytes_read < TFTP_BLOCK_SIZE)
            break;

        block++;
    }

    printf("Upload completed\n");

    fclose(fp);
    close(sockfd);

    return 0;
}