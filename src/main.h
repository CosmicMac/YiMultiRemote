#ifndef YMR_MAIN_H
#define YMR_MAIN_H

//@formatter:off

void getToken(WiFiClient &client, uint8_t &token);
void fireYiMessage(uint16_t id);
void capturePhoto();
void toggleVideo();
void action();
void setMode(const char* type,  const char* param);
void restartESP(uint16_t delayMs = 100);

//@formatter:on

#endif // YMR_MAIN_H
