#include <arpa/inet.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAXPENDING 5
#define BUFFERSIZE 32
#define PORT_NUMBER 4000
#define BUFLEN 48
#define EXIT 0
// the probability with which the packets will be dropped is defined here.
// Change this value to get the value of your choice.
#define PDR 0.1

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

// function to generate the boolean whether to drop the current packet or not
// based on PDR.
bool drop_packet() {
  srand(time(NULL));
  int num = ((double)rand() / RAND_MAX) * 10 + (int)1;

  if (num <= (int)((double)10 * PDR)) {
    return true;
  }
  return false;
}

// function to log the packet details based on the packet type.
void log_packet(Packet packet, char *action) {
  if (strcmp(action, "DROP PKT") == 0) {
    printf("%s: Seq no. = %d bytes Type = %s Payload = %s\n", action,
           packet.seq_no, packet.data_type == NAME ? "NAME" : "ID",
           packet.payload);
  } else if (packet.type == DATA) {
    printf("%s: Seq no. = %d Size = %d Type: %s word: %s\n", action,
           packet.seq_no, packet.size, packet.data_type == NAME ? "NAME" : "ID",
           packet.payload);
  } else if (packet.type == ACK) {
    printf("%s: Seq no. = %d bytes Type: %s\n", action, packet.seq_no,
           packet.data_type == NAME ? "NAME" : "ID");
  }
}

// function to change the state of server depending of the various conditions.
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

  bytesSent =
      recv(client1Socket, &prev_client1_pkt, sizeof(prev_client1_pkt), 0);

  bytesSent =
      recv(client2Socket, &prev_client2_pkt, sizeof(prev_client2_pkt), 0);

  int name_socket;
  int id_socket;

  printf("\n\n-------- CONNECTION SUCESSFULL WITH BOTH CLIENTS ---------\n\n");
  if (prev_client1_pkt.data_type == NAME) {
    name_socket = client1Socket;
    id_socket = client2Socket;
  } else {
    name_socket = client2Socket;
    id_socket = client1Socket;
  }

  printf("\n\n-------- BEGINNING TRANSFER ---------\n\n");
  printf("\n\n-------- TRANSFER LOGS ---------\n\n");

  // switch state for 4 states of FSM
  while (1) {

    switch (state) {
    case 0:
      while (true) {
        // printf("case 0\n");
        bytesRcvd = recv(name_socket, &data_pkt, sizeof(data_pkt), 0);
        if (bytesRcvd < 0) {
          printf("ERROR: failed to receive data from client 1\n");
          exit(EXIT);
        }

        sleep(1);

        bool drop = drop_packet();

        if ((!drop &&
             prev_client1_pkt.seq_no + strlen(prev_client1_pkt.payload) ==
                 data_pkt.seq_no &&
             data_pkt.data_type == NAME) ||
            strcmp(data_pkt.payload, "FIN") == 0) {
          break;
        }

        log_packet(data_pkt, "DROP PKT");
      }

      prev_client1_pkt = data_pkt;
      if (strcmp(data_pkt.payload, "FIN") != 0) {
        fprintf(fp, "%s,", data_pkt.payload);
        log_packet(data_pkt, "RCVD PKT");
      }

      state = get_next_state(state, client1_fin, client2_fin);
      break;

    case 1:

      // printf("case 1\n");
      ack_pkt.type = ACK;
      ack_pkt.size = sizeof(ack_pkt);
      ack_pkt.seq_no = data_pkt.seq_no;
      ack_pkt.data_type = NAME;

      bytesSent = send(name_socket, &ack_pkt, sizeof(ack_pkt), 0);
      if (bytesSent < sizeof(ack_pkt)) {
        printf("ERROR: failed to send ack to client 1\n");
        exit(EXIT);
      }
      if (strcmp(data_pkt.payload, "FIN") == 0) {
        close(client1Socket);
        client1_fin = true;
      } else {
        log_packet(ack_pkt, "SENT ACK");
      }

      // printf("ACK sent to client 1 from server\n\n");
      state = get_next_state(state, client1_fin, client2_fin);

      break;

    case 2:
      while (true) {
        // printf("case 2\n");
        bytesRcvd = recv(id_socket, &data_pkt, sizeof(data_pkt), 0);
        if (bytesRcvd < 0) {
          printf("ERROR: failed to receive data from client 1\n");
          exit(EXIT);
        }

        bool drop = drop_packet();

        if ((!drop &&
             prev_client2_pkt.seq_no + strlen(prev_client2_pkt.payload) ==
                 data_pkt.seq_no &&
             data_pkt.data_type == ID) ||
            strcmp(data_pkt.payload, "FIN") == 0) {
          break;
        }

        log_packet(data_pkt, "DROP PKT");
      }

      prev_client2_pkt = data_pkt;

      if (strcmp(data_pkt.payload, "FIN") != 0) {
        fprintf(fp, "%s,", data_pkt.payload);
        log_packet(data_pkt, "RCVD PKT");
      }

      state = get_next_state(state, client1_fin, client2_fin);
      break;

    case 3:
      ack_pkt.type = ACK;
      ack_pkt.size = sizeof(ack_pkt);
      ack_pkt.seq_no = data_pkt.seq_no;
      ack_pkt.data_type = ID;

      bytesSent = send(id_socket, &ack_pkt, sizeof(ack_pkt), 0);
      if (bytesSent < sizeof(ack_pkt)) {
        printf("ERROR: failed to send ack to client 2\n");
        exit(EXIT);
      }
      if (strcmp(data_pkt.payload, "FIN") == 0) {
        close(client2Socket);
        client2_fin = true;
      } else {
        log_packet(ack_pkt, "SENT ACK");
      }
      state = get_next_state(state, client1_fin, client2_fin);
      break;

    case 4:
      fseek(fp, -1, SEEK_END);
      fputc('.', fp);
      fclose(fp);
      close(serverSocket);
      return 0;
    }
  }
}
