#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>
using namespace std;

#include "foggy_tcp.h"
#include "foggy_function.h"
#include "foggy_backend.h"
#include "grading.h"


#define debug_printf(fmt, ...)                            \
  do {                                                    \
    if (DEBUG_PRINT) fprintf(stdout, fmt, ##__VA_ARGS__); \
  } while (0)
  
#define BUF_SIZE 4096

int main(int argc, const char* argv[]) {
  if (argc != 5) {
    cerr << "Usage: " << argv[0] << " <server-ip> <server-port> <filename> <testcase number>\n";
    return -1;
  }

  const char* server_ip = argv[1];
  const char* server_port = argv[2];
  const char* filename = argv[3];
  int test_number = atoi(argv[4]);


  switch(test_number){
    case 1:{ 
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);
        if(sock->window.last_byte_sent == sock->window.last_ack_received && sock->connected == 2){
            printf("Pass\n");
        }
        sock->dying = 1;
        foggy_close(sock);
        break;
    }
    case 2:{ 
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, false);
        char buffer[BUF_SIZE];
        memset(buffer, 'A', BUF_SIZE);

        // Manually set an incorrect sequence number
        sock->window.last_byte_sent-=1;
        uint32_t original_last_ack_recv  = sock->window.last_ack_received;



        // Send the packet with the incorrect sequence number
        foggy_write(sock, buffer, BUF_SIZE);
        sleep(1);

        // Check if the server responds with an error or handles the incorrect sequence number
        for (int i = 0; i < 5; ++i) {
            if (sock->window.last_ack_received != original_last_ack_recv) {
            printf("Failed\n");
            break;
            } else {
            printf("Waiting for server response...\n");
            sleep(1); // Wait for 1 second before retrying
            }
        }
        if (sock->window.last_ack_received == original_last_ack_recv) { //last ack received has not changed
            printf("Pass\n");
        }
        break;
    }
    case 3:
    {
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, false);
        char buffer[BUF_SIZE];
        memset(buffer, 'A', BUF_SIZE);

        uint32_t original_ack = sock->window.last_ack_received;

        foggy_write(sock, buffer, BUF_SIZE);
        sleep(1);
        for (int i = 0; i < 2; ++i) {
            sleep(1);
        }

        if(original_ack + 4096 == sock->window.last_ack_received){
            printf("Pass\n");
        }
        break;
    }
    case 4:
    {
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);

        uint32_t original_advertise_window_size = sock->window.advertised_window;

        uint32_t original_last_byte_sent = sock->window.last_byte_sent;

        while(sock->window.advertised_window == original_advertise_window_size){
            check_for_pkt(sock, NO_WAIT);
        }

        if(sock->window.advertised_window != 2000){ // Can't Get correct advertised window
            printf("Failed \n");
            return 0;
        }

        char buffer[MSS];

        memset(buffer, 'A', MSS);

        send_pkts(sock, (uint8_t*)buffer, MSS); // can send

        send_pkts(sock, (uint8_t*)buffer, MSS); // can't send

        if(sock->send_window[0].is_sent != 1 || sock->send_window[1].is_sent !=  0){
            printf("Failed 1\n");
            break;
        }
        
        check_for_pkt(sock, NO_FLAG); // ACK for first pkt

        send_pkts(sock, nullptr, 0); //just for process send window

        if(sock->send_window[0].is_sent == 0){
          printf("Failed 2\n");
        }
        else{
          printf("Pass \n");
        }
        
        break;
    }
    case 5:
    {
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);

        sock->window.congestion_window = 10*MSS; //ensures no effect on congestion window
        
        char buffer[MSS];

        uint32_t initial_seq = sock->window.last_byte_sent;

        memset(buffer, 'A', MSS);

        uint8_t* pkt1 = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + MSS,
          ACK_FLAG_MASK,
          MAX(MAX_NETWORK_BUFFER - (uint16_t)sock->received_len, (uint16_t)MSS), 0, NULL,
          (uint8_t*)buffer, MSS);
        
        
        foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)(pkt1);
        sendto(sock->socket, pkt1, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));

        
        check_for_pkt(sock, NO_FLAG);
        
        if(sock->window.last_ack_received != initial_seq + MSS){
            printf("Failed \n");
            return 0;
        }
       

        uint8_t* pkt2 = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent + MSS + sock->window.advertised_window + 1, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + MSS,
          ACK_FLAG_MASK,
          MAX(MAX_NETWORK_BUFFER - (uint16_t)sock->received_len, (uint16_t)MSS), 0, NULL,
          (uint8_t*)buffer, MSS);

        hdr = (foggy_tcp_header_t *)(pkt2);
        sendto(sock->socket, pkt2, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));

        check_for_pkt(sock, NO_FLAG);

        if(sock->window.last_ack_received == initial_seq + 2*MSS + MAX_NETWORK_BUFFER + 1){
            printf("Failed \n");
            return 0;
        }


        uint8_t* pkt2_1 = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent + sock->window.advertised_window, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + MSS,
          ACK_FLAG_MASK,
          MAX(MAX_NETWORK_BUFFER - (uint16_t)sock->received_len, (uint16_t)MSS), 0, NULL,
          (uint8_t*)buffer, MSS);

        hdr = (foggy_tcp_header_t *)(pkt2_1);
        sendto(sock->socket, pkt2_1, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));

        check_for_pkt(sock, NO_FLAG);

        if(sock->window.last_ack_received == initial_seq + 2*MSS + MAX_NETWORK_BUFFER + 1){
            printf("Failed \n");
            return 0;
        }

        uint8_t* pkt3 = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent + 2*MSS, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + MSS,
          ACK_FLAG_MASK,
          MAX(MAX_NETWORK_BUFFER - (uint16_t)sock->received_len, (uint16_t)MSS), 0, NULL,
          (uint8_t*)buffer, MSS);

        hdr = (foggy_tcp_header_t *)(pkt3);
        sendto(sock->socket, pkt3, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));

        check_for_pkt(sock, NO_FLAG);

        if(sock->window.last_ack_received == initial_seq + MSS + 10){
            printf("Failed \n");
            return 0;
        }


        uint8_t* pkt4 = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent + MSS, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + MSS,
          ACK_FLAG_MASK,
          MAX(MAX_NETWORK_BUFFER - (uint16_t)sock->received_len, (uint16_t)MSS), 0, NULL,
          (uint8_t*)buffer, MSS);

        hdr = (foggy_tcp_header_t *)(pkt4);
        sendto(sock->socket, pkt4, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));

        check_for_pkt(sock, NO_FLAG);

        if(sock->window.last_ack_received == initial_seq + 3*MSS){
            printf("Pass \n");
        }
        break;
    }
    case 6:{
      foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);

      char buffer[MSS];
      memset(buffer, 'A', MSS);
      
      uint32_t last_ack_recv = sock->window.last_ack_received;


      if(sock->window.dup_ack_count != 0){
        printf("Failed Setting Initial ACK Count\n");
        return 0;
      }

      send_pkts(sock, (uint8_t*)buffer, MSS);
      send_pkts(sock, nullptr, 0);

      check_for_pkt(sock, NO_FLAG); // recv first dup ack;
      send_pkts(sock, nullptr, 0);  // no resend

      if(sock->window.dup_ack_count != 1){
        printf("Failed \n");
        return 0;
      }

      check_for_pkt(sock, NO_FLAG); // recv first dup ack;
      send_pkts(sock, nullptr, 0); // no resend

      if(sock->window.dup_ack_count != 2){
        printf("Failed \n");
        return 0;
      }

      check_for_pkt(sock, NO_FLAG); // recv first dup ack;
      send_pkts(sock, nullptr, 0); // should trigger resend

      if(sock->window.dup_ack_count != 3){
        printf("Failed \n");
        return 0;
      }


      check_for_pkt(sock, NO_FLAG); // recv ack now

      if(sock->window.last_ack_received != last_ack_recv + MSS || sock->window.dup_ack_count != 0){
        printf("Fail \n");
        return 0;
      }
      
      check_for_pkt(sock, NO_FLAG);

      if(sock->window.dup_ack_count != 1){
        printf("Fail \n");
      }
      else{
        printf("Pass \n");
      }
      break;
    }
    case 7:{
      foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);
      
      if(sock->window.reno_state != RENO_SLOW_START){
        printf("Fail \n");
        return 0;
      }
      char buffer[MSS];
      memset(buffer, 'A', MSS);

      uint32_t init_con = sock->window.congestion_window;

      sock->window.congestion_window = MSS-1; // setting to just 1 byte less than pkt

      send_pkts(sock, (uint8_t*)buffer, MSS);
      send_pkts(sock, nullptr, 0);

      if(sock->send_window[0].is_sent == 1){
        printf("Fail \n");
      }

      sock->window.congestion_window = MSS;

      send_pkts(sock, nullptr, 0);

      if(sock->send_window[0].is_sent == 0){
        printf("Fail \n");
        return 0;
      }

      check_for_pkt(sock, NO_FLAG);

      if(sock->window.congestion_window != init_con + MSS && sock->window.reno_state != RENO_SLOW_START){
        printf("Fail \n");
        return 0;
      }

      sock->window.congestion_window = sock->window.ssthresh - MSS - 1; 

      send_pkts(sock, (uint8_t*)buffer, MSS);
      send_pkts(sock, nullptr, 0);

      check_for_pkt(sock, NO_FLAG);

      if(sock->window.reno_state != RENO_SLOW_START){
        printf("Fail \n");
        return 0;
      }

      sock->window.congestion_window = sock->window.ssthresh - MSS;

      send_pkts(sock, (uint8_t*)buffer, MSS);
      send_pkts(sock, nullptr, 0);

      check_for_pkt(sock, NO_FLAG);

      if(sock->window.reno_state != RENO_CONGESTION_AVOIDANCE){
        printf("Fail \n");
        return 0;
      }

      sock->window.reno_state = RENO_SLOW_START;
      sock->window.congestion_window = init_con;
      sock->window.ssthresh = MSS*64;
      

      send_pkts(sock, (uint8_t*)buffer, MSS);
      send_pkts(sock, nullptr, 0);

      check_for_pkt(sock, NO_FLAG); // dup ACK 1
      if(sock->window.congestion_window != init_con || sock->window.reno_state != RENO_SLOW_START){
        printf("Fail \n");
        return 0;
      }
      check_for_pkt(sock, NO_FLAG); // dup ACK 2
      if(sock->window.congestion_window != init_con || sock->window.reno_state != RENO_SLOW_START){
        printf("Fail \n");
        return 0;
      }
      check_for_pkt(sock, NO_FLAG); // dup ACK 3
      
      send_pkts(sock, nullptr, 0); // resend

      if(sock->window.reno_state != RENO_FAST_RECOVERY || sock->window.ssthresh != (uint32_t)(init_con/2)  || sock->window.congestion_window != (uint32_t)(sock->window.ssthresh + 3*MSS)){
        printf("Fail, target reno %d, ssthresh %d, cong %d\n", 2, (uint32_t)(init_con/2), (uint32_t)(sock->window.ssthresh + 3*MSS));
        return 0;
      }

      printf("Pass \n");
      break;
    }
  
  case 8:{
    foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);
    
    if(sock->window.reno_state != RENO_SLOW_START){
      printf("Fail \n");
      return 0;
    }
    char buffer[MSS];
    memset(buffer, 'A', MSS);

    uint32_t init_con = sock->window.congestion_window;

    sock->window.congestion_window = MSS;

    send_pkts(sock, (uint8_t*)buffer, MSS);
    send_pkts(sock, nullptr, 0);

    send_pkts(sock, (uint8_t*)buffer, MSS);
    send_pkts(sock, nullptr, 0);

    if(sock->send_window[1].is_sent == 1){
      printf("Fail \n");
      return 0;
    }

    check_for_pkt(sock, NO_FLAG);
    send_pkts(sock, nullptr, 0);

    if(sock->send_window[0].is_sent == 0){
      printf("Fail \n");
      return 0;
    }
    check_for_pkt(sock, NO_FLAG);

    sock->window.reno_state = RENO_CONGESTION_AVOIDANCE;
    sock->window.congestion_window = sock->window.ssthresh;

    send_pkts(sock, (uint8_t*)buffer, MSS);
    send_pkts(sock, nullptr, 0);
    
    check_for_pkt(sock, NO_FLAG);

    if(sock->window.congestion_window != sock->window.ssthresh + MSS*MSS/sock->window.ssthresh || sock->window.dup_ack_count != 0){
      printf("Fail \n");
      return 0;
    }

    sock->window.reno_state = RENO_CONGESTION_AVOIDANCE;
    sock->window.congestion_window = sock->window.ssthresh;

    send_pkts(sock, (uint8_t*)buffer, MSS);
    send_pkts(sock, nullptr, 0);
    
    check_for_pkt(sock, NO_FLAG);  //first dup ACK

    if(sock->window.congestion_window != sock->window.ssthresh && sock->window.dup_ack_count != 1){
      printf("Fail \n");
      return 0;
    }
    check_for_pkt(sock, NO_FLAG);  //second dup ACK
    if(sock->window.congestion_window != sock->window.ssthresh && sock->window.dup_ack_count != 2){
      printf("Fail \n");
      return 0;
    }
    check_for_pkt(sock, NO_FLAG);  //third dup ACK
    if(sock->window.congestion_window != sock->window.ssthresh && sock->window.dup_ack_count != 3){
      printf("Fail \n");
      return 0;
    }

    send_pkts(sock, nullptr, 0);  // resend

    if(sock->window.reno_state != RENO_FAST_RECOVERY || sock->window.ssthresh != (uint32_t)(MSS*32)  || sock->window.congestion_window != (uint32_t)(sock->window.ssthresh + 3*MSS)){
        printf("Fail, target reno %d, ssthresh %d, cong %d\n", 2, (uint32_t)(MSS*32), (uint32_t)(sock->window.ssthresh + 3*MSS));
        return 0;
    }

    printf("Pass \n");
    break;

  }
  case 9:{
    foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_INITIATOR, server_port, server_ip, true);
    
    if(sock->window.reno_state != RENO_SLOW_START){
      printf("Fail \n");
      return 0;
    }
    char buffer[MSS];
    memset(buffer, 'A', MSS);

    sock->window.dup_ack_count = 3;
    sock->window.reno_state = RENO_FAST_RECOVERY;
    uint32_t init_cong = sock->window.congestion_window;
    uint32_t init_SS = sock->window.ssthresh;

    send_pkts(sock, (uint8_t*)buffer, MSS);
    send_pkts(sock, nullptr, 0);
    check_for_pkt(sock, NO_FLAG);

    if(sock->window.reno_state != RENO_CONGESTION_AVOIDANCE || sock->window.congestion_window != init_SS || sock->window.dup_ack_count != 0){
      printf("Fail \n");
      return 0;
    }

    sock->window.reno_state = RENO_FAST_RECOVERY;
    sock->window.congestion_window = init_cong;
    sock->window.ssthresh = init_SS;
    sock->window.dup_ack_count = 3;

    send_pkts(sock, (uint8_t*)buffer, MSS);
    send_pkts(sock, nullptr, 0);
    
    check_for_pkt(sock, NO_FLAG);  //first dup ACK

    if(sock->window.reno_state != RENO_FAST_RECOVERY || sock->window.dup_ack_count != 3 || sock->window.congestion_window != init_cong + MSS){
      printf("Fail dup %d \n", sock->window.dup_ack_count);
      return 0;
    }
    send_pkts(sock, nullptr, 0); //resend

    printf("Pass \n");

  }


  return 0;
  }
}
