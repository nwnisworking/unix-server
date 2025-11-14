#include <sys/signal.h>
#include "test.h"
#include <unistd.h>

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);

  sendMessage(socket, REQ_OK | NAME, "test");

  close(socket);

  ASSERT(1, "Connection terminated successfully");

  return 0;
}