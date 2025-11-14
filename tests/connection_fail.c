#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", 1234);

  ASSERT(socket == -1, "Connection to port failed");

  return 0;
}