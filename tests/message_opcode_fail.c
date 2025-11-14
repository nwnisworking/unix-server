#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;

  // The first request should be name.
  if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send name message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_ERR | PASSWORD), "Unexpected opcode in request");

  return 0;
}