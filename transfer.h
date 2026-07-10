#ifndef TRANSFER_H
#define TRANSFER_H

#include "common.h"
#include "packet.h"

int handle_rrq(int sockfd, struct sockaddr_in* client_addr,
               const char* filename);

int handle_wrq(int sockfd, struct sockaddr_in* client_addr,
               const char* filename);

#endif
