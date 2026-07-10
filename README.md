# TFTP Protocol Implementation in C

A simple implementation of the **Trivial File Transfer Protocol (TFTP)** using **UDP sockets** in C on Linux. The project follows the basic principles of **RFC 1350** and supports both file upload and download between a client and a server.

## Features

- Read Request (RRQ)
- Write Request (WRQ)
- File transfer using UDP
- 512-byte data block transmission
- ACK and ERROR packet handling
- Timeout and retransmission support
- Duplicate packet detection
- Concurrent client handling using `fork()`
- Modular code structure

## Building

Compile the project using:

```bash
make
```

This generates two executables:

- `server`
- `client`

## Running

Start the server:

```bash
./server
```

Start the client:

```bash
./client
```

## Usage

### Download a file (RRQ)

```bash
get <filename>
```

Example:

```bash
get hello.txt
```

### Upload a file (WRQ)

```bash
put <filename>
```

Example:

```bash
put sample.txt
```

## Working

1. The client sends a **Read Request (RRQ)** or **Write Request (WRQ)**.
2. The server validates the request and forks a child process for the transfer.
3. Files are transferred in **512-byte DATA packets**.
4. Each DATA packet is acknowledged with an ACK packet.
5. If an ACK or DATA packet is lost, the sender retransmits after a timeout.
6. The transfer ends when the last DATA packet contains fewer than **512 bytes**.

## Packet Types

| Opcode | Packet |
|---------|--------|
| 1 | RRQ (Read Request) |
| 2 | WRQ (Write Request) |
| 3 | DATA |
| 4 | ACK |
| 5 | ERROR |

## Error Handling

The implementation handles common TFTP errors including:

- File not found
- File already exists
- Access violation
- Illegal TFTP operation
- Unknown transfer ID
- Timeout and retransmission
- Duplicate DATA and ACK packets

## Requirements

- Linux
- GCC Compiler
- POSIX Socket API

## References

- RFC 1350 – Trivial File Transfer Protocol
- Linux Socket Programming
