/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */
 
 /*
 * This file implements the foggy-TCP backend. The backend runs in a different
 * thread and handles all the socket operations separately from the application.
 *
 * This is where most of your code should go. Feel free to modify any function
 * in this file.
 */



#include <assert.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "foggy_backend.h"
#include "foggy_function.h"
#include "foggy_packet.h"
#include "foggy_tcp.h"

/**
 * Tells if a given sequence number has been acknowledged by the socket.
 *
 * @param sock The socket to check for acknowledgements.
 * @param seq Sequence number to check.
 *
 * @return 1 if the sequence number has been acknowledged, 0 otherwise.
 */
int has_been_acked(foggy_socket_t *sock, uint32_t seq) {
  int result;
  while (pthread_mutex_lock(&(sock->window.ack_lock)) != 0) {
  }
  result = after(sock->window.last_ack_received, seq);
  pthread_mutex_unlock(&(sock->window.ack_lock));
  return result;
}

/**
 * Checks if the socket received any data.
 *
 * It first peeks at the header to figure out the length of the packet and
 * then reads the entire packet.
 *
 * @param sock The socket used for receiving data on the connection.
 * @param flags Flags that determine how the socket should wait for data.
 * Check `foggy_read_mode_t` for more information.
 */
void check_for_pkt(foggy_socket_t *sock, foggy_read_mode_t flags) {
  foggy_tcp_header_t hdr;
  uint8_t *pkt;
  socklen_t conn_len = sizeof(sock->conn);
  ssize_t len = 0;
  uint32_t plen = 0, buf_size = 0, n = 0;

  while (pthread_mutex_lock(&(sock->recv_lock)) != 0) {
  }
  switch (flags) {
    case NO_FLAG:
      len = recvfrom(sock->socket, &hdr, sizeof(foggy_tcp_header_t), MSG_PEEK,
                     (struct sockaddr *)&(sock->conn), &conn_len);
      break;

    // Fallthrough.
    case NO_WAIT:
      len = recvfrom(sock->socket, &hdr, sizeof(foggy_tcp_header_t),
                     MSG_DONTWAIT | MSG_PEEK, (struct sockaddr *)&(sock->conn),
                     &conn_len);
      break;

    default:
      perror("ERROR unknown flag");
  }


  if (len >= (ssize_t)sizeof(foggy_tcp_header_t)) {
    plen = get_plen(&hdr);
    pkt = (uint8_t*) malloc(plen);
    while (buf_size < plen) {
      n = recvfrom(sock->socket, pkt + buf_size, plen - buf_size, 0,
                   (struct sockaddr *)&(sock->conn), &conn_len);
      buf_size = buf_size + n;
    }
    on_recv_pkt(sock, pkt);  // calling function to handle the received packet, some logic to be implemented in this function
    free(pkt);
  }
  pthread_mutex_unlock(&(sock->recv_lock));
}



void *begin_backend(void *in) {
  foggy_socket_t *sock = (foggy_socket_t *)in;
  int death, buf_len, send_signal;
  uint8_t *data;

  while (1) {
    while (pthread_mutex_lock(&(sock->death_lock)) != 0) {
    }
    death = sock->dying;
    pthread_mutex_unlock(&(sock->death_lock));

    while (pthread_mutex_lock(&(sock->send_lock)) != 0) {
    }
    buf_len = sock->sending_len;

    if (!sock->send_window.empty()) {
      // printf("Sending window is not empty\n");
      send_pkts(sock, NULL, 0);
      check_for_pkt(sock, NO_WAIT);
    }

    if (death && buf_len == 0 && sock->send_window.empty()) { // when the three condition is true, then the socket is destroyed
      break;   
    }

    // Normal Work Flows
    if (buf_len > 0) {  // something in the data to send
      data = (uint8_t*)malloc(buf_len);
      memcpy(data, sock->sending_buf, buf_len); // copy the data to send

      // free the sending buffer and reset the sending length in socket
      sock->sending_len = 0;
      free(sock->sending_buf);
      sock->sending_buf = NULL;

      // unlock the sending lock, allow other process to send data
      pthread_mutex_unlock(&(sock->send_lock));
      send_pkts(sock, data, buf_len); // logic to send the data, need to be changed
      free(data);
    } else {
      pthread_mutex_unlock(&(sock->send_lock));
    }

    check_for_pkt(sock, NO_WAIT);

    while (pthread_mutex_lock(&(sock->recv_lock)) != 0) {
    }

    send_signal = sock->received_len > 0;

    pthread_mutex_unlock(&(sock->recv_lock));

    if (send_signal) {
      pthread_cond_signal(&(sock->wait_cond));
    }
  }

  pthread_exit(NULL);
  return NULL;
}


void foggy_listen(foggy_socket_t *sock) {
  if (sock->type != TCP_LISTENER) {
    perror("ERROR not a listener socket");
    return;
  }

  while (1) {
    check_for_pkt(sock, NO_FLAG);
  }
}