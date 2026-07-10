#include "packet.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

// Builds a Read Request (RRQ) packet and returns the size of the packet.
int build_rrq_packet(uint8_t* buffer, const char* filename, const char* mode) {
    uint8_t* ptr = buffer;

    uint16_t opcode = htons(OP_RRQ);

    // Copy the opcode to the buffer
    memcpy(ptr, &opcode, 2);
    ptr += 2;

    // Copy the filename and mode to the buffer
    strcpy((char*)ptr, filename);
    ptr += strlen(filename) + 1;

    // Copy the mode to the buffer
    strcpy((char*)ptr, mode);
    ptr += strlen(mode) + 1;

    return (ptr - buffer);
}

// Builds a Write Request (WRQ) packet and returns the size of the packet.
int build_wrq_packet(uint8_t* buffer, const char* filename, const char* mode) {
    uint8_t* ptr = buffer;

    uint16_t opcode = htons(OP_WRQ);

    // Copy the opcode to the buffer
    memcpy(ptr, &opcode, 2);
    ptr += 2;

    // Copy the filename and mode to the buffer
    strcpy((char*)ptr, filename);
    ptr += strlen(filename) + 1;

    // Copy the mode to the buffer
    strcpy((char*)ptr, mode);
    ptr += strlen(mode) + 1;

    return (ptr - buffer);
}

// Builds a DATA packet and returns the size of the packet.
int build_data_packet(uint8_t* buffer, uint16_t block, const uint8_t* data,
                      size_t length) {
    if (length > TFTP_BLOCK_SIZE)
        return -1;

    uint8_t* ptr = buffer;

    uint16_t opcode = htons(OP_DATA);
    uint16_t blk = htons(block);

    // Copy the opcode and block number to the buffer
    memcpy(ptr, &opcode, 2);
    ptr += 2;

    memcpy(ptr, &blk, 2);
    ptr += 2;

    memcpy(ptr, data, length);
    ptr += length;

    return (ptr - buffer);
}

// Builds an ACK packet and returns the size of the packet.
int build_ack_packet(uint8_t* buffer, uint16_t block) {
    uint8_t* ptr = buffer;

    uint16_t opcode = htons(OP_ACK);
    uint16_t blk = htons(block);

    // Copy the opcode to the buffer
    memcpy(ptr, &opcode, 2);
    ptr += 2;

    // Copy the block number to the buffer
    memcpy(ptr, &blk, 2);
    ptr += 2;

    return (ptr - buffer);
}

// Builds an ERROR packet and returns the size of the packet.
int build_error_packet(uint8_t* buffer, uint16_t error_code,
                       const char* message) {
    uint8_t* ptr = buffer;

    uint16_t opcode = htons(OP_ERROR);
    uint16_t err = htons(error_code);

    // Copy the opcode to the buffer
    memcpy(ptr, &opcode, 2);
    ptr += 2;

    // Copy the error code to the buffer
    memcpy(ptr, &err, 2);
    ptr += 2;

    // Copy the error message to the buffer
    strcpy((char*)ptr, message);
    ptr += strlen(message) + 1;

    return (ptr - buffer);
}

// Parses a packet from the buffer and fills the TFTPPacket structure.
int parse_packet(const uint8_t* buffer, size_t length, TFTPPacket* packet) {
    if (length < 2)
        return -1;

    const uint8_t* ptr = buffer;
    uint16_t temp;

    // Copy the opcode from the buffer and convert it to host byte order
    memcpy(&temp, ptr, sizeof(temp));
    packet->opcode = ntohs(temp);
    ptr += 2;

    // Handle the packet based on its opcode
    switch (packet->opcode) {
        // Handle Read Request and Write Request packets
        case OP_RRQ:
        case OP_WRQ:
            strcpy(packet->request.filename, (char*)ptr);
            ptr += strlen((char*)ptr) + 1;
            strcpy(packet->request.mode, (char*)ptr);
            break;

        // Handle Data packets
        case OP_DATA:
            memcpy(&temp, ptr, sizeof(temp));
            packet->data.block = ntohs(temp);
            ptr += 2;
            packet->data.data_length = length - 4;
            memcpy(packet->data.data, ptr, packet->data.data_length);
            break;

        // Handle ACK packets
        case OP_ACK:
            memcpy(&temp, ptr, sizeof(temp));
            packet->ack.block = ntohs(temp);
            break;

        // Handle Error packets
        case OP_ERROR:
            memcpy(&temp, ptr, sizeof(temp));
            packet->error.error_code = ntohs(temp);
            ptr += 2;
            strcpy(packet->error.message, (char*)ptr);
            break;

        default:
            return -1;
    }

    return 0;
}