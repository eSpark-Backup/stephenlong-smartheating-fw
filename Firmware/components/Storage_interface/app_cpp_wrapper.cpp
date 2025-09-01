#include "LD2410_interface.hpp"
#include "Storage_interface.h"

extern "C" {
    bool get_presence_detected(void) {
        return LD2410_getPresenceDetected();
    }
}