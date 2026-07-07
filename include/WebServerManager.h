// include/WebServerManager.h
#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "ConfigStore.h"
#include "ISpindelReceiver.h"
#include "TemperatureController.h"
#include "RelayController.h"

class WebServerManager {
public:
    WebServerManager(ConfigStore* cfg, ISpindelReceiver* isp, TemperatureController* temp, RelayController* relay, SystemStatus* status);
    void begin();
    void loop();

private:
    AsyncWebServer server;
    ConfigStore* configStore;
    ISpindelReceiver* ispindelReceiver;
    TemperatureController* temperatureController;
    RelayController* relayController;
    SystemStatus* systemStatus;

    bool authenticate(AsyncWebServerRequest* request);
    void handleISpindel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleStatus(AsyncWebServerRequest* request);
    void handleConfigGet(AsyncWebServerRequest* request);
    void handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleManualControl(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
};

#endif // WEBSERVERMANAGER_H
