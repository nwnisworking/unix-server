#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;

  // The first request should be name.
  if(sendMessage(socket, REQ_OK | NAME, "") != MSG_OK) ASSERT(0, "Send name message failed");

  debugMessage(&msg);
  ASSERT(recvMessage(socket, &msg) == MSG_OK, "Empty message sent");
  return 0;
}