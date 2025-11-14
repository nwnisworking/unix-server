#include "test.h"

int main(){
  int i = 0;
  for(; i < 5; i++){
    int socket = clientSocket("127.0.0.1", SERVER_PORT);
    Message msg;

    if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");
    
    if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

    if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send password message failed");

    if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive password message failed");
    
    if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive authentication message failed");
  }

  ASSERT(i == 5, "Similar client connections");

  return 0;
}