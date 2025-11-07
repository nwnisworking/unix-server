#ifndef UTILS_H
#define UTILS_H

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// This variable is defined in main.c
// Since the program may run as either server or client,
// we use a single global file descriptor to manage the active connection.
// By declaring it extern here, we can access it in socketsignal.c for signal handling.
// This allows us to close the socket gracefully on termination signals such as SIGINT, SIGTERM, or SIGQUIT.
extern int fd;

/**
 * Reset the signal handlers to their default behavior.
 */
void resetSignalHandler();

/**
 * Install custom signal handlers for termination signals like SIGINT, SIGTERM, or SIGQUIT.
 */
void installSignalHandler();

#endif // UTILS_H