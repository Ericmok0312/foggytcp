/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */

#include <deque>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "foggy_function.h"
#include "foggy_backend.h"
#include <unistd.h>
#include <thread>
#include "grading.h"
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define debug_printf(fmt, ...)                            \
  do {                                                    \
    if (DEBUG_PRINT) fprintf(stdout, fmt, ##__VA_ARGS__); \
  } while (0)


/**
 * Updates the socket information to represent the newly received packet.
 *
 * In the current stop-and-wait implementation, this function also sends an
 * acknowledgement for the packet.
 *
 * @param sock The socket used for handling packets received.
 * @param pkt The packet data received by the socket.
 */
void on_recv_pkt(foggy_socket_t *sock, uint8_t *pkt) {
  debug_printf("Received packet\n");
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)pkt;
  uint8_t flags = get_flags(hdr);
  switch (flags) {
      case SYN_FLAG_MASK: {
          debug_printf("Receive SYN %u, sending Seq %u \n", get_seq(hdr), sock->window.last_byte_sent);

          // Update next_seq_expected for the first connection
          sock->window.next_seq_expected = get_seq(hdr) + 1;

          sock->connected = 1; // inidcate first handshaking done

          // Send SYN-ACK
          uint8_t *syn_ack_pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent,  // Telling the client that we are ready to receive, and the initial seq number
              get_seq(hdr) + 1,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), SYN_FLAG_MASK | ACK_FLAG_MASK,
              MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL, NULL, 0);
          sendto(sock->socket, syn_ack_pkt, sizeof(foggy_tcp_header_t), 0,
                (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
          free(syn_ack_pkt);
          break;
      }
      case (SYN_FLAG_MASK | ACK_FLAG_MASK): {
          debug_printf("Receive SYN-ACK %u-%u, sending ACK %u \n", get_seq(hdr), get_ack(hdr), get_seq(hdr) + 1);

          // Update next_seq_expected for the first connection
          sock->window.next_seq_expected = get_seq(hdr) + 1;
          sock->window.last_ack_received = get_ack(hdr); // update ack

          sock->connected = 2; // handshaking done, initiater side only need to confirm once

          // Adding any possible data to receive window
          add_receive_window(sock, pkt);
          process_receive_window(sock);
        
          // Send SYN-ACK
          uint8_t *syn_ack_pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent,  // Telling the client that we are ready to receive, and the initial seq number
              get_seq(hdr) + 1,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL, NULL, 0);
          sendto(sock->socket, syn_ack_pkt, sizeof(foggy_tcp_header_t), 0,
                (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
          free(syn_ack_pkt);
          break;
      }
      case FIN_FLAG_MASK: {
          debug_printf("Receive FIN %u \n", get_seq(hdr));
          uint8_t *fin_ack_pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent,    // Acknowledge the FIN packet
              get_seq(hdr) + 1,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL, NULL, 0);
          sendto(sock->socket, fin_ack_pkt, sizeof(foggy_tcp_header_t), 0,
                (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
          free(fin_ack_pkt);

          debug_printf("Sending FIN-ACK %u\n", get_seq(hdr) + 1);


          // Update the state to indicate that the connection is closing
          while (pthread_mutex_lock(&(sock->connected_lock)) != 0) {
          }
          sock->connected = 4;
          pthread_mutex_unlock(&(sock->connected_lock));
          debug_printf("Setting connected to 4\n");
          break;
      }
      case ACK_FLAG_MASK: {  // NEED TO CHANGE THIS
          uint32_t ack = get_ack(hdr);
          // TODO: change here to implement sliding window

          // if (get_payload_len(pkt) == 0) handle_congestion_window(sock, pkt);
          sock->window.advertised_window = get_advertised_window(hdr); //getting the amount of data the receiver can accept


          if (after(ack, sock->window.last_ack_received)) { 
              sock->window.last_ack_received = ack;                           
              if(sock->connected == 3) {
                  while(pthread_mutex_lock(&(sock->death_lock)) != 0){      
                  } 
                  sock->dying = 1;
                  pthread_mutex_unlock(&(sock->death_lock));
                  debug_printf("Receive FIN-ACK %u\n", get_ack(hdr));
                  sock->window.dup_ack_count = 0;
              }
              else{
                   debug_printf("Receive ACK %u\n", get_ack(hdr));
                   sock->window.dup_ack_count = 0;
              }
          }
          else{
            if(!sock->send_window.empty()){
              sock->window.dup_ack_count++;
            }
            if(sock->window.dup_ack_count == 3){
              resend(sock);
            }
          }

          if(sock->connected == 1) {
              sock->connected = 2; // connection established
          }
      } 
      // Fallthrough
      default: {
          if (get_payload_len(pkt) > 0) {
              debug_printf("Received data packet %u %u\n", get_seq(hdr),
                          get_seq(hdr) + get_payload_len(pkt));

              sock->window.advertised_window = get_advertised_window(hdr);
              // Add the packet to receive window and process receive window
              add_receive_window(sock, pkt); 
              process_receive_window(sock);
              // Send ACK
              debug_printf("Sending ACK packet %u\n", sock->window.next_seq_expected);

              uint8_t *ack_pkt = create_packet(
                  sock->my_port, ntohs(sock->conn.sin_port),
                  sock->window.last_byte_sent, sock->window.next_seq_expected,
                  sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
                  MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0,
                  NULL, NULL, 0);
              sendto(sock->socket, ack_pkt, sizeof(foggy_tcp_header_t), 0,
                    (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
              free(ack_pkt);
          }
          break;
      }
  }
}




/**
 * Breaks up the data into packets and sends a single packet at a time.
 *
 * You should most certainly update this function in your implementation.
 *
 * @param sock The socket to use for sending data.
 * @param data The data to be sent.
 * @param buf_len The length of the data being sent.
 */
void send_pkts(foggy_socket_t *sock, uint8_t *data, int buf_len, int flags) {
  uint8_t *data_offset = data;
  transmit_send_window(sock);

  if (buf_len > 0) {
    while (buf_len != 0) {
      uint16_t payload_len = MIN(buf_len, (int)MSS);

      send_window_slot_t slot;
      slot.is_sent = 0;
      slot.msg = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + payload_len,
          flags,
          MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL,
          data_offset, payload_len);
      sock->send_window.push_back(slot);

      buf_len -= payload_len;
      data_offset += payload_len;
      sock->window.last_byte_sent += payload_len;
    }
  } else if (flags == FIN_FLAG_MASK) {
    debug_printf("Sending FIN %u\n", sock->window.last_byte_sent);
    send_window_slot_t slot;
    slot.is_sent = 0;
    slot.msg = create_packet(
        sock->my_port, ntohs(sock->conn.sin_port),
        sock->window.last_byte_sent, sock->window.next_seq_expected,
        sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t),
        FIN_FLAG_MASK,
        MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL,
        NULL, 0);
    sock->send_window.push_back(slot);
    sock->window.last_byte_sent += 1;
    while(pthread_mutex_lock(&(sock->connected_lock))!=0){      
    }
    sock->connected = 3;
    pthread_mutex_unlock(&(sock->connected_lock));
  }

  receive_send_window(sock);
}





void add_receive_window(foggy_socket_t *sock, uint8_t *pkt) {

  if(sock->receive_window.size() == RECEIVE_WINDOW_SLOT_SIZE) return;

  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)pkt;

  // Stop-and-wait implementation
  // Insert the packet into the first place of receive window
  // receive_window_slot_t *cur_slot = &(sock->receive_window[0]);
  // if (cur_slot->is_used == 0) {
  //   cur_slot->is_used = 1;
  //   cur_slot->msg = (uint8_t*) malloc(get_plen(hdr));
  //   memcpy(cur_slot->msg, pkt, get_plen(hdr));
  // }


  receive_window_slot_t temp;
  temp.msg = (uint8_t*) malloc(get_plen(hdr));
  temp.is_used = 1;
  memcpy(temp.msg, pkt, get_plen(hdr));

  sock->receive_window.push(move(temp));

}

void process_receive_window(foggy_socket_t *sock) {
  // Stop-and-wait implementation.
  // Only process the first packet in the window.
  const receive_window_slot_t& cur_slot = sock->receive_window.top();


  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)cur_slot.msg;
  // Discard unexpected packet
  if (get_seq(hdr) != sock->window.next_seq_expected) return;
  // Update next seq number expected
  uint16_t payload_len = get_payload_len(cur_slot.msg);
  sock->window.next_seq_expected += payload_len;
  // Copy to received_buf
  sock->received_buf = (uint8_t*)
      realloc(sock->received_buf, sock->received_len + payload_len);

  memcpy(sock->received_buf + sock->received_len, get_payload(cur_slot.msg),
          payload_len);
          
  sock->received_len += payload_len;
  // Free the slot
  free(cur_slot.msg); 
  sock->receive_window.pop();

  // if (cur_slot->is_used != 0) {
  //   foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)cur_slot->msg;
  //   // Discard unexpected packet
  //   if (get_seq(hdr) != sock->window.next_seq_expected) return;
  //   // Update next seq number expected
  //   uint16_t payload_len = get_payload_len(cur_slot->msg);
  //   sock->window.next_seq_expected += payload_len;
  //   // Copy to received_buf
  //   sock->received_buf = (uint8_t*)
  //       realloc(sock->received_buf, sock->received_len + payload_len);
  //   memcpy(sock->received_buf + sock->received_len, get_payload(cur_slot->msg),
  //          payload_len);
  //   sock->received_len += payload_len;
  //   // Free the slot
  //   cur_slot->is_used = 0;
  //   free(cur_slot->msg);
  //   cur_slot->msg = NULL;
  // }
}

//TODO: implement sliding window

void transmit_send_window(foggy_socket_t *sock) {
  // Check if there are no new packets to process
  if (sock->send_window.empty() || sock->window.last_sent_pos == sock->send_window.size() - 1) {
    return;
  }

  // Process the send window
  while (true) {
    send_window_slot_t &next_slot = sock->send_window[sock->window.last_sent_pos + 1];
    uint16_t payload_len = get_payload_len(next_slot.msg);

    if (sock->window.window_used + payload_len > sock->window.congestion_window ||
        sock->window.advertised_window < payload_len) {
          debug_printf("either reach congestion window limit or advertised window limit, values: %u,  %u\n", sock->window.window_used,   sock->window.advertised_window);
      break;
    }

    // Update the window used size and advertised window size
    sock->window.window_used += payload_len;
    sock->window.advertised_window -= payload_len;

    // Get the current slot and header
    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)next_slot.msg;

    // Print debug information
    debug_printf("Sending packet %u %u\n", get_seq(hdr), get_seq(hdr) + payload_len);

    // Mark the slot as sent and send the packet
    next_slot.is_sent = 1;
    sendto(sock->socket, next_slot.msg, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));

    // Update the last sent position
    sock->window.last_sent_pos++;

    // Check if we have reached the end of the window to process
    if (sock->window.last_sent_pos == sock->send_window.size() - 1) {
      break;
    }
  }
}

void receive_send_window(foggy_socket_t *sock) {
  // Pop out the packets that have been ACKed
  while (1) {
    if (sock->send_window.empty()) break;

    send_window_slot_t slot = sock->send_window.front();
    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)slot.msg;

    if (slot.is_sent == 0) {
      break;
    }
    if (has_been_acked(sock, get_seq(hdr)) == 0) {
      debug_printf("Seq waiting for ack is %u, but last ack received is %u \n", get_seq(hdr), sock->window.last_ack_received);
      break;
    }

    sock->send_window.pop_front();
    sock->window.last_sent_pos--;
    sock->window.window_used -= get_payload_len(slot.msg);
    free(slot.msg);
  }
}

void resend(foggy_socket_t *sock){
  send_window_slot_t &packet = sock->send_window.front();
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)packet.msg;
  debug_printf("Trigger resend, sending seq %d\n", get_seq(hdr));
  sendto(sock->socket, packet.msg, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
  sock->window.dup_ack_count == 0;
}