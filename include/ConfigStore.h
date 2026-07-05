#pragma once

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ProfileManager.h"
#include "FermentationInfo.h"

// NOTE : extrait à fusionner avec ton ConfigStore réel (config WiFi/MQTT/Grainfather...).
// Seules les méthodes de persistance profil + fermentation sont détaillées ici.
class ConfigStore {
public:
    ConfigStore();
    bool save();   // config générale existante
    bool load();   // config générale existante

    // Persistance du profil de température (/profile.json)
    bool saveProfile(const ProfileManager& profile);
    bool loadProfile(ProfileManager& profile);

    // Persistance des métadonnées de fermentation (/fermentation.json)
    bool saveFermentation(const FermentationInfo& info);
    bool loadFermentation(FermentationInfo& info);

private:
    // Variables de config existantes du projet
    // ...
};
