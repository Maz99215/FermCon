// =====================================================================
// EXTRAIT à fusionner dans ton WebServerManager réel.
// Regroupe les routes REST JSON : profil de température + fermentation.
// Style pointeurs, membres _server / _profileManager / _fermentationInfo /
// _configStore. Adapte les noms à ton WebServerManager existant.
// Auth : remplace authenticate("user","pass") par tes identifiants (Config).
// =====================================================================
#include "WebServerManager.h"
#include "ProfileManager.h"
#include "FermentationInfo.h"
#include "ConfigStore.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Constructeur unifié (profil + fermentation)
WebServerManager::WebServerManager(AsyncWebServer* server,
                                   ProfileManager* profileManager,
                                   FermentationInfo* fermentationInfo,
                                   ConfigStore* configStore)
    : _server(server),
      _profileManager(profileManager),
      _fermentationInfo(fermentationInfo),
      _configStore(configStore) {}

void WebServerManager::setupRoutes() {
    // ----------------------------------------------------------------
    // PROFIL DE TEMPÉRATURE
    // ----------------------------------------------------------------
    _server->on("/api/profile", HTTP_GET, [this](AsyncWebServerRequest* request) {
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        JsonDocument doc;
        JsonObject profileObj = doc.to<JsonObject>();
        _profileManager->toJson(profileObj);
        profileObj["currentStep"] = _profileManager->getCurrentStepInfo();
        profileObj["setpoint"]    = _profileManager->getCurrentSetpoint();
        profileObj["active"]      = _profileManager->isActive();
        serializeJson(doc, *response);
        request->send(response);
    });

    _server->on("/api/profile", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("body", true)) {
            request->send(400, "text/plain", "Missing body");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, request->getParam("body", true)->value())) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }
        _profileManager->fromJson(doc.as<JsonObjectConst>());
        _configStore->saveProfile(*_profileManager);
        request->send(200, "text/plain", "Profile saved");
    });

    _server->on("/api/profile/activate", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("body", true)) {
            request->send(400, "text/plain", "Missing body");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, request->getParam("body", true)->value())) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }
        bool active = doc["active"];
        _profileManager->setActive(active);
        if (active) _profileManager->start();
        else        _profileManager->stop();
        _configStore->saveProfile(*_profileManager);
        request->send(200, "text/plain", "Profile activation updated");
    });

    // ----------------------------------------------------------------
    // FERMENTATION (libellé d'étape libre + jours de fermentation)
    // ----------------------------------------------------------------
    _server->on("/api/fermentation", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["stageName"]   = _fermentationInfo->getStageName();
        doc["fermentDays"] = _fermentationInfo->getFermentDays();
        doc["started"]     = _fermentationInfo->started;
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    _server->on("/api/fermentation", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->hasParam("body", true)) {
            request->send(400, "application/json", "{\"error\":\"No body provided\"}");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, request->getParam("body", true)->value())) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        if (doc["stageName"].is<const char*>()) {
            _fermentationInfo->setStageName(doc["stageName"].as<String>());
        }
        if (doc["action"].is<const char*>()) {
            String action = doc["action"].as<String>();
            if (action == "start")      _fermentationInfo->startBatch();
            else if (action == "reset") _fermentationInfo->resetBatch();
        }
        _configStore->saveFermentation(*_fermentationInfo);

        JsonDocument resp;
        resp["stageName"]   = _fermentationInfo->getStageName();
        resp["fermentDays"] = _fermentationInfo->getFermentDays();
        resp["started"]     = _fermentationInfo->started;
        String out;
        serializeJson(resp, out);
        request->send(200, "application/json", out);
    });
}
