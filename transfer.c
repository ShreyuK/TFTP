#include "transfer.h"

// Handles a Read Request (RRQ) from a client
int handle_rrq(int sockfd, struct sockaddr_in* client_addr,
               const char* filename) {
    uint8_t buffer[TFTP_MAX_PACKET_SIZE];
    uint8_t data[TFTP_BLOCK_SIZE];
    uint8_t last_packet[TFTP_MAX_PACKET_SIZE];

    TFTPPacket packet;
    int len;

    char path[512];
    snprintf(path, sizeof(path), "%s%s", SERVER_DIR, filename);

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        len = build_error_packet(buffer, ERR_FILE_NOT_FOUND, "File not found");
        send_packet(sockfd, client_addr, buffer, len);
        return -1;
    }

    uint16_t block = 1;

    // Send the first DATA packet
    while (1) {
        size_t bytes_read = fread(data, 1, TFTP_BLOCK_SIZE, fp);
        if (ferror(fp)) {
            len = build_error_packet(buffer, ERR_DISK_FULL, "Disk full");
            send_packet(sockfd, client_addr, buffer, len);
            fclose(fp);
            return -1;
        }

        // Build the DATA packet
        int last_packet_len =
            build_data_packet(last_packet, block, data, bytes_read);

        int retries = 0;

        // Send the DATA packet to the client
        if (send_packet(sockfd, client_addr, last_packet, last_packet_len) <
            0) {
            perror("sendto");
            fclose(fp);
            return -1;
        }

        // Wait for the corresponding ACK packet from the client
        while (1) {
            printf("Sent DATA block %u\n", block);

            int n = receive_packet_timeout(sockfd, client_addr, buffer,
                                           sizeof(buffer));

            // Handle timeout and retries
            if (n == RECV_TIMEOUT) {
                retries++;

                // If maximum retries reached, abort the transfer
                if (retries == MAX_RETRIES) {
                    printf("Transfer aborted.\n");
                    fclose(fp);
                    return -1;
                }

                // Resend the last DATA packet to prompt the client to resend
                // the ACK
                send_packet(sockfd, client_addr, last_packet, last_packet_len);

                printf(
                    "Timeout waiting for ACK %u "
                    "(Retry %d/%d)\n",
                    block, retries, MAX_RETRIES);

                continue;
            }

            // If an error occurred while receiving the packet, abort the
            // transfer
            if (n == RECV_ERROR) {
                fclose(fp);
                return -1;
            }

            // Parse the received packet and check for errors
            if (parse_packet(buffer, n, &packet) != 0)
                continue;

            // Handle the received packet based on its opcode
            if (packet.opcode != OP_ACK)
                continue;

            // Handle duplicate or unexpected ACK packets
            if (packet.ack.block < block) {
                printf("Duplicate ACK %u received\n", packet.ack.block);

                // Resend the last DATA packet to prompt the client to resend
                // the ACK
                send_packet(sockfd, client_addr, last_packet, last_packet_len);
                retries++;

                continue;
            }

            // Handle unexpected ACK packets
            if (packet.ack.block > block) {
                printf("Unexpected ACK %u\n", packet.ack.block);

                continue;
            }

            printf("Received ACK %u\n", block);

            break;
        }

        // If the last DATA packet was sent, break the loop
        if (bytes_read < TFTP_BLOCK_SIZE)
            break;

        block++;
    }

    printf("File transfer completed\n");

    fclose(fp);
    return 0;
}

// Handles a Write Request (WRQ) from a client
int handle_wrq(int sockfd, struct sockaddr_in* client_addr,
               const char* filename) {
    uint8_t buffer[TFTP_MAX_PACKET_SIZE];

    TFTPPacket packet;

    int len;

    // Check if file already exists
    char path[512];
    snprintf(path, sizeof(path), "%s%s", SERVER_DIR, filename);
    FILE* fp = fopen(path, "rb");

    if (fp != NULL) {
        fclose(fp);

        len =
            build_error_packet(buffer, ERR_FILE_EXISTS, "File already exists");

        send_packet(sockfd, client_addr, buffer, len);

        return -1;
    }

    // Create the file for writing
    snprintf(path, sizeof(path), "%s%s", SERVER_DIR, filename);
    fp = fopen(path, "wb");

    if (fp == NULL) {
        len = build_error_packet(buffer, ERR_ACCESS, "Cannot create file");

        send_packet(sockfd, client_addr, buffer, len);

        return -1;
    }

    // Send ACK 0 to the client to indicate readiness to receive data
    len = build_ack_packet(buffer, 0);
    send_packet(sockfd, client_addr, buffer, len);

    printf("ACK 0 sent\n");

    uint16_t expected_block = 1;
    uint16_t last_ack = 0;
    int retries = 0;

    // Receive DATA packets from the client
    while (1) {
        int n =
            receive_packet_timeout(sockfd, client_addr, buffer, sizeof(buffer));

        // Handle timeout and retries
        if (n == RECV_TIMEOUT) {
            retries++;

            // If maximum retries reached, abort the transfer
            if (retries >= MAX_RETRIES) {
                printf("Client not responding.\n");

                fclose(fp);

                return -1;
            }

            printf(
                "Timeout waiting for DATA "
                "(Retry %d/%d)\n",
                retries, MAX_RETRIES);

            // Resend the last ACK to prompt the client to resend the DATA
            // packet
            len = build_ack_packet(buffer, last_ack);
            send_packet(sockfd, client_addr, buffer, len);

            continue;
        }

        // If an error occurred while receiving the packet, abort the transfer
        if (n == RECV_ERROR) {
            fclose(fp);
            return -1;
        }

        retries = 0;

        // Parse the received packet and check for errors
        if (parse_packet(buffer, n, &packet) != 0) {
            continue;
        }

        // Handle the received packet based on its opcode
        if (packet.opcode != OP_DATA) {
            continue;
        }

        // Handle duplicate or unexpected DATA packets
        if (packet.data.block < expected_block) {
            printf("Duplicate DATA block %u\n", packet.data.block);

            // Resend the last ACK to prompt the client to resend the DATA
            // packet
            len = build_ack_packet(buffer, packet.data.block);
            send_packet(sockfd, client_addr, buffer, len);

            continue;
        }

        // Handle unexpected DATA packets
        if (packet.data.block != expected_block) {
            printf("Unexpected DATA block %u\n", packet.data.block);

            continue;
        }

        // Write the received data to the file
        size_t written =
            fwrite(packet.data.data, 1, packet.data.data_length, fp);

        if (written != packet.data.data_length) {
            perror("fwrite");
            fclose(fp);
            return -1;
        }

        printf("Received DATA block %u\n", packet.data.block);

        //  Send ACK for the received DATA packet
        last_ack = packet.data.block;
        len = build_ack_packet(buffer, packet.data.block);

        send_packet(sockfd, client_addr, buffer, len);

        expected_block++;

        // Last DATA packet is indicated by a packet smaller than the maximum
        // block size
        if (packet.data.data_length < TFTP_BLOCK_SIZE) {
            break;
        }
    }

    printf("Upload completed\n");

    fclose(fp);

    return 0;
}