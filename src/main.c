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
#include <sys/wait.h>

#include "socket.h"
#include "socketsignal.h"
#include "protocol.h"
#include "test.h"

// This is a file descriptor for the socket 
// which can either be a server or accepted client socket.
static int fd = -1;

// User structure containing name and password
typedef struct {
  const char* name;
  const char* password;
} User;

/**
 * Handles the authentication process for a connected client.
 */
void authenticateUser(User* user);

/**
 * Handles incoming messages from a connected client.
 * @param user Pointer to the authenticated User structure.
 */
void handleUserMessage(User* user);

/**
 * Cleanup function to be called at program exit.
 * Closes the global socket file descriptor if it is open.
 */
void cleanup();

/**
 * Signal handler function to handle termination signals.
 * Closes the global socket and terminates child processes gracefully.
 */
void signalHandler();

/**
 * Receive a message from the server and check for expected flags.
 * Exits the program if an error occurs or unexpected flags are received.
 * 
 * @param msg Pointer to the Message structure to populate.
 * @param expect_flags The expected flags in the message status.
 */
int response(Message* msg, int expect_flags);

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

  installSignalHandler(signalHandler);

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

      while(waitpid(-1, NULL, WNOHANG) > 0);

      continue;
    }
    // Child process handles the client socket
    else{
      close(fd); // Close the server socket in the child process
      fd = client_fd;

      struct timeval timeout;
      timeout.tv_sec = 30;

      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      
      break;
    }
  }

  if(pid == 0){
    User user;

    authenticateUser(&user);
    handleUserMessage(&user);
      
    cleanup();
    _exit(EXIT_SUCCESS);
  }

  return 0;
}

void signalHandler(){
  kill(0, SIGTERM);

  if(fd != -1){
    close(fd);
    fd = -1;
  }

  while(waitpid(-1, NULL, WNOHANG) > 0);

  printf("\n[Server]: Caught termination signal, shutting down gracefully...\n");

  _exit(0);
}

void authenticateUser(User* user){
  Message msg;
  char username[BUFFER_SIZE];
  char password[BUFFER_SIZE];

  if(!response(&msg, REQ_OK | NAME)) cleanup(), _exit(EXIT_FAILURE);
  sendMessage(fd, RES_OK | NAME, NULL);

  strncpy(username, msg.data, ntohs(msg.length));

  if(!response(&msg, REQ_OK | PASSWORD)) cleanup(), _exit(EXIT_FAILURE);
  sendMessage(fd, RES_OK | PASSWORD, NULL);

  strncpy(password, msg.data, ntohs(msg.length));

  for(int i = 0; users[i].name != NULL; i++){
    if(
      strcmp(users[i].name, username) == 0 &&
      strcmp(users[i].password, password) == 0
    ){
      user->name = users[i].name;
      
      printf("[Server]: User '%s' authenticated successfully\n", username);
      sendMessage(fd, RES_OK | AUTH, "You are now authenticated");
      return;
    }
  }

  printf("[Server]: Authentication failed for user '%s'\n", username);
  sendMessage(fd, RES_ERR | AUTH, "Invalid username or password");
  cleanup();
  _exit(EXIT_FAILURE);

  return;
}

void handleUserMessage(User* user) {
  int parent_pipe[2];
  int shell_pipe[2];
  pid_t shell_pid;

  if(pipe(parent_pipe) < 0 || pipe(shell_pipe) < 0){
    printf("[Server]: Unable to create pipes for shell communication\n");
    sendMessage(fd, RES_ERR | DATA, "Unable to create pipes");
    return;
  }

  shell_pid = fork();

  if(shell_pid < 0){
    printf("[Server]: Unable to fork shell process\n");
    sendMessage(fd, RES_ERR | DATA, "Unable to fork shell process");
    return;
  }
  // Child shell process
  else if(shell_pid == 0){
    // Disable unused pipe ends
    close(parent_pipe[0]);
    close(shell_pipe[1]);

    // Redirect stdin, stdout, stderr to the appropriate pipe ends
    dup2(shell_pipe[0], STDIN_FILENO);
    dup2(parent_pipe[1], STDOUT_FILENO);
    dup2(parent_pipe[1], STDERR_FILENO);

    // Close the duplicated pipe ends since stdout, stdin, stderr now use them
    close(shell_pipe[0]);
    close(parent_pipe[1]);

    execl("./unix-shell", "./unix-shell", "-server", NULL);
  
    // If execl returns, an error occurred. Time to bail.
    perror("execl");
    fflush(stderr);
    _exit(EXIT_FAILURE);
  }
  // Parent process
  else{
    // Disable unused pipe ends
    close(parent_pipe[1]);
    close(shell_pipe[0]);

    Message msg;
    ssize_t bytes;
    size_t total = 0;
    char buffer[BUFFER_SIZE];
    int exec_failed = 0;

    // When the shell starts, it may output a prompt.
    while((bytes = read(parent_pipe[0], buffer + total, sizeof(buffer) - total - 1)) > 0){
      total+= bytes;
      buffer[total] = '\0';
      if(strstr(buffer, "[PROMPT]")) break;
      if(strstr(buffer, "execl")) {
        exec_failed = 1;
        break;
      }
    }

    if(exec_failed){
      printf("[Server]: Shell execution failed\n");
      sendMessage(fd, RES_ERR | DATA, "Shell execution failed");
      kill(shell_pid, SIGTERM);
      waitpid(shell_pid, NULL, 0);
      return;
    }

    // After the initial prompt, that's when user commands can be processed. 
    while(1){
      total = 0;

      if(!response(&msg, REQ_OK)) break;

      if(hasFlag(msg.status, CLOSE)){
        printf("[Server]: User '%s' disconnected the session\n", user->name);
        sendMessage(fd, RES_OK | CLOSE, "Connection closed");
        break;
      }

      printf("[user:%s]: %s\n", user->name, msg.data);

      int result;

      result = write(shell_pipe[1], msg.data, ntohs(msg.length));
      result+= write(shell_pipe[1], "\n", 1);

      if(result < 0){
        printf("[Server]: Failed to write to shell\n");
        sendMessage(fd, RES_ERR | DATA, "Failed to write to shell");
        break;
      }

      memset(buffer, 0, sizeof(buffer));
      // Read from the shell until the next prompt is displayed.
      while((bytes = read(parent_pipe[0], buffer + total, sizeof(buffer) - total - 1)) > 0){
        total+= bytes;
        buffer[total] = '\0';

        if(strstr(buffer, "[PROMPT]")) break;
      }

      char *occurence = strstr(buffer, "[PROMPT]");

      if(occurence) *occurence = '\0';

      sendMessage(fd, RES_OK | DATA, buffer);
    }
  
    kill(shell_pid, SIGTERM);
    waitpid(shell_pid, NULL, 0);
  }
}

void cleanup(){
  printf("\nCleaning up socket before exit...\n");

  if(fd >= 0){
    close(fd);
    fd = -1;
  }
}

int response(Message* msg, int expect_flags){
  int status = recvMessage(fd, msg);
  
  // Check if the message was received successfully
  if(status == MSG_MALFORMED || ntohs(msg->length) != strlen(msg->data)){
    printf("[Client]: Malformed message received\n");
    sendMessage(fd, msg->status | RES_ERR, "Malformed message received");
    return 0;
  }

  if(status == MSG_ERROR){
    printf("[Client]: Error receiving message\n");
    sendMessage(fd, msg->status | RES_ERR, "Error message received");

    return 0;
  }

  if(status == MSG_PEER_CLOSED){
    printf("[Client]: Client closed the connection\n");
    return 0;
  }

  // Check for error flag in the message status
  if(hasFlag(msg->status, ERR_BIT)){
    printf("[Client]: %s\n", msg->data);
    return 0;
  }

  // Check for unexpected flags in the message status
  if(!hasFlag(msg->status, expect_flags)){
    printf("[Client]: Unexpected response from server\n");
    sendMessage(fd, msg->status | RES_ERR, "Unexpected response");
    return 0;
  }
  
  return 1;
}
