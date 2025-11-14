#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;


  if(send(socket, "t", 1, 0) != 1)ASSERT(0, "Send malformed header failed");

  if(recvMessage(socket, &msg) != MSG_OK)ASSERT(0, "Receive response message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_ERR), "Malformed header detected");

  return 0;
}