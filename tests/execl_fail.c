#include "test.h"

// This test requires you to rename unix-shell to something else
// so that execl fails to find it.
int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;

  if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive authenticated message failed");

  if(sendMessage(socket, REQ_OK | DATA, "echo Hello, world!") != MSG_OK) ASSERT(0, "Send command message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive command response message failed");

  debugMessage(&msg);
  ASSERT(hasFlag(msg.status, RES_ERR | DATA), "Unix-shell can't be found");

  return 0;
}