#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  Message msg;

  int passed_commands = 0;

  if(sendMessage(socket, REQ_OK | NAME, "test") != MSG_OK) ASSERT(0, "Send name message failed");
  
  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive name message failed");

  if(sendMessage(socket, REQ_OK | PASSWORD, "test") != MSG_OK) ASSERT(0, "Send password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive password message failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive authenticated message failed");

  if(sendMessage(socket, REQ_OK | DATA, "echo 'a command was run'") != MSG_OK) ASSERT(0, "Send echo command failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive echo command message failed");

  debugMessage(&msg);

  if(hasFlag(msg.status, RES_OK | DATA)) passed_commands++;

  if(sendMessage(socket, REQ_OK | DATA, "pwd") != MSG_OK) ASSERT(0, "Send built-in command failed");

  if(recvMessage(socket, &msg) != MSG_OK) ASSERT(0, "Receive built-in command message failed");

  debugMessage(&msg);

  if(hasFlag(msg.status, RES_OK | DATA)) passed_commands++;

  ASSERT(passed_commands == 2, "Custom Unix Shell Commands executed successfully");

  return 0;
}