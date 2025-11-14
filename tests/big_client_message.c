#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;
  char buffer[BUFFER_SIZE];

  for(int i = 0; i < BUFFER_SIZE - 1; i++){
    buffer[i] = '1' + (i % 9);
  }

  if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");
  
  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive authentication message failed");
  
  if(sendMessage(socket, REQ_OK | DATA, buffer) != MSG_OK) ASSERT(0, "Send data message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive data response message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_OK | DATA), "Big client message handled successfully");

  return 0;
}