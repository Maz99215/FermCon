#pragma once

#include <ctime>

// Macro optionnelle pour rediriger time() vers mock_time()
#ifdef UNIT_TEST
#define time(x) mock_time(x)
#endif

// Fonction de mock pour time()
time_t mock_time(time_t* timer);

// Helpers pour contrôler le temps mock
extern "C" {
    void mock_setEpoch(uint32_t epoch);
}