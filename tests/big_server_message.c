#include "test.h"

// The Unix-Shell Maximum Input Size is 16384 bytes. If it exceeds that, it breaks.
int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;
  char buffer[BUFFER_SIZE];

  for(int i = 5; i < 16372; i++){
    buffer[i] = '1' + (i % 9);
  }

  memcpy(buffer, "echo ", 5);
  memcpy(buffer + 16372, " > test.txt", 12);

  buffer[16372 + 12] = '\0';

  if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");
  
  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive authentication message failed");

  if(sendMessage(socket, REQ_OK | DATA, buffer) != MSG_OK) ASSERT(0, "Send echo command message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive echo command response message failed");

  if(sendMessage(socket, REQ_OK | DATA, "cat < test.txt") != MSG_OK) ASSERT(0, "Send command output message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive command output response message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_OK | DATA), "Big server message handled successfully");

  return 0;
}