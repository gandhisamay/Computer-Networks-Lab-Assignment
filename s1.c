#include <arpa/inet.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAXPENDING 5
#define BUFFERSIZE 32
#define PORT_NUMBER 4000
#define BUFLEN 512
#define EXIT 0

typedef enum { DATA, ACK, FIN } type;
typedef enum { ONE, TWO } client_id;
typedef struct {
  int size;
  int seq_no;
  type type;
  char payload[BUFLEN];
  client_id client;
} Packet;

void log_packet(Packet packet) {
  printf("Payload: %s\n", packet.payload);
  printf("Type: %s\n", packet.type == DATA  ? "DATA"
                       : packet.type == FIN ? "FIN"
                                            : "ACK");
  printf("Size: %d\n", packet.size);
  printf("Seq no: %d\n", packet.seq_no);
  printf("Client: %s\n", packet.client == ONE ? "ONE" : "TWO");
}

int get_next_state(int prev_state, bool client1_fin, bool client2_fin) {
  if (client1_fin && client2_fin) {
    return 4;
  } else if (prev_state == 2) {
    return 3;
  } else if (client1_fin && prev_state == 3) {
    return 2;
  } else if (prev_state == 0) {
    return 1;
  } else if (client2_fin && prev_state == 1) {
    return 0;
  } else if (!client1_fin && !client2_fin) {
    if (prev_state == 1)
      return 2;
    else if (prev_state == 3)
      return 0;
  }
  return 4;
}

int main() {
  /*CREATE A TCP SOCKET*/
  int serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (serverSocket < 0) {
    printf("Error while server socket creation");
    exit(0);
  }
  printf("Server Socket Created\n");
  /*CONSTRUCT LOCAL ADDRESS STRUCTURE*/
  struct sockaddr_in serverAddress, client1Address, client2Address;
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT_NUMBER);
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  printf("Server address assigned\n");

  int temp = bind(serverSocket, (struct sockaddr *)&serverAddress,
                  sizeof(serverAddress));

  if (temp < 0) {
    printf("Error while binding\n");
    exit(0);
  }

  printf("Binding successful\n");
  int temp1 = listen(serverSocket, MAXPENDING);
  if (temp1 < 0) {
    printf("Error in listen");
    exit(0);
  }

  printf("Now Listening\n");

  int client1Length = sizeof(client1Address);
  int client1Socket = accept(serverSocket, (struct sockaddr *)&client1Address,
                             (unsigned int *)&client1Length);
  if (client1Length < 0) {
    printf("Error in client 1 socket");
    exit(0);
  }

  int client2Length = sizeof(client2Address);
  int client2Socket = accept(serverSocket, (struct sockaddr *)&client2Address,
                             (unsigned int *)&client2Length);
  if (client2Length < 0) {
    printf("Error in client 2 socket");
    exit(0);
  }

  printf("CLIENT 1 SOCKET: %d CLIENT 2 SOCKET: %d\n", client1Socket,
         client2Socket);

  // first will receive name and then id and then name.

  // Accept is a blocking system call.
  int state = 0;
  Packet data_pkt;
  Packet ack_pkt;
  Packet prev_client1_pkt;
  Packet prev_client2_pkt;
  int bytesRcvd;
  int bytesSent;
  bool client1_fin = false;
  bool client2_fin = false;

  prev_client2_pkt.seq_no = 0;
  prev_client2_pkt.size = 0;

  prev_client1_pkt.seq_no = 0;
  prev_client1_pkt.size = 0;

  ack_pkt.type = ACK;

  FILE *fp = fopen("list.txt", "w+");
  while (1) {

    switch (state) {
    case 0:
      while (true) {
        printf("case 0\n");
        bytesRcvd = recv(client1Socket, &data_pkt, sizeof(data_pkt), 0);
        if (bytesRcvd < 0) {
          printf("ERROR: failed to receive data from client 1\n");
          exit(EXIT);
        }

        if (prev_client1_pkt.seq_no + prev_client1_pkt.size ==
                data_pkt.seq_no &&
            data_pkt.client == ONE) {
          break;
        }

        printf("-------- Package discarded --------\n\n");

        log_packet(data_pkt);

        printf("\n");
      }

      prev_client1_pkt = data_pkt;
      if (data_pkt.type != FIN) {
        fprintf(fp, "%s,", data_pkt.payload);
      }
      log_packet(data_pkt);

      state = get_next_state(state, client1_fin, client2_fin);
      break;

    case 1:

      printf("case 1\n");
      // send ack
      bytesSent = send(client1Socket, &ack_pkt, sizeof(ack_pkt), 0);
      if (bytesSent < sizeof(ack_pkt)) {
        printf("ERROR: failed to send ack to client 1\n");
        exit(EXIT);
      }
      if (data_pkt.type == FIN) {
        close(client1Socket);
        client1_fin = true;
      }
      printf("ACK sent to client 1 from server\n\n");
      state = get_next_state(state, client1_fin, client2_fin);

      break;

    case 2:
      while (true) {
        printf("case 2\n");
        bytesRcvd = recv(client2Socket, &data_pkt, sizeof(data_pkt), 0);
        if (bytesRcvd < 0) {
          printf("ERROR: failed to receive data from client 1\n");
          exit(EXIT);
        }

        if (prev_client2_pkt.seq_no + prev_client2_pkt.size ==
                data_pkt.seq_no &&
            data_pkt.client == TWO) {
          break;
        }

        printf("-------- Package discarded --------\n\n");
        log_packet(data_pkt);

        printf("\n");
      }

      prev_client2_pkt = data_pkt;

      if (data_pkt.type != FIN) {
        fprintf(fp, "%s,", data_pkt.payload);
      }

      log_packet(data_pkt);
      state = get_next_state(state, client1_fin, client2_fin);
      break;

    case 3:
      printf("case 3\n");
      bytesSent = send(client2Socket, &ack_pkt, sizeof(ack_pkt), 0);
      if (bytesSent < sizeof(ack_pkt)) {
        printf("ERROR: failed to send ack to client 2\n");
        exit(EXIT);
      }
      printf("ACK sent to client 2 from server\n\n");
      if (data_pkt.type == FIN) {
        close(client2Socket);
        client2_fin = true;
      }
      state = get_next_state(state, client1_fin, client2_fin);
      break;

    case 4:
      fclose(fp);
      close(serverSocket);
      return 0;
    }
  }
}
