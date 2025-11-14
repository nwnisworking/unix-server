#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;

  if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");
  
  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  if(sendMessage(socket, REQ_OK | PASSWORD, "Invalid Pass") != MSG_OK) ASSERT(0, "Send password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive password message failed");
  
  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive authentication message failed");
  
  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_ERR | AUTH), msg.data);

  return 0;
}