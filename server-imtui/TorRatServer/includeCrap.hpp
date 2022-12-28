#pragma once
// This is just all the includes and stuff that I need to use in multiple files

#include "imtui/imtui.h"
#include "imtui/imtui-impl-ncurses.h"
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <signal.h>
#include <fcntl.h>

// Also this macro
#define perror_exit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)