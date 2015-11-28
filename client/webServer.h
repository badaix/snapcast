#include <iostream>
#include <stdio.h>
#include <evhttp.h>
#include <thread>
#include <memory>
#include <cstdint>
#include "controller.h"

class WebServer {
  public:
    void startServer(unsigned short port);
    void stopServer();
    static std::string name_;
    static std::string ip_;
    static size_t port_;
  private:
    static void set_server(evhttp_request*, void*);
    static void get_server(evhttp_request*, void*);
    void start(unsigned short port);
};
