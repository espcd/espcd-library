#ifndef ESPCD_h
#define ESPCD_h

class ESPCD {
  public:
    ESPCD(char* url);
    void setup();
    void loop();
  private:
    char* url;
};

#endif
