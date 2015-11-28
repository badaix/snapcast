#include "webServer.h"
#include <iostream>

using namespace std;

std::string WebServer::name_ = "";
std::string WebServer::ip_ = "";
size_t WebServer::port_ = 0;

void WebServer::set_server(struct evhttp_request *req, void *arg)
{
    struct evkeyvalq params;
    const struct evhttp_uri *uri;
    const char *query;

// TODO: switch back to reading URI, this is getting the post body
    /*
    size_t len = evbuffer_get_length(evhttp_request_get_input_buffer(req));
    struct evbuffer *in_evb = evhttp_request_get_input_buffer(req);
    char *data = (char*) malloc(len + 1);
    evbuffer_copyout(in_evb, data, len);
    data[len] = '\0';
    cout << data << std::endl;
    */
    uri = evhttp_request_get_evhttp_uri(req);
    query = evhttp_uri_get_query(uri);
    // Parse the query for later lookups
    cout << "Changing server" << std::endl;
    if (evhttp_parse_query_str(query, &params) == -1) {
      auto *OutBuf = evhttp_request_get_output_buffer(req);
      evbuffer_add_printf(OutBuf, "Missing required parameters");
      evhttp_send_reply(req, HTTP_BADREQUEST, "", OutBuf);
      return;
    }

    const char *nameParam = evhttp_find_header(&params, "name");
    if (nameParam != NULL) {
      cout << "Got name param" << std::endl;
      std::string name(nameParam);
      WebServer::name_ = string(name);
    }
    const char *ipParam = evhttp_find_header(&params, "ip");
    if (ipParam != NULL) {
      cout << "Got ip param" << std::endl;
      std::string ip(ipParam);
      WebServer::ip_ = string(ip);
    }
    const char *portParam = evhttp_find_header(&params, "port");
    if (portParam != NULL) {
      cout << "Got port param" << std::endl;
      WebServer::port_ = atoi(portParam);
    }

    WebServer::get_server(req, arg);
}

void WebServer::get_server(struct evhttp_request *req, void *arg)
{
    auto *OutBuf = evhttp_request_get_output_buffer(req);
    std::cout << "Got " << WebServer::name_ << ":" << WebServer::ip_ << ":" << WebServer::port_ << std::endl;
    evbuffer_add_printf(OutBuf, "{\"name\": \"%s\",\"ip\": \"%s\", \"port\": %zd", WebServer::name_.c_str(), WebServer::ip_.c_str(), WebServer::port_);
    evhttp_send_reply(req, HTTP_OK, "", OutBuf);
}

 
void WebServer::start(unsigned short port) {
 if (!event_init())
  {
    std::cerr << "Failed to init libevent." << std::endl;
    return;
  }
  char const SrvAddress[] = "127.0.0.1";
  std::unique_ptr<evhttp, decltype(&evhttp_free)> Server(evhttp_start(SrvAddress, port), &evhttp_free);
  if (!Server)
  {
    std::cerr << "Failed to init http server." << std::endl;
    return;
  }
  evhttp_set_cb(Server.get(), "/set", &WebServer::set_server, NULL); 
  evhttp_set_cb(Server.get(), "/get", &WebServer::get_server, NULL); 
  //evhttp_set_gencb(Server.get(), OnReq, nullptr);
  if (event_dispatch() == -1)
  {
    std::cerr << "Failed to run message loop." << std::endl;
    return;
  }
}

void WebServer::startServer(unsigned short port) {
  std::thread([this, port] { this->start(port); }).detach();
//  std::thread(&WebServer::start, this).detach();
}

void WebServer::stopServer() {
//  if (evbase != NULL) {
//    event_base_loopexit(evbase, NULL);
//  }
  //webThread.join();
}
