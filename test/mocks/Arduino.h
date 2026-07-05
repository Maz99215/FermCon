#pragma once

#include <cstdint>
#include <string>
#include <cmath>
#include <cstdio>

// Constantes
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Classe String minimaliste
class String {
private:
    std::string data;

public:
    String() = default;
    String(const char* str) : data(str) {}
    String(const std::string& str) : data(str) {}
    String(int val) : data(std::to_string(val)) {}
    String(unsigned int val) : data(std::to_string(val)) {}
    String(long val) : data(std::to_string(val)) {}
    String(unsigned long val) : data(std::to_string(val)) {}
    String(float val, int decimals = 2) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.*f", decimals, val);
        data = buffer;
    }
    String(double val, int decimals = 2) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.*f", decimals, val);
        data = buffer;
    }

    String operator+(const String& other) const {
        return String(data + other.data);
    }
    String operator+(const char* other) const {
        return String(data + other);
    }

    String& operator+=(const String& other) {
        data += other.data;
        return *this;
    }
    String& operator+=(const char* other) {
        data += other;
        return *this;
    }

    bool operator==(const String& other) const {
        return data == other.data;
    }
    bool operator==(const char* other) const {
        return data == other;
    }

    const char* c_str() const {
        return data.c_str();
    }

    size_t length() const {
        return data.length();
    }

    operator const char*() const {
        return c_str();
    }
};

// Fonctions GPIO no-op
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);

// Horloge mockable
unsigned long millis();

// Objet Serial factice
class SerialClass {
public:
    void begin(long baud);
    void print(const char* str);
    void print(String str);
    void print(int val);
    void print(float val);
    void println(const char* str);
    void println(String str);
    void println(int val);
    void println(float val);
};
extern SerialClass Serial;

// delay no-op
void delay(unsigned long ms);
