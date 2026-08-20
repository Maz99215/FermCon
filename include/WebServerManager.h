#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "ConfigStore.h"
#include "ISpindelReceiver.h"
#include "TemperatureController.h"
#include "RelayController.h"
#include "ProfileManager.h"
#include "FermentationInfo.h"

class WebServerManager {
public:
    WebServerManager(ConfigStore* cfg, ISpindelReceiver* isp, TemperatureController* temp,
                     RelayController* relay, SystemStatus* status,
                     ProfileManager* profile, FermentationInfo* ferment);
    void begin();
    void loop();

private:
    AsyncWebServer server;
    ConfigStore* configStore;
    ISpindelReceiver* ispindelReceiver;
    TemperatureController* temperatureController;
    RelayController* relayController;
    SystemStatus* systemStatus;
    ProfileManager* profileManager;
    FermentationInfo* fermentationInfo;

    bool authenticate(AsyncWebServerRequest* request);
    void handleISpindel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleStatus(AsyncWebServerRequest* request);
    void handleConfigGet(AsyncWebServerRequest* request);
    void handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleProfileGet(AsyncWebServerRequest* request);
    void handleProfilePost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleProfileActivate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleFermentationGet(AsyncWebServerRequest* request);
    void handleFermentationPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleRestart(AsyncWebServerRequest* request);

    // Redemarrage differe demande via l'API
    volatile bool _restartRequested = false;
    unsigned long _restartDeadline = 0;
};

#endif // WEBSERVERMANAGER_H
