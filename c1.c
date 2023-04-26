#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFLEN 512
#define PORT_NUMBER 4000
#define EXIT 0
#define BEGIN_SEQ_MAX 4096

// packet struct.
typedef enum { DATA, ACK, FIN } type;
typedef enum { NAME, ID, NA } data_type;
typedef struct {
  int size;
  int seq_no;
  type type;
  char payload[BUFLEN];
  data_type data_type;
} Packet;

int fd;
Packet curr_pkt, prev_pkt;

char *get_next_word(FILE *fp) {
  char *buf = (char *)malloc(BUFLEN * sizeof(char));
  static char c;
  if (c == '.')
    return NULL;

  int i = 0;
  do {
    c = fgetc(fp);
    if (c == ',' || c == '.')
      break;
    buf[i] = c;
    i++;
  } while (c != ',' && c != '.');

  buf[i] = '\0';

  printf("word: %s\n", buf);
  return buf;
}

void send_pkt() {
  // now for each and every character from the file.
  int bytesSent = send(fd, &curr_pkt, sizeof(curr_pkt), 0);

  if (bytesSent != sizeof(curr_pkt)) {
    printf("ERROR - Couldn't send the id packet. Bytes sent: %d\n", bytesSent);
    exit(EXIT);
  }
}

void rcv_ack() {
  do {

    int ready;
    Packet ack_pkt;
    // now its time to receive the ack packet.
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 2; // 5 seconds
    timeout.tv_usec = 0;
    ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready == -1) {
      perror("select");
    } else if (ready == 0) {
      printf("Timeout occurred, resending packet.\n");
      int bytesSent = send(fd, &curr_pkt, sizeof(curr_pkt), 0);
      if (bytesSent != sizeof(curr_pkt)) {
        printf("ERROR - Couldn't send the id packet. Bytes sent: %d\n",
               bytesSent);
        exit(EXIT);
      }
    } else {
      if (FD_ISSET(fd, &read_fds)) {
        int recv_len = recv(fd, &ack_pkt, sizeof(ack_pkt), 0);
        prev_pkt = curr_pkt;
        break;
      }
    }
  } while (1);
}

int main() {
  fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    printf("Error while creating the socket\n");
    exit(EXIT);
  }

  // now the fd for socket is allocated we can start the chutiyapa now.
  //
  struct sockaddr_in serverAddr;

  memset(&serverAddr, 0, sizeof(serverAddr));

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT_NUMBER);
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  printf("The server has been assigned a ip address of 127.0.0.1\n");

  int connection =
      connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

  if (connection < 0) {
    printf("Failed to establish a connection betweeen the TCP server and the "
           "socket\n");
    exit(EXIT);
  }

  printf("Connection established successfully\n");

  FILE *fp = fopen("name.txt", "r");

  if (fp == NULL) {
    printf("ERROR - Failed to open the file!");
    exit(EXIT);
  }

  prev_pkt.seq_no = 0;
  prev_pkt.size = 0;
  prev_pkt.data_type = NAME;
  prev_pkt.type = DATA;

  int send_len = send(fd, &prev_pkt, sizeof(prev_pkt), 0);
  printf("syn received\n");

  while (1) {
    char *word = get_next_word(fp);
    if (word == NULL) {
      // time to send the find packet.
      Packet fin;
      fin.size = sizeof(fin);
      fin.seq_no = prev_pkt.seq_no + prev_pkt.size;
      fin.type = FIN;
      fin.data_type = NAME;
      curr_pkt = fin;
      send_pkt();
      rcv_ack();
      printf(" --------  Connection ended with server ---------\n");
      break;
    }

    curr_pkt.type = DATA;
    curr_pkt.seq_no = prev_pkt.seq_no + prev_pkt.size;
    curr_pkt.size = sizeof(curr_pkt);
    curr_pkt.data_type = NAME;
    strcpy(curr_pkt.payload, word);

    send_pkt();
    rcv_ack();
    printf("Packet sent successfully\n\n");
  }

  fclose(fp);
  close(fd);

  return 0;
}
