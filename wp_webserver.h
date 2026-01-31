#pragma once
#include <WebServer.h>

extern WebServer server;

typedef String (*CmdCallback)(const String &cmd);
typedef String (*StatusCallback)();

void WebServerModule_begin();
void WebServerModule_handle();
void WebServerModule_setCommandCallback(CmdCallback cb);
void WebServerModule_setStatusCallback(StatusCallback cb);