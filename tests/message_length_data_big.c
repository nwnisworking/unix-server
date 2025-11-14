#include "test.h"
#include <unistd.h>
  
int sendAll(int fd, const char* buffer, int size, ssize_t* sent_bytes){
  *sent_bytes = 0;

  while(*sent_bytes < size){
    ssize_t bytes = send(fd, buffer + *sent_bytes, size - *sent_bytes, 0);

    if(bytes <= 0){
      return MSG_ERROR;
    }

    *sent_bytes += bytes;
  }

  return MSG_OK;
}

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  char data[] = "Hello, world!";
  int size = 4; // Intentionally incorrect size
  char malformed_data[3 + sizeof(data)];
  ssize_t total_bytes;
  Message msg;
  
  malformed_data[0] = REQ_OK | NAME;
  
  uint16_t net_length = htons(size);

  memcpy(&malformed_data[1], &net_length, sizeof(uint16_t));
  memcpy(&malformed_data[3], data, sizeof(malformed_data) - 3);

  if(sendAll(socket, malformed_data, 3 + size, &total_bytes) != MSG_OK) ASSERT(0, "Send malformed message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive response message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_ERR | NAME), "Malformed message data size larger than declared size");

  return 0;
}