#include "test.h"

int main(){
  int socket = clientSocket("127.0.0.1", SERVER_PORT);
  
  ASSERT(socket > -1, "Connection to port succeeded");
  
  return 0;
}