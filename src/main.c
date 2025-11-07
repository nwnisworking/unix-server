#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>

#include "socket.h"
#include "socketsignal.h"

// This is a global file descriptor for the socket 
// which can either be a server or accepted client socket.
// The purpose of this global variable is to allow signal handlers to close the socket gracefully.
int fd = -1;

// User structure containing name and password
typedef struct {
  const char* name;
  const char* password;
} User;

/**
 * Validates that a received message matches the expected status.
 * 
 * @param fd The file descriptor to receive the message from.
 * @param msg Pointer to the Message structure to store the received message.
 * @param status The expected status byte of the message.
 * @return 1 if the message is valid and matches the expected status, 0 otherwise
 */
int validateMessage(Message* msg, uint8_t status);

int clientResponse(Message* msg, uint8_t expected_status){
  if(recvMessage(fd, msg) < 0){
    printf("[Server]: Error receiving message from client\n");
    return 0;
  }

  if(msg->status != expected_status){
    printf("[Server]: Unexpected message status\n");
    return 0;
  }

  return 1;
}

/**
 * Handles the authentication process for a connected client.
 * @return The index of the authenticated user in the users array, or -1 on failure.
 */
int authenticateUser();

/**
 * Handles incoming messages from a connected client.
 * @param user Pointer to the authenticated User structure.
 */
void handleUserMessage(User* user);

/**
 * Reverses the given data string in place.
 * 
 * @param data The string data to be reversed.
 */
void reverse(char* data);

/**
 * Cleanup function to be called at program exit.
 * Closes the global socket file descriptor if it is open.
 */
void cleanup();

// Predefined users for authentication testing. 
// This variable is only visible within this file.
static User users[] = {
  {"test", "test"},
  {"bob", "dylan"},
  {"alice", "wonderland"},
  {NULL, NULL}
};

int main(){
  atexit(cleanup);

  pid_t pid; // Process ID for forked child processes

  fd = serverSocket(SERVER_PORT);

  if(fd < 0){
    printf("[Server]: Unable to create server socket\n");
    exit(EXIT_FAILURE);
  }

  installSignalHandler();

  printf("[Server]: Listening on port %d\n", SERVER_PORT);

  while(1){
    // We can just ignore the address since we don't care about the client details here
    int client_fd = accept(fd, NULL, NULL);

    if(client_fd < 0){
      printf("[Server]: Unable to accept client connection\n");
      continue;
    }

    pid = fork();

    // Unable to provide a new process for the client
    if(pid < 0){
      printf("[Server]: Unable to fork process for client\n");
      close(client_fd);
      continue;
    }
    // Parent process closes the client socket and continues to accept new connections
    else if(pid > 0){
      printf("[Server]: Accepted new client connection\n");
      close(client_fd);
      continue;
    }
    // Child process handles the client socket
    else{
      close(fd); // Close the server socket in the child process
      fd = client_fd;

      struct timeval timeout;
      timeout.tv_sec = 4; // 4 seconds timeout

      // Set receive timeout on the client socket to prevent user from sending incorrect data length.
      // This ensures that recv calls will timeout if no data is received within the specified time.
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

      break;
    }
  }

  if(pid == 0){
    int index = authenticateUser();

    if(index == -1){
      cleanup();
      _exit(EXIT_FAILURE);
    }
    else{
      handleUserMessage(&users[index]);
      
      cleanup();
      _exit(EXIT_SUCCESS);
    }
  }

  return 0;
}

int authenticateUser(){
  Message msg;
  char username[BUFFER_SIZE];
  char password[BUFFER_SIZE];

  recvMessage(fd, &msg);

  if(msg.status == (REQUEST_STATUS | USERNAME)){
    sendMessage(fd, RESPONSE_STATUS | SUCCESS_STATUS, "");
    strncpy(username, msg.data, ntohs(msg.length));
  }
  else{
    sendMessage(fd, RESPONSE_STATUS | ERROR_STATUS, "Error receiving username");
    return -1;
  }

  recvMessage(fd, &msg);

  if(msg.status == (REQUEST_STATUS | PASSWORD)){
    sendMessage(fd, RESPONSE_STATUS | SUCCESS_STATUS, "");
    strncpy(password, msg.data, ntohs(msg.length));
  }
  else{
    sendMessage(fd, RESPONSE_STATUS | ERROR_STATUS, "Error receiving password");
    return -1;
  }

  for(int i = 0; users[i].name != NULL; i++){
    if(
      strcmp(users[i].name, username) == 0 &&
      strcmp(users[i].password, password) == 0
    ){
      printf("[Server]: User '%s' authenticated successfully\n", username);
      sendMessage(fd, RESPONSE_STATUS | SUCCESS_STATUS, "You are now authenticated");
      return i;
    }
  }

  printf("[Server]: Authentication failed for user '%s'\n", username);
  sendMessage(fd, RESPONSE_STATUS | ERROR_STATUS, "Invalid username or password");
  
  return -1;
}

void handleUserMessage(User* user){
  Message msg;

  while(1){
    if(recvMessage(fd, &msg) < 0){
      printf("[user:%s] Error receiving message or connection closed\n", user->name);
      break;
    }

    if(msg.status == (REQUEST_STATUS | CLOSE)){
      printf("[user:%s] Close request received\n", user->name);
      break;
    }
    else if(msg.status == (REQUEST_STATUS | DATA)){
      printf("[user:%s] %s\n", user->name, msg.data);
      
      reverse(msg.data);

      if(sendMessage(fd, RESPONSE_STATUS | SUCCESS_STATUS, msg.data) < 0){
        printf("Error sending message back to client\n");
        break;
      }
    }
  }
}

void reverse(char* data){
  size_t len = strlen(data);
  for(size_t i = 0; i < len / 2; i++){
    char temp = data[i];
    data[i] = data[len - i - 1];
    data[len - i - 1] = temp;
  }
}

void cleanup(){
  printf("Cleaning up socket before exit...\n");

  if(fd >= 0){
    close(fd);
    fd = -1;
  }
}