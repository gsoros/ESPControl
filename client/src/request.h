#ifndef REQUEST_H
#define REQUEST_H

#include <ESP8266HTTPClient.h>

class Request {
   public:
    int responseBufSize = 512;

    int requestGet(char *url, char *response) {
        int httpCode = 0;
        WiFiClient client;
        HTTPClient http;
        if (http.begin(client, url)) {
            // Serial.printf("[HTTP] GET %s\n", url);
            httpCode = http.GET();
            if (httpCode <= 0) {
                Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
            } else {
                // Serial.printf("[HTTP] GET... code: %d\n", httpCode);
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                    snprintf(response, this->responseBufSize, http.getString().c_str());
                    // Serial.printf("[HTTP] Response: %s\n", response);
                }
            }
            http.end();
        } else {
            Serial.println("[HTTP] Unable to connect");
        }
        return httpCode;
    }
};

#endif