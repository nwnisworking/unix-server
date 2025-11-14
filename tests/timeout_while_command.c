#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;

  if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");
  
  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive response message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive response message failed");

  if(sendMessage(socket, REQ_OK | DATA, "echo Happens only once") != MSG_OK) ASSERT(0, "Send command message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive command response message failed");

  debugMessage(&msg);

  sleep(35);

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive command response message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_ERR), "Timeout while idling");

  return 0;
}