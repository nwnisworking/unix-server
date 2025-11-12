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
 * Check if the given status byte has the specified flag set.
 * 
 * @param status The status byte to check.
 * @param flag The flag to check for.
 * @return 1 if the flag is set, 0 otherwise.
 */
int hasFlag(uint8_t status, uint8_t flag);

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

      while(waitpid(-1, NULL, WNOHANG) > 0);

      continue;
    }
    // Child process handles the client socket
    else{
      close(fd); // Close the server socket in the child process
      fd = client_fd;

      struct timeval timeout;
      timeout.tv_sec = 100;

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

    // When the shell starts, it may output a prompt.
    while((bytes = read(parent_pipe[0], msg.data, BUFFER_SIZE - 1)) > 0){
      if(strstr(msg.data, "[PROMPT]")) break;
    }

    // After the initial prompt, that's when user commands can be processed. 
    while(1){
      size_t total = 0;

      if(!response(&msg, REQ_OK)) break;

      if(hasFlag(msg.status, CLOSE)){
        printf("[Server]: User '%s' disconnected the session\n", user->name);
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

      memset(msg.data, 0, sizeof(msg.data));
      // Read from the shell until the next prompt is displayed.
      while((bytes = read(parent_pipe[0], msg.data + total, BUFFER_SIZE - total - 1)) > 0){
        total+= bytes;
        msg.data[total] = '\0';

        if(strstr(msg.data, "[PROMPT]")) break;
      }

      char *occurence = strstr(msg.data, "[PROMPT]");

      if(occurence) *occurence = '\0';

      sendMessage(fd, RES_OK | DATA, msg.data);
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

int hasFlag(uint8_t status, uint8_t flag){
  return (status & flag) == flag;
}

int response(Message* msg, int expect_flags){
  int status = recvMessage(fd, msg);

  // Check if the message was received successfully
  if(status != MSG_OK){
    printf("[Client]: Unable to reach the client\n");
    return 0;
  }

  // Check for error flag in the message status
  if(hasFlag(msg->status, ERR_BIT)){
    printf("[Client]: %s\n", msg->data);
    // cleanup();
    // _exit(EXIT_FAILURE);
    return 0;
  }

  // Check for unexpected flags in the message status
  if(!hasFlag(msg->status, expect_flags)){
    printf("[Client]: Unexpected response from server\n");
    // cleanup();
    // _exit(EXIT_FAILURE);
    return 0;
  }
  
  return 1;
}
