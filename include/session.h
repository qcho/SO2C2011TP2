#ifndef SESSION_H
#define SESSION_H

#include <tty.h>
#include <shell.h>
#include <driver/video.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <permission/user.h>

#define NAME_MAX_LENGTH	20
#define PASS_MAX_LENGTH	20

void session_login();

int session_isLoggedIn();

const char* session_getName();

void session_logout();

#endif
