#include "time_mock.h"

// Variable globale pour le temps mock
static time_t mock_epoch = 0;

// Implémentation de mock_time()
time_t mock_time(time_t* timer) {
    if (timer != nullptr) {
        *timer = mock_epoch;
    }
    return mock_epoch;
}

// Helpers pour contrôler le temps mock
extern "C" {
    void mock_setEpoch(uint32_t epoch) {
        mock_epoch = epoch;
    }
}