#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "utils.h"

// Funzioni per la gestione del server
int init_server(int port);
void start_server(int server_fd);
void *handle_client(void *arg);

// Funzioni di utilità per le porte
int bind_to_available_port(int server_fd, struct sockaddr_in *address, int start_port);

#endif