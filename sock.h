#ifndef _SOCK_H
#define _SOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_LENG 2048 // lo subimos por seguridad


int sock_send(int sd, char * buff, int size);
int sock_receive(int sd, char*buff, int size);


#endif
