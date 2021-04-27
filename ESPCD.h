#ifndef ESPCD_h
#define ESPCD_h

#include <WebServer.h>

class ESPCD {
  public:
    ESPCD(char* url);
    void setup();
    void loop();
    WebServer& get_server();
  private:
    char* url;
};

#endif
