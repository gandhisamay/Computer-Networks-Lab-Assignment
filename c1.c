#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFLEN 48
#define PORT_NUMBER 4000
#define EXIT 0
#define BEGIN_SEQ_MAX 4096
#define PDR 0.5

// packet struct.
typedef enum { DATA, ACK } type;
typedef enum { NAME, ID, NA } data_type;
typedef struct {
  int size;
  int seq_no;
  type type;
  char payload[BUFLEN];
  // this field helps in identifying what type of data is being sent whether it
  // is a NAME or ID being sent.
  data_type data_type;
} Packet;

int fd;
Packet curr_pkt, prev_pkt;

bool drop_packet() {
  srand(time(NULL));
  int num = ((double)rand() / RAND_MAX) * 10 + (int)1;

  if (num <= (int)((double)10 * PDR)) {
    return true;
  }
  return false;
}

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

  return buf;
}

void log_packet(Packet packet, char *action) {
  if (packet.type == DATA) {
    printf("%s: Seq no. = %d Size = %d bytes Payload = %s\n", action,
           packet.seq_no, packet.size, packet.payload);
  } else {
    printf("%s: Seq no. = %d bytes\n", action, packet.seq_no);
  }
}

void send_pkt(bool log) {
  int bytesSent = send(fd, &curr_pkt, sizeof(curr_pkt), 0);

  if (bytesSent != sizeof(curr_pkt)) {
    printf("ERROR - Couldn't send the id packet. Bytes sent: %d\n", bytesSent);
    exit(EXIT);
  }

  if (log) {
    log_packet(curr_pkt, "SEND PKT");
  }
}

void rcv_ack(bool log) {
  do {

    int ready;
    Packet ack_pkt;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready == -1) {
      perror("select");
    } else if (ready == 0) {
      int bytesSent = send(fd, &curr_pkt, sizeof(curr_pkt), 0);
      if (bytesSent != sizeof(curr_pkt)) {
        printf("ERROR - Couldn't send the id packet. Bytes sent: %d\n",
               bytesSent);
        exit(EXIT);
      }

      log_packet(curr_pkt, "RE-TRANSMIT PKT");
    } else {
      if (FD_ISSET(fd, &read_fds)) {
        int recv_len = recv(fd, &ack_pkt, sizeof(ack_pkt), 0);
        // lets drop the acks here on the receiver side.
        if (drop_packet() && log) {
          log_packet(ack_pkt, "DROP PKT");
        } else if (ack_pkt.seq_no == curr_pkt.seq_no) {
          prev_pkt = curr_pkt;
          if (log)
            log_packet(ack_pkt, "RCVD ACK");
          break;
        }
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

  while (1) {
    char *word = get_next_word(fp);
    if (word == NULL) {
      Packet fin;
      fin.size = sizeof(fin);
      fin.seq_no = prev_pkt.seq_no;
      fin.type = DATA;
      strcpy(fin.payload, "FIN");
      fin.data_type = NAME;
      curr_pkt = fin;
      send_pkt(false);
      rcv_ack(false);
      printf(" --------  Connection ended with server ---------\n");
      break;
    }

    curr_pkt.type = DATA;
    curr_pkt.seq_no = prev_pkt.seq_no + strlen(curr_pkt.payload);
    curr_pkt.size = sizeof(curr_pkt);
    curr_pkt.data_type = NAME;
    strcpy(curr_pkt.payload, word);

    send_pkt(true);
    rcv_ack(true);
  }

  fclose(fp);
  close(fd);

  return 0;
}
