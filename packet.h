#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>

// TFTP Constants
#define TFTP_PORT 69

#define TFTP_BLOCK_SIZE 512
#define TFTP_MAX_PACKET_SIZE 516

// TFTP Opcodes
#define OP_RRQ 1
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5

// TFTP Error Codes
#define ERR_NOT_DEFINED 0
#define ERR_FILE_NOT_FOUND 1
#define ERR_ACCESS 2
#define ERR_DISK_FULL 3
#define ERR_ILLEGAL_OP 4
#define ERR_UNKNOWN_TID 5
#define ERR_FILE_EXISTS 6
#define ERR_NO_SUCH_USER 7

// Transfer Modes
#define MODE_OCTET "octet"
#define MODE_NETASCII "netascii"

// Parsed Packet Structure
typedef struct {
    uint16_t opcode;

    union {
        struct {
            char filename[256];
            char mode[16];
        } request;

        struct {
            uint16_t block;
            uint8_t data[TFTP_BLOCK_SIZE];
            size_t data_length;
        } data;

        struct {
            uint16_t block;
        } ack;

        struct {
            uint16_t error_code;
            char message[256];
        } error;
    };

} TFTPPacket;

// Packet Builder Functions
int build_rrq_packet(uint8_t* buffer, const char* filename, const char* mode);

int build_wrq_packet(uint8_t* buffer, const char* filename, const char* mode);

int build_data_packet(uint8_t* buffer, uint16_t block, const uint8_t* data,
                      size_t length);

int build_ack_packet(uint8_t* buffer, uint16_t block);

int build_error_packet(uint8_t* buffer, uint16_t error_code,
                       const char* message);

// Packet Parser
int parse_packet(const uint8_t* buffer, size_t length, TFTPPacket* packet);

#endif