#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "ConfigStore.h"
#include "ISpindelReceiver.h"
#include "TemperatureController.h"
#include "ProfileManager.h"
#include "DataPublisher.h"

class WebServerManager {
public:
    // v0.4.0 : FermentationInfo* et RelayController* retires du constructeur (BE7)
    WebServerManager(ConfigStore* cfg, TemperatureController* temp,
                     ISpindelReceiver* isp, SystemStatus* status,
                     ProfileManager* profile,
                     DataPublisher* publisher);

    void begin();
    void loop();

private:
    AsyncWebServer server;
    ConfigStore* configStore;
    ISpindelReceiver* ispindelReceiver;
    TemperatureController* temperatureController;
    SystemStatus* systemStatus;
    ProfileManager* profileManager;
    DataPublisher* dataPublisher;

    bool authenticate(AsyncWebServerRequest* request);

    // Handlers
    void handleISpindel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleStatus(AsyncWebServerRequest* request);
    void handleConfigGet(AsyncWebServerRequest* request);
    void handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleProfileGet(AsyncWebServerRequest* request);
    void handleProfilePost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleProfileActivate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    void handleRestart(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);

    // Helpers
    void sendJson(AsyncWebServerRequest* request, int code, const JsonDocument& doc);
    void sendError(AsyncWebServerRequest* request, int httpCode, const char* errorCode,
                   const char* message, const char* field = nullptr,
                   float minVal = 0, float maxVal = 0);

    // Redemarrage differe
    volatile bool _restartRequested = false;
    unsigned long _restartDeadline = 0;
};

#endif // WEBSERVERMANAGER_H
