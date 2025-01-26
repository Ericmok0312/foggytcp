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
#include "grading.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define debug_printf(fmt, ...)                            \
  do {                                                    \
    if (DEBUG_PRINT) fprintf(stdout, fmt, ##__VA_ARGS__); \
  } while (0)

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
      debug_printf("Waiting for ack lock in has been acked\n");
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
    debug_printf("Waiting for mutex lock in check_for_pkt\n");
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
    //debug_printf("Current states connected: %d, death: %d \n", sock->connected, sock->dying);

    while (pthread_mutex_lock(&(sock->death_lock)) != 0) {
      debug_printf("Waiting for dying lock in begin backend\n");
    }
    death = sock->dying;

    pthread_mutex_unlock(&(sock->death_lock));
    
    // while(pthread_mutex_lock(&(sock->connected_lock)) != 0){
    //   debug_printf("waiting for conn lock\n");
    // }
    // pthread_mutex_unlock(&(sock->connected_lock));

    // if(sock->connected == 3 && death == 3){ // in the stage of waiting for FIN
    //   debug_printf("WAITING FOR FIN of other side\n");
    //   pthread_mutex_unlock(&(sock->connected_lock));
    //   check_for_pkt(sock, NO_WAIT);
    //   continue;
    // }

    // if(sock->connected == 4 && death == 0){
    //   pthread_mutex_unlock(&(sock->send_lock));
    //   send_pkts(sock, NULL, 0);
    //   check_for_pkt(sock, NO_WAIT);
    //   continue;
    // }




    // debug_printf("Backend running, dying %d\n", death);
    while (pthread_mutex_lock(&(sock->send_lock)) != 0) {
      debug_printf("Waiting for send lock in begin backend\n");
    }

    if(death == 2){  // counting down for dying
      //debug_printf("Waiting for countdown timer\n");
      if (check_time_out(sock)){
        debug_printf("Timer reached, closing socket\n");
        while(pthread_mutex_lock(&(sock->death_lock))!=0){
          debug_printf("Waiting for death lock in begin backend\n");
        }
        sock->dying = 1;
        pthread_mutex_unlock(&(sock->death_lock));
        pthread_mutex_unlock(&(sock->send_lock));
        continue;
      }
    }

    if(death == 3){
      //debug_printf("Waiting for FIN of otherside\n");
      pthread_mutex_unlock(&(sock->send_lock));
      send_pkts(sock, NULL, 0);
      check_for_pkt(sock, NO_WAIT);
      continue;
    }


    buf_len = sock->sending_len;

    if (!sock->send_window.empty()) {
      send_pkts(sock, NULL, 0);
      check_for_pkt(sock, NO_WAIT);
    }


    if (death == 1 && buf_len == 0 && sock->send_window.empty()) { // when the three condition is true, then the socket is destroyed
      debug_printf("Socket is dying\n");
      pthread_mutex_unlock(&(sock->send_lock));
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
      debug_printf("Waiting for recv lock in begin backend\n");
    }
    while(pthread_mutex_lock(&(sock->connected_lock)) != 0) {
      debug_printf("Waiting for connected lock in begin backend\n");
    }

    send_signal = sock->received_len > 0  || sock->connected == 4;

    pthread_mutex_unlock(&(sock->recv_lock));

    pthread_mutex_unlock(&(sock->connected_lock));
    

    if (send_signal) {
      // debug_printf("signaling\n");
      pthread_cond_signal(&(sock->wait_cond));
    }
    //debug_printf("Next loop\n");
  }
  debug_printf("Thread terminated\n");
  pthread_exit(NULL);
  return NULL;
}


void foggy_listen(foggy_socket_t *sock) {
  if (sock->type != TCP_LISTENER) {
    perror("ERROR not a listener socket");
    return;
  }
  
  debug_printf("Listening on port %d\n", ntohs(sock->conn.sin_port));

  while(pthread_mutex_lock(&(sock->connected_lock)) != 0) {  
    debug_printf("Waiting for conn lock in foggy listen\n");
  }

  while (sock->connected != 2) {
    check_for_pkt(sock, NO_FLAG);
    if(check_time_out(sock)){
        sock->window.next_seq_expected = 0;

        sock->connected = 0; // reset handshake

        sock->window.advertised_window = WINDOW_INITIAL_ADVERTISED; // updating advertised wi

        sock->window.last_byte_sent--;

        sock->window.timeout_timer = time(nullptr);
    }
  }

  pthread_mutex_unlock(&(sock->connected_lock)); // release the lock

  debug_printf("Connection established\n");
}



void foggy_connect(foggy_socket_t *sock) {
  if (sock->type != TCP_INITIATOR) {
    perror("ERROR not a initiator socket");
    return;
  }

  debug_printf("Connecting to port %d\n", ntohs(sock->conn.sin_port));

  while(pthread_mutex_lock(&(sock->death_lock)) != 0) {  
  }

  debug_printf("Sending SYN packet %d\n", sock->window.last_byte_sent);
  
  uint8_t *syn_pkt = create_packet(
                  sock->my_port, ntohs(sock->conn.sin_port),
                  sock->window.last_byte_sent, sock->window.next_seq_expected,
                  sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), SYN_FLAG_MASK,
                  MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0,
                  NULL, NULL, 0);

  sendto(sock->socket, syn_pkt, sizeof(foggy_tcp_header_t), 0,
                    (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // sending syn packet, currently no timeout

  

  reset_time_out(sock);

  while(pthread_mutex_lock(&(sock->connected_lock)) != 0) {  
    debug_printf("Waiting for conn lock in foggy connect\n");
  }


  while (sock->connected != 2) {
    check_for_pkt(sock, NO_FLAG);
    if(check_time_out(sock)){
      sendto(sock->socket, syn_pkt, sizeof(foggy_tcp_header_t), 0,
                    (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // sending syn packet, currently no timeout
      reset_time_out(sock);
    }
  }

  free(syn_pkt); // prevent leakage

  sock->window.last_byte_sent++; // update the last byte sent
  

  pthread_mutex_unlock(&(sock->connected_lock));
  pthread_mutex_unlock(&(sock->death_lock)); // release the lock

  
  debug_printf("Connection established\n");
}