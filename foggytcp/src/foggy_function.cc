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
 * recv_lock locked in check_for_pkt
 *
 * locking connected lock and death lock and connected lock
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

          sock->window.advertised_window = get_advertised_window(hdr); // updating advertised window
          debug_printf("Setting advertised window to %d\n", sock->window.advertised_window);
          // Send SYN-ACK
          uint8_t *syn_ack_pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent,  // Telling the client that we are ready to receive, and the initial seq number
              get_seq(hdr) + 1,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), SYN_FLAG_MASK | ACK_FLAG_MASK,
              MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL, NULL, 0);
          sendto(sock->socket, syn_ack_pkt, sizeof(foggy_tcp_header_t), 0,
                (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
          
          sock->window.last_byte_sent++; // update the last byte sent
          reset_time_out(sock); // start counting timeout
          free(syn_ack_pkt);
          break;
      }
      case (SYN_FLAG_MASK | ACK_FLAG_MASK): {
          debug_printf("Receive SYN-ACK %u-%u, sending ACK %u \n", get_seq(hdr), get_ack(hdr), get_seq(hdr) + 1);

          // Update next_seq_expected for the first connection
          sock->window.next_seq_expected = get_seq(hdr) + 1;
          sock->window.last_ack_received = get_ack(hdr); // update ack
          sock->window.advertised_window = get_advertised_window(hdr); // updating advertised window
          debug_printf("Setting advertised window to %d\n", sock->window.advertised_window);
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
            debug_printf("Waiting for conn lock in on recv pkt\n");
          } 
          if(sock->connected == 2){
            sock->connected = 4;
            debug_printf("Setting connected to 4\n");
          }
          else if(sock->connected == 3){
            sock->connected = 0;
            debug_printf("Setting connected to 0\n");
          }
          pthread_mutex_unlock(&(sock->connected_lock));
          while (pthread_mutex_lock(&(sock->death_lock)) != 0){
            debug_printf("Waiting for death lock in on recv pkt\n");
          }
          if(sock->dying == 3){ // client receiving FIN-ACK from server
            sock->dying = 2;  // entering the stage for counting down
            reset_time_out(sock);
            debug_printf("Setting dying to 2\n");
            debug_printf("Setting terminate socket timer to %ld \n", sock->window.timeout_timer);
          }
          pthread_mutex_unlock(&(sock->death_lock));
          break;
      }
      case ACK_FLAG_MASK: {  // NEED TO CHANGE THIS
          uint32_t ack = get_ack(hdr);
          // TODO: change here to implement sliding window
          // if (get_payload_len(pkt) == 0) handle_congestion_window(sock, pkt);
          sock->window.advertised_window = get_advertised_window(hdr); //getting the amount of data the receiver can accept
          debug_printf("Setting advertised window to %d\n", sock->window.advertised_window);
          // while (pthread_mutex_lock(&(sock->window.ack_lock)) != 0)
          // {
          //   debug_printf("Waiting for ack lock in on recv pkt\n");
          // }
          if (after(ack, sock->window.last_ack_received)) { 
              sock->window.last_ack_received = ack;                           
              if(sock->connected == 3) {
                  while(pthread_mutex_lock(&(sock->death_lock)) != 0){
                    debug_printf("Waiting for death lock in on recv pkt, ack\n");
                  } 

                  if(sock->dying != 3){
                    sock->dying = 2; // dying 3, indicating the state for only receiving packets
                  }
                  else{
                    sock->dying = 1;  // new
                  }
                  pthread_mutex_unlock(&(sock->death_lock));
                  debug_printf("Receive FIN-ACK %u\n", get_ack(hdr));
                  sock->window.timeout_timer = time(nullptr);
                  sock->window.dup_ack_count = 0;
              }
              else if((sock->connected == 4 && sock->dying == 3)){
                while(pthread_mutex_lock(&(sock->death_lock)) != 0){
                }
                sock->dying = 1; // immediate close
                sock->connected = 0;
                pthread_mutex_unlock(&(sock->death_lock));
                reset_time_out(sock);
                debug_printf("Receive FIN-ACK %u\n", get_ack(hdr));
              }
              else{
                  debug_printf("Receive ACK %u\n", get_ack(hdr));
                  sock->window.dup_ack_count = 0;
              }
          }
          else if (ack==sock->window.last_ack_received){
            if(!sock->send_window.empty()){
              sock->window.dup_ack_count++;
              debug_printf("Duplicate ACK +1\n");
            }
            if(sock->window.dup_ack_count == 3){
              debug_printf("Duplicate ACK for 3 Times\n");
              resend(sock);
            }
          }
          // pthread_mutex_unlock(&(sock->window.ack_lock));
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
              debug_printf("Setting advertised_window to %ds\n", sock->window.advertised_window);
              // Add the packet to receive window and process receive window
              bool send_ack = check_send_ack(sock, pkt);

              add_receive_window(sock, pkt); 
              process_receive_window(sock);
              // Send ACK
              if(send_ack){
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
    debug_printf("Start Send FIN %u\n", sock->window.last_byte_sent);
    send_window_slot_t slot;
    slot.is_sent = 0;
    slot.msg = create_packet(
        sock->my_port, ntohs(sock->conn.sin_port),
        sock->window.last_byte_sent, sock->window.next_seq_expected,
        sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t),
        FIN_FLAG_MASK,
        MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL,
        NULL, 0);

    sock->send_window.emplace_back(move(slot));
    //sendto(sock->socket, slot.msg, get_plen((foggy_tcp_header_t*)slot.msg), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
    sock->window.last_byte_sent += 1;
    // free(slot.msg);
    while(pthread_mutex_lock(&(sock->connected_lock))!=0){      
      debug_printf("waiting connected_lock in send pkts\n");
    }
    while(pthread_mutex_lock(&(sock->death_lock)) != 0){
    }
    if(sock->connected != 4){
      sock->connected = 3;
      sock->dying = 3;
    }
    else{
      sock->dying = 2;
    }
    pthread_mutex_unlock(&(sock->connected_lock));
    pthread_mutex_unlock(&(sock->death_lock));
    debug_printf("Sended FIN %u\n", sock->window.last_byte_sent);
  }
  receive_send_window(sock);
}



void add_receive_window(foggy_socket_t *sock, uint8_t *pkt) {
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)pkt;
  uint32_t p_seq = get_seq(hdr);
  uint32_t p_end = p_seq + get_payload_len(pkt);
  if(get_payload_len(pkt) == 0){
    return;
  }
  // Check if the packet is within the receive window
  if (p_seq < sock->window.next_seq_expected || p_end > sock->window.next_seq_expected + MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS)) {
    debug_printf("Packets not in receive window seq: %d\n", p_seq);
    return; // packet not in receive window
  }

  sock->window.received_pkt.insert(p_seq);

  // Handle in-order packet
  if (p_seq == sock->window.next_seq_expected) {
    receive_window_slot_t *cur_slot = &(sock->receive_window[sock->window.receive_window_end_ptr]);
    if (cur_slot->is_used == 0) {
      cur_slot->is_used = 1;
      cur_slot->msg = (uint8_t*) malloc(get_plen(hdr));
      memcpy(cur_slot->msg, pkt, get_plen(hdr));
      sock->window.receive_window_end_ptr = (sock->window.receive_window_end_ptr + 1) % RECEIVE_WINDOW_SLOT_SIZE;
    } else {
      debug_printf("Error: Slot already used for in-order packet. Process_receive_window efficiency not enough\n");
      receive_window_slot_t temp;
      temp.msg = (uint8_t*) malloc(get_plen(hdr));
      temp.is_used = 1;
      memcpy(temp.msg, pkt, get_plen(hdr));
      sock->out_of_order_queue[p_seq] = temp.msg;
    }

    uint32_t cur_recv = sock->window.next_seq_expected + get_payload_len(pkt);
    uint8_t* non_seq_slot = nullptr;
    while (!sock->out_of_order_queue.empty()) {
      cur_slot = &(sock->receive_window[sock->window.receive_window_end_ptr]);
      non_seq_slot = sock->out_of_order_queue.begin()->second;
      uint32_t seq = sock->out_of_order_queue.begin()->first;
      uint16_t payload_len = get_payload_len(non_seq_slot);
      if (cur_slot->is_used == 0) {
        if(seq == cur_recv){
          cur_slot->is_used = 1;
          cur_slot->msg = non_seq_slot;
          cur_recv += payload_len;
          sock->window.receive_window_end_ptr = (sock->window.receive_window_end_ptr + 1) % RECEIVE_WINDOW_SLOT_SIZE;
        } else {
          debug_printf("Duplicate packet received\n");
        }
      } else {
        debug_printf("Error: Slot already used for in-order packet. Process_receive_window efficiency not enough\n");
        break;
      }
      sock->out_of_order_queue.erase(seq);
    }
  } else { // Handle out-of-order packet
    debug_printf("Non-sequential packet seq: %d\n", p_seq);
    receive_window_slot_t temp;
    temp.msg = (uint8_t*) malloc(get_plen(hdr));
    temp.is_used = 1;
    memcpy(temp.msg, pkt, get_plen(hdr));
    sock->out_of_order_queue[p_seq] = temp.msg;
  }

  debug_printf("Number of packets in seq receive window and in non seq window : %d, %d\n", (sock->window.receive_window_end_ptr - sock->window.receive_window_start_ptr + RECEIVE_WINDOW_SLOT_SIZE) % RECEIVE_WINDOW_SLOT_SIZE, sock->out_of_order_queue.size());
}

void process_receive_window(foggy_socket_t *sock) {
  receive_window_slot_t* cur_slot = nullptr;

  while (true) {
    cur_slot = &(sock->receive_window[sock->window.receive_window_start_ptr]);

    if (cur_slot->is_used == 0) return;

    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)cur_slot->msg;
    if (get_seq(hdr) != sock->window.next_seq_expected) {
      return;
    }

    uint16_t payload_len = get_payload_len(cur_slot->msg);
    sock->window.next_seq_expected += payload_len;

    debug_printf("Setting next_seq_expected to: %ld\n", sock->window.next_seq_expected);

    sock->received_buf = (uint8_t*) realloc(sock->received_buf, sock->received_len + payload_len);
    memcpy(sock->received_buf + sock->received_len, get_payload(cur_slot->msg), payload_len);
    sock->received_len += payload_len;

    sock->window.received_pkt.erase(get_seq(hdr));
    free(cur_slot->msg);
    cur_slot->msg = nullptr;
    cur_slot->is_used = 0;

    sock->window.receive_window_start_ptr = (sock->window.receive_window_start_ptr + 1) % RECEIVE_WINDOW_SLOT_SIZE;

    while (!sock->out_of_order_queue.empty()) {
      uint32_t seq = sock->out_of_order_queue.begin()->first;
      if (seq == sock->window.next_seq_expected) {
        uint8_t* non_seq_slot = sock->out_of_order_queue.begin()->second;
        uint16_t payload_len = get_payload_len(non_seq_slot);
        sock->received_buf = (uint8_t*) realloc(sock->received_buf, sock->received_len + payload_len);
        memcpy(sock->received_buf + sock->received_len, get_payload(non_seq_slot), payload_len);
        sock->received_len += payload_len;
        sock->window.next_seq_expected += payload_len;
        free(non_seq_slot);
        sock->out_of_order_queue.erase(seq);
      } else {
        break;
      }
    }

    if (sock->window.receive_window_start_ptr == sock->window.receive_window_end_ptr) {
      break;
    }
  }
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

    if (sock->window.window_used + payload_len > sock->window.congestion_window){
      debug_printf("Reach congestion window limit\n");
      break;
    }
    if(sock->window.advertised_window < payload_len) {
      debug_printf("Reach advertised window limit\n");
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


  debug_printf("# Packets in send window %d\n", sock->window.last_sent_pos+1);
  

}


void receive_send_window(foggy_socket_t *sock) {
  //debug_printf("Called here\n");

  // Timeout resend
  if (check_time_out(sock) && !sock->send_window.empty()) {
    debug_printf("Timeout reached, resending packet %u\n", get_seq((foggy_tcp_header_t*)sock->send_window.front().msg));
    resend(sock);
    return;
  }
  

  // Pop out the packets that have been ACKed
  bool checked = false;
  while (1) {
    if (sock->send_window.empty()) break;

    send_window_slot_t slot = sock->send_window.front();
    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)slot.msg;

    if (slot.is_sent == 0 || slot.msg == nullptr) {
      break;
    }
    if (has_been_acked(sock, get_seq(hdr)) == 0) {
      //debug_printf("Seq waiting for ack is %u, but last ack received is %u , duplicate ACK count value: %d\n", get_seq(hdr), sock->window.last_ack_received, sock->window.dup_ack_count);
      break;
    }

    sock->send_window.pop_front();
    sock->window.last_sent_pos--;
    sock->window.window_used -= get_payload_len(slot.msg);
    free(slot.msg);
    slot.msg = nullptr;
    checked = true;
  }

  if(checked){
    reset_time_out(sock);
  }

  //debug_printf("packets waiting for ACK %d \n", sock->window.last_sent_pos+1);
}

inline void resend(foggy_socket_t *sock){
  send_window_slot_t &packet = sock->send_window.front();
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)packet.msg;
  debug_printf("Trigger resend, sending seq %d\n", get_seq(hdr));
  sendto(sock->socket, packet.msg, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
  sock->window.dup_ack_count = 0;
  reset_time_out(sock);
}

inline void reset_time_out(foggy_socket_t *sock){
  auto now = std::chrono::system_clock::now();
  auto new_time = now + std::chrono::seconds(1);
  sock->window.timeout_timer = std::chrono::system_clock::to_time_t(new_time); // Update the send time to current time, might need to change later
  debug_printf("Setting timeout to %ld \n", sock->window.timeout_timer);
}

inline bool check_time_out(foggy_socket_t *sock){
  auto now = std::chrono::system_clock::now();
  time_t current_time = std::chrono::system_clock::to_time_t(now);
  if(sock->window.timeout_timer != time(nullptr) && sock->window.timeout_timer <= current_time ){
    debug_printf("Time out \n");
    return true;
  }
  else{
    return false;
  }
}

inline void reset_ack_time_out(foggy_socket_t *sock){
  auto now = std::chrono::system_clock::now();
  auto new_time = now + std::chrono::seconds(1);
  sock->window.ack_timeout = std::chrono::system_clock::to_time_t(new_time); // Update the send time to current time, might need to change later
  debug_printf("Setting ack timeout to %ld \n", sock->window.ack_timeout);
}

inline bool check_ack_time_out(foggy_socket_t *sock){
  auto now = std::chrono::system_clock::now();
  time_t current_time = std::chrono::system_clock::to_time_t(now);
  if(sock->window.ack_timeout != time(nullptr) && sock->window.ack_timeout <= current_time ){
    debug_printf("Time out \n");
    return true;
  }
  else{
    return false;
  }
}


inline bool check_send_ack(foggy_socket_t* sock, uint8_t* pkt){
  uint32_t seq = get_seq((foggy_tcp_header_t*)pkt);
  if(sock->window.next_seq_expected > seq){
    debug_printf("Packets received again\n");
    return false;
  }
  if(sock->window.next_seq_expected == seq){
    if(sock->window.send_ack_state == NO_PREV_ACK_WAIT){
      sock->window.send_ack_state = SEQ_PREV_PKT_NOT_ACK;
      reset_ack_time_out(sock);
      debug_printf("Sequential arrive, wait for next pkt to ack\n");
      return false;
    }
    else if(sock->window.send_ack_state == SEQ_PREV_PKT_NOT_ACK){
      sock->window.send_ack_state = NO_PREV_ACK_WAIT;
      debug_printf("Sequential arrive, send immediately due to prev not-acked pkts\n");
      sock->window.ack_timeout = time(nullptr);
      return true;
    }
    else if(sock->window.send_ack_state == NOT_SEQ){
      if(sock->out_of_order_queue.begin()->first == seq + get_payload_len(pkt)){ // gap fully filled
        sock->window.send_ack_state = NO_PREV_ACK_WAIT;
        debug_printf("Sequential arrive and fill all gaps, send ack immediately\n");
      }
      else{
        debug_printf("Sequential arrive but not fill all gaps\n");
        sock->window.send_ack_state = NOT_SEQ;
      }
      return true;
    }
  }
  else{
    debug_printf("Not sequential arrive so send ack immediately\n");
    sock->window.send_ack_state = NOT_SEQ;
    return true;
  }

}

