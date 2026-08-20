#include "ConfigValidator.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Helper : validation d'un champ scalaire avec bornes
// ---------------------------------------------------------------------------
static bool validateUint32(uint32_t value, uint32_t minVal, uint32_t maxVal,
                           const char* fieldName, ValidationResult& result) {
    if (value < minVal || value > maxVal) {
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = fieldName;
        result.message = "hors bornes";
        result.min = (float)minVal;
        result.max = (float)maxVal;
        return false;
    }
    return true;
}

static bool validateFloat(float value, float minVal, float maxVal,
                          const char* fieldName, ValidationResult& result) {
    if (value < minVal || value > maxVal) {
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = fieldName;
        result.message = "hors bornes";
        result.min = minVal;
        result.max = maxVal;
        return false;
    }
    return true;
}

static bool validateInt(int value, int minVal, int maxVal,
                        const char* fieldName, ValidationResult& result) {
    if (value < minVal || value > maxVal) {
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = fieldName;
        result.message = "hors bornes";
        result.min = (float)minVal;
        result.max = (float)maxVal;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// validateConfig : verification des bornes individuelles
// ---------------------------------------------------------------------------
bool validateConfig(const SystemConfig& config, ValidationResult& result) {
    result.ok = true;

    // 12 parametres runtime bornes
    if (!validateFloat(config.setpoint, SETPOINT_MIN, SETPOINT_MAX, "setpoint", result)) return false;
    if (!validateFloat(config.hysteresis, HYSTERESIS_MIN, HYSTERESIS_MAX, "hysteresis", result)) return false;
    if (!validateFloat(config.temp_offset, TEMP_OFFSET_MIN, TEMP_OFFSET_MAX, "temp_offset", result)) return false;
    if (!validateUint32(config.min_compressor_delay, MIN_COMPRESSOR_DELAY_MIN, MIN_COMPRESSOR_DELAY_MAX, "min_compressor_delay", result)) return false;
    if (!validateUint32(config.cool_min_on_s, COOL_MIN_ON_S_MIN, COOL_MIN_ON_S_MAX, "cool_min_on_s", result)) return false;
    if (!validateUint32(config.heat_min_on_s, HEAT_MIN_ON_S_MIN, HEAT_MIN_ON_S_MAX, "heat_min_on_s", result)) return false;
    if (!validateUint32(config.max_on_timeout_s, MAX_ON_TIMEOUT_S_MIN, MAX_ON_TIMEOUT_S_MAX, "max_on_timeout_s", result)) return false;
    if (!validateUint32(config.temp_read_interval_ms, TEMP_READ_INTERVAL_MS_MIN, TEMP_READ_INTERVAL_MS_MAX, "temp_read_interval_ms", result)) return false;
    if (!validateFloat(config.temp_plausible_min_c, TEMP_PLAUSIBLE_MIN_C_MIN, TEMP_PLAUSIBLE_MIN_C_MAX, "temp_plausible_min_c", result)) return false;
    if (!validateFloat(config.temp_plausible_max_c, TEMP_PLAUSIBLE_MAX_C_MIN, TEMP_PLAUSIBLE_MAX_C_MAX, "temp_plausible_max_c", result)) return false;
    if (!validateUint32(config.temp_fault_trip_s, TEMP_FAULT_TRIP_S_MIN, TEMP_FAULT_TRIP_S_MAX, "temp_fault_trip_s", result)) return false;
    if (!validateUint32(config.temp_fault_clear_s, TEMP_FAULT_CLEAR_S_MIN, TEMP_FAULT_CLEAR_S_MAX, "temp_fault_clear_s", result)) return false;

    // mqtt_port
    if (!validateInt(config.mqtt_port, MQTT_PORT_MIN, MQTT_PORT_MAX, "mqtt_port", result)) return false;

    // Contraintes croisees C1 a C5
    // C1 : temp_plausible_max_c >= temp_plausible_min_c + 10.0
    if (config.temp_plausible_max_c < config.temp_plausible_min_c + 10.0f) {
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = "temp_plausible_max_c";
        result.message = "doit etre >= temp_plausible_min_c + 10";
        result.min = config.temp_plausible_min_c + 10.0f;
        result.max = TEMP_PLAUSIBLE_MAX_C_MAX;
        return false;
    }

    // C2 : temp_fault_clear_s >= temp_fault_trip_s
    if (config.temp_fault_clear_s < config.temp_fault_trip_s) {
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = "temp_fault_clear_s";
        result.message = "doit etre >= temp_fault_trip_s";
        result.min = (float)config.temp_fault_trip_s;
        result.max = (float)TEMP_FAULT_CLEAR_S_MAX;
        return false;
    }

    // C3 : si ap_enabled, alors ap_ssid non vide ET longueur(ap_password) >= 8
    if (config.ap_enabled) {
        if (strlen(config.ap_ssid) == 0) {
            result.ok = false;
            result.code = "VALIDATION_ERROR";
            result.field = "ap_ssid";
            result.message = "AP active mais SSID vide";
            return false;
        }
        if (strlen(config.ap_password) < AP_MIN_PASSWORD_LEN) {
            result.ok = false;
            result.code = "VALIDATION_ERROR";
            result.field = "ap_password";
            result.message = "mot de passe AP trop court (min 8 car.)";
            return false;
        }
    }

    // C4 : si gf_enabled, alors gf_endpoint commence par http:// ou https://
    if (config.gf_enabled) {
        if (strncmp(config.gf_endpoint, "http://", 7) != 0 &&
            strncmp(config.gf_endpoint, "https://", 8) != 0) {
            result.ok = false;
            result.code = "VALIDATION_ERROR";
            result.field = "gf_endpoint";
            result.message = "doit commencer par http:// ou https://";
            return false;
        }
    }

    // C5 : si mqtt_enabled, alors mqtt_broker non vide
    if (config.mqtt_enabled) {
        if (strlen(config.mqtt_broker) == 0) {
            result.ok = false;
            result.code = "VALIDATION_ERROR";
            result.field = "mqtt_broker";
            result.message = "MQTT active mais broker vide";
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// clampConfig : ecretage silencieux aux bornes dures
// ---------------------------------------------------------------------------
static uint32_t clampUint32(uint32_t value, uint32_t minVal, uint32_t maxVal,
                            const char* fieldName) {
    if (value < minVal) {
        Serial.printf("[CONFIG] ecretage %s: %u -> %u (borne basse)\n", fieldName, value, minVal);
        return minVal;
    }
    if (value > maxVal) {
        Serial.printf("[CONFIG] ecretage %s: %u -> %u (borne haute)\n", fieldName, value, maxVal);
        return maxVal;
    }
    return value;
}

static float clampFloat(float value, float minVal, float maxVal,
                        const char* fieldName) {
    if (value < minVal) {
        Serial.printf("[CONFIG] ecretage %s: %.2f -> %.2f (borne basse)\n", fieldName, value, minVal);
        return minVal;
    }
    if (value > maxVal) {
        Serial.printf("[CONFIG] ecretage %s: %.2f -> %.2f (borne haute)\n", fieldName, value, maxVal);
        return maxVal;
    }
    return value;
}

static int clampInt(int value, int minVal, int maxVal,
                    const char* fieldName) {
    if (value < minVal) {
        Serial.printf("[CONFIG] ecretage %s: %d -> %d (borne basse)\n", fieldName, value, minVal);
        return minVal;
    }
    if (value > maxVal) {
        Serial.printf("[CONFIG] ecretage %s: %d -> %d (borne haute)\n", fieldName, value, maxVal);
        return maxVal;
    }
    return value;
}

void clampConfig(SystemConfig& config) {
    config.setpoint             = clampFloat(config.setpoint, SETPOINT_MIN, SETPOINT_MAX, "setpoint");
    config.hysteresis           = clampFloat(config.hysteresis, HYSTERESIS_MIN, HYSTERESIS_MAX, "hysteresis");
    config.temp_offset          = clampFloat(config.temp_offset, TEMP_OFFSET_MIN, TEMP_OFFSET_MAX, "temp_offset");
    config.min_compressor_delay = clampUint32(config.min_compressor_delay, MIN_COMPRESSOR_DELAY_MIN, MIN_COMPRESSOR_DELAY_MAX, "min_compressor_delay");
    config.cool_min_on_s        = clampUint32(config.cool_min_on_s, COOL_MIN_ON_S_MIN, COOL_MIN_ON_S_MAX, "cool_min_on_s");
    config.heat_min_on_s        = clampUint32(config.heat_min_on_s, HEAT_MIN_ON_S_MIN, HEAT_MIN_ON_S_MAX, "heat_min_on_s");
    config.max_on_timeout_s     = clampUint32(config.max_on_timeout_s, MAX_ON_TIMEOUT_S_MIN, MAX_ON_TIMEOUT_S_MAX, "max_on_timeout_s");
    config.temp_read_interval_ms = clampUint32(config.temp_read_interval_ms, TEMP_READ_INTERVAL_MS_MIN, TEMP_READ_INTERVAL_MS_MAX, "temp_read_interval_ms");
    config.temp_plausible_min_c = clampFloat(config.temp_plausible_min_c, TEMP_PLAUSIBLE_MIN_C_MIN, TEMP_PLAUSIBLE_MIN_C_MAX, "temp_plausible_min_c");
    config.temp_plausible_max_c = clampFloat(config.temp_plausible_max_c, TEMP_PLAUSIBLE_MAX_C_MIN, TEMP_PLAUSIBLE_MAX_C_MAX, "temp_plausible_max_c");
    config.temp_fault_trip_s    = clampUint32(config.temp_fault_trip_s, TEMP_FAULT_TRIP_S_MIN, TEMP_FAULT_TRIP_S_MAX, "temp_fault_trip_s");
    config.temp_fault_clear_s   = clampUint32(config.temp_fault_clear_s, TEMP_FAULT_CLEAR_S_MIN, TEMP_FAULT_CLEAR_S_MAX, "temp_fault_clear_s");
    config.mqtt_port            = clampInt(config.mqtt_port, MQTT_PORT_MIN, MQTT_PORT_MAX, "mqtt_port");

    // Contraintes croisees apres ecretage individuel
    // C1
    if (config.temp_plausible_max_c < config.temp_plausible_min_c + 10.0f) {
        float old = config.temp_plausible_max_c;
        config.temp_plausible_max_c = config.temp_plausible_min_c + 10.0f;
        Serial.printf("[CONFIG] ecretage C1 temp_plausible_max_c: %.2f -> %.2f\n", old, config.temp_plausible_max_c);
    }
    // C2
    if (config.temp_fault_clear_s < config.temp_fault_trip_s) {
        uint32_t old = config.temp_fault_clear_s;
        config.temp_fault_clear_s = config.temp_fault_trip_s;
        Serial.printf("[CONFIG] ecretage C2 temp_fault_clear_s: %u -> %u\n", old, config.temp_fault_clear_s);
    }
    // C3, C4, C5 sont des contraintes booleennes, non ecretables
}

// ---------------------------------------------------------------------------
// validateProfileStep
// ---------------------------------------------------------------------------
bool validateProfileStep(const ProfileStep& step, uint8_t index, ValidationResult& result) {
    result.ok = true;

    // Rejet durationS == 0
    if (step.durationS == 0) {
        char fieldBuf[32];
        snprintf(fieldBuf, sizeof(fieldBuf), "steps[%u].durationS", index);
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = fieldBuf;
        result.message = "duree nulle interdite";
        result.min = (float)STEP_DURATION_S_MIN;
        result.max = (float)STEP_DURATION_S_MAX;
        return false;
    }

    // Bornes durationS
    char fieldDur[32];
    snprintf(fieldDur, sizeof(fieldDur), "steps[%u].durationS", index);
    if (!validateUint32(step.durationS, STEP_DURATION_S_MIN, STEP_DURATION_S_MAX, fieldDur, result)) {
        return false;
    }

    // Bornes tempStart
    char fieldStart[32];
    snprintf(fieldStart, sizeof(fieldStart), "steps[%u].tempStart", index);
    if (!validateFloat(step.tempStart, STEP_TEMP_C_MIN, STEP_TEMP_C_MAX, fieldStart, result)) {
        return false;
    }

    // Bornes tempEnd
    char fieldEnd[32];
    snprintf(fieldEnd, sizeof(fieldEnd), "steps[%u].tempEnd", index);
    if (!validateFloat(step.tempEnd, STEP_TEMP_C_MIN, STEP_TEMP_C_MAX, fieldEnd, result)) {
        return false;
    }

    // Type inconnu
    if (step.type != ProfileStep::PALIER && step.type != ProfileStep::RAMPE) {
        char fieldType[32];
        snprintf(fieldType, sizeof(fieldType), "steps[%u].type", index);
        result.ok = false;
        result.code = "VALIDATION_ERROR";
        result.field = fieldType;
        result.message = "type d'etape inconnu";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// validateStepLabel — NOUVEAU v0.4.0
// Longueur <= 23 (STEP_LABEL_MAX_LEN - 1), rejet caracteres de controle
// ---------------------------------------------------------------------------
bool validateStepLabel(const char* label, uint8_t index, ValidationResult& out) {
    out.ok = true;

    size_t len = strlen(label);
    if (len > STEP_LABEL_MAX_LEN - 1) {  // > 23
        char fieldBuf[32];
        snprintf(fieldBuf, sizeof(fieldBuf), "steps[%u].label", index);
        out.ok = false;
        out.code = "VALIDATION_ERROR";
        out.field = fieldBuf;
        out.message = "libelle trop long (max 23 caracteres)";
        out.min = 0;
        out.max = (float)(STEP_LABEL_MAX_LEN - 1);
        return false;
    }

    // Rejet des caracteres de controle (< 0x20, sauf \0 deja couvert par strlen)
    for (size_t i = 0; i < len; i++) {
        if (static_cast<unsigned char>(label[i]) < 0x20) {
            char fieldBuf[32];
            snprintf(fieldBuf, sizeof(fieldBuf), "steps[%u].label", index);
            out.ok = false;
            out.code = "VALIDATION_ERROR";
            out.field = fieldBuf;
            out.message = "caractere de controle interdit dans le libelle";
            return false;
        }
    }

    return true;
}
