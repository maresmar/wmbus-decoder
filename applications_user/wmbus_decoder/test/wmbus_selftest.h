#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
} WmBusSelftestSummary;

void wmbus_selftest_run_all(WmBusSelftestSummary* summary, bool log_results);
void wmbus_run_selftests(void);
