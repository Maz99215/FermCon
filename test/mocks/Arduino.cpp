#include "Arduino.h"

// Variable globale pour l'horloge mock
static unsigned long mock_millis = 0;

// Fonctions GPIO no-op
void pinMode(uint8_t pin, uint8_t mode) {}
void digitalWrite(uint8_t pin, uint8_t value) {}
int digitalRead(uint8_t pin) { return LOW; }

// Horloge mockable
unsigned long millis() {
    return mock_millis;
}

void mock_setMillis(unsigned long value) {
    mock_millis = value;
}

void mock_advanceMillis(unsigned long delta) {
    mock_millis += delta;
}

void mock_resetMillis() {
    mock_millis = 0;
}

// Objet Serial factice
SerialClass Serial;

void SerialClass::begin(long baud) {}
void SerialClass::print(const char* str) {}
void SerialClass::print(String str) {}
void SerialClass::print(int val) {}
void SerialClass::print(float val) {}
void SerialClass::println(const char* str) {}
void SerialClass::println(String str) {}
void SerialClass::println(int val) {}
void SerialClass::println(float val) {}

// delay no-op
void delay(unsigned long ms) {}