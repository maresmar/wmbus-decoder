#include "wmbus_app.h"
#include "../core/wmbus_config.h"
#include "../test/wmbus_selftest.h"

#include <furi.h>

int32_t wmbus_decoder_app(void* arg) {
    UNUSED(arg);

#if WMBUS_SELFTESTS
    wmbus_run_selftests();
#endif

    return wmbus_app();
}
