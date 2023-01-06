#pragma once
// This is just all the includes and stuff that I need to use in multiple files
// Are all of these required? I dont even know anymore lol

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
#include <sstream>
#include <fstream>
#include <filesystem>

// Also this macro
#define perror_exit(msg) perror(msg); exit(EXIT_FAILURE);
