
#include <fstream>
#include <iostream>
#include <unistd.h>
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
    case 1:{ // Test for Connection seq and ack synchronize and socket state setting
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);
        if(sock->window.last_byte_sent == sock->window.last_ack_received && sock->connected == 2){
            printf("Pass\n");
        }
        sock->dying = 1;
        foggy_close(sock);
        break;
    }
    case 2:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, false);
        uint32_t original_next_seq_expecterd = sock->window.next_seq_expected;
        for(int i=0; i<10; ++i){
            sleep(1);
        }
        if(sock->window.next_seq_expected == original_next_seq_expecterd){
            printf("Pass\n");
        }
        else{
            printf("Fail\n");
        }
        break;
    }
    case 3:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, false);
        uint32_t original_next_seq_expecterd = sock->window.next_seq_expected;
        for(int i=0; i<2; ++i){
            sleep(1);
        }
        if(sock->window.next_seq_expected == original_next_seq_expecterd){
            printf("Fail\n");
        }
        else{
            printf("Pass\n");
        }
        break;
    }
    case 4:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);

        printf("Test, original last_ack_received: %d \n", sock->window.last_ack_received);
        uint8_t* pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent, // Telling the client that we are ready to receive, and the initial seq number
              sock->window.next_seq_expected,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              2000, 0, NULL, NULL, 0);
        foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)(pkt);
        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
        
        


        check_for_pkt(sock, NO_FLAG); // first pkt

        if(sock->received_len != MSS){
            printf("Failed 1\n");
        }

        check_for_pkt(sock, NO_FLAG); // second pkt

        if(sock->received_len != 2*MSS){
            printf("Failed 2\n");
        }
        else{
            printf("Pass \n");
        }

        break;
    }

    case 5:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);

        uint32_t init_next_seq_exp = sock->window.next_seq_expected;

        check_for_pkt(sock, NO_FLAG); // first pkts, should can recv;

        if(sock->received_len != MSS){
            printf("Fail \n");
            return 0;
        }

        check_for_pkt(sock, NO_FLAG); // second pkts, cannot recv, out of window so not even put to out_of_order_queue
 
        if(sock->received_len == MSS*2 || sock->out_of_order_queue.size() != 0){
            printf("Fail \n");
            return 0;
        }

        check_for_pkt(sock, NO_FLAG); // second_2 pkts, cannot recv, out of window so not even put to out_of_order_queue
 
        if(sock->received_len == MSS*2 || sock->out_of_order_queue.size() != 1){
            printf("Fail \n");
            return 0;
        }

        check_for_pkt(sock, NO_FLAG); // third pkts, can recv, but not sequential arrive

        if(sock->received_len == MSS*2 || sock->out_of_order_queue.size() != 2){ // inside window, put in queue for non-seq pkt
            printf("Fail \n");
            return 0;
        }
        

        check_for_pkt(sock, NO_FLAG); // 4th pkts, can recv, sequential arrive

        if(sock->received_len != MSS*3 || sock->out_of_order_queue.size() != 1){ // inside window, put in queue for non-seq pkt
            printf("Fail \n");
            return 0;
        }

        if(sock->window.next_seq_expected == init_next_seq_exp + 3*MSS){
            printf("Pass \n");
        }
        else{
            printf("Fail \n");
        }
        break;
    }
  
  case 6:{

    foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);

    uint32_t next_seq = sock->window.next_seq_expected;


    //sock->window.next_seq_expected = next_seq;

    uint8_t* pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent, // Telling the client that we are ready to receive, and the initial seq number
              sock->window.next_seq_expected,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              2000, 0, NULL, NULL, 0);
    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)(pkt);


    sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // first duplicate ACK

    sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // second duplicate ACk

    sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // third duplicate ACK

    check_for_pkt(sock, NO_FLAG);  //now should can recv pkt resent, but is first pkt
    if(sock->window.next_seq_expected != next_seq + MSS){
        printf("Fail \n");
        return 0;
    }
    
    sock->window.next_seq_expected = next_seq;

    check_for_pkt(sock, NO_FLAG); // should can recv one more repeated pkt

    if(sock->window.next_seq_expected != next_seq + MSS){
        printf("Fail \n");
    }
    else{
        printf("Pass \n");
    }

    break;
    }
    case 7:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);

        check_for_pkt(sock, NO_FLAG);
        check_for_pkt(sock, NO_FLAG);
        check_for_pkt(sock, NO_FLAG);  

        uint8_t* pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent, // Telling the client that we are ready to receive, and the initial seq number
              sock->window.next_seq_expected,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              MSS*20, 0, NULL, NULL, 0);

        foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)(pkt);


        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // first duplicate ACK

        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // second duplicate ACk

        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // third duplicate ACK

        check_for_pkt(sock, NO_FLAG);

        printf("Pass \n");
        break;
    }
    case 8:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);
        check_for_pkt(sock, NO_FLAG);
        check_for_pkt(sock, NO_FLAG);

        check_for_pkt(sock, NO_FLAG);

        uint8_t* pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent, // Telling the client that we are ready to receive, and the initial seq number
              sock->window.next_seq_expected,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              MSS*20, 0, NULL, NULL, 0);

        foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)(pkt);


        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // first duplicate ACK

        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // second duplicate ACk

        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // third duplicate ACK

        check_for_pkt(sock, NO_FLAG);

        printf("Pass \n");
        break;
        
    }
    case 9:{
        foggy_socket_t* sock = (foggy_socket_t*)foggy_socket(TCP_LISTENER, server_port, server_ip, true);
        check_for_pkt(sock, NO_FLAG);

        uint8_t* pkt = create_packet(
              sock->my_port, ntohs(sock->conn.sin_port),
              sock->window.last_byte_sent, // Telling the client that we are ready to receive, and the initial seq number
              sock->window.next_seq_expected,
              sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
              MSS*20, 0, NULL, NULL, 0);

        foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)(pkt);


        sendto(sock->socket, pkt, get_plen(hdr), 0, (struct sockaddr *)&(sock->conn), sizeof(sock->conn)); // first duplicate ACK

        check_for_pkt(sock, NO_FLAG);
        check_for_pkt(sock, NO_FLAG);
        
        printf("Pass \n");

    }

  return 0;

}
}
