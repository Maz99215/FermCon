#include "ConfigStore.h"

ConfigStore::ConfigStore() {
    if (!LittleFS.begin()) {
        Serial.println("Erreur initialisation LittleFS");
    }
}

bool ConfigStore::saveProfile(const ProfileManager& profile) {
    File file = LittleFS.open("/profile.json", "w");
    if (!file) {
        Serial.println("Erreur ouverture fichier profile.json");
        return false;
    }

    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    profile.toJson(json);

    if (serializeJson(doc, file) == 0) {
        Serial.println("Erreur ecriture profile.json");
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool ConfigStore::loadProfile(ProfileManager& profile) {
    if (!LittleFS.exists("/profile.json")) {
        return false;
    }

    File file = LittleFS.open("/profile.json", "r");
    if (!file) {
        Serial.println("Erreur ouverture profile.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Erreur parsing profile.json");
        file.close();
        return false;
    }

    profile.fromJson(doc.as<JsonObject>());
    file.close();
    return true;
}

// ===== Persistance des métadonnées de fermentation (/fermentation.json) =====
bool ConfigStore::saveFermentation(const FermentationInfo& info) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    info.toJson(obj);

    File file = LittleFS.open("/fermentation.json", "w");
    if (!file) {
        Serial.println("Erreur ouverture fermentation.json");
        return false;
    }
    serializeJson(doc, file);
    file.close();
    return true;
}

bool ConfigStore::loadFermentation(FermentationInfo& info) {
    if (!LittleFS.exists("/fermentation.json")) {
        return false;
    }
    File file = LittleFS.open("/fermentation.json", "r");
    if (!file) {
        return false;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.println("Erreur parsing fermentation.json");
        return false;
    }
    info.fromJson(doc.as<JsonObject>());
    return true;
}
