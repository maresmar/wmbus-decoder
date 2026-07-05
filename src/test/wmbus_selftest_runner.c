#include "wmbus_selftest_i.h"

#include "../core/wmbus_config.h"

static void wmbus_selftest_log_check_result(const char* name, const char* detail, bool pass) {
    if(pass) {
        FURI_LOG_I(TAG, "%s %s", name, detail);
    } else {
        FURI_LOG_W(TAG, "%s %s", name, detail);
    }
}

static void wmbus_selftest_report_check_result(
    File* file,
    const char* name,
    const char* detail,
    bool pass) {
    if(!file || !name || !detail) return;
    wmbus_selftest_write_report_line(file, "%s %s %s\n", pass ? "PASS" : "FAIL", name, detail);
}

static void wmbus_selftest_note_summary(WmBusSelftestSummary* summary, bool pass) {
    summary->total++;
    if(pass) {
        summary->passed++;
    } else {
        summary->failed++;
    }
}

static void wmbus_selftest_run_checks(
    WmBusSelftestSummary* summary,
    bool log_results,
    File* report,
    const WmBusSelftestCheck* checks,
    size_t check_count) {
    for(size_t i = 0; i < check_count; i++) {
        char detail[WMBUS_SELFTEST_LINE_MAX] = {0};
        bool pass = checks[i].run(detail, sizeof(detail));

        wmbus_selftest_note_summary(summary, pass);
        if(log_results) wmbus_selftest_log_check_result(checks[i].name, detail, pass);
        if(report) wmbus_selftest_report_check_result(report, checks[i].name, detail, pass);
    }
}

static void
    wmbus_selftest_run_internal(WmBusSelftestSummary* summary, bool log_results, File* report) {
    for(size_t i = 0; i < wmbus_selftest_get_case_count(); i++) {
        const WmBusSelftestCase* test_case = wmbus_selftest_get_case(i);
        WmBusSelftestResult result = {0};
        bool pass = wmbus_selftest_run_case(test_case, &result);

        wmbus_selftest_note_summary(summary, pass);
        if(log_results) wmbus_selftest_log_case_result(test_case, &result, pass);
        if(report) wmbus_selftest_report_case_result(report, test_case, &result, pass);
    }

    size_t count = 0;
    const WmBusSelftestCheck* checks = wmbus_selftest_tooling_checks(&count);
    wmbus_selftest_run_checks(summary, log_results, report, checks, count);

    checks = wmbus_selftest_mode_checks(&count);
    wmbus_selftest_run_checks(summary, log_results, report, checks, count);

    checks = wmbus_selftest_parser_checks(&count);
    wmbus_selftest_run_checks(summary, log_results, report, checks, count);
}

void wmbus_run_selftests(void) {
#if WMBUS_SELFTESTS
    WmBusSelftestSummary summary = {0};
    Storage* storage = furi_record_open(RECORD_STORAGE);
    wmbus_storage_ensure_app_folder(storage);
    File* report = storage_file_alloc(storage);
    bool report_open =
        storage_file_open(report, WMBUS_SELFTEST_REPORT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    FURI_LOG_I(TAG, "selftests begin");
    if(report_open) {
        wmbus_selftest_write_report_line(report, "selftests begin\n");
    } else {
        FURI_LOG_W(TAG, "failed to open selftest report: %s", WMBUS_SELFTEST_REPORT_PATH);
    }

    wmbus_selftest_run_internal(&summary, true, report_open ? report : NULL);

    if(summary.failed == 0) {
        FURI_LOG_I(
            TAG, "selftests done total=%lu passed=%lu failed=0", summary.total, summary.passed);
        if(report_open) {
            wmbus_selftest_write_report_line(
                report,
                "selftests done total=%lu passed=%lu failed=0\n",
                summary.total,
                summary.passed);
        }
    } else {
        FURI_LOG_W(
            TAG,
            "selftests done total=%lu passed=%lu failed=%lu",
            summary.total,
            summary.passed,
            summary.failed);
        if(report_open) {
            wmbus_selftest_write_report_line(
                report,
                "selftests done total=%lu passed=%lu failed=%lu\n",
                summary.total,
                summary.passed,
                summary.failed);
        }
    }

    if(report_open) storage_file_close(report);
    storage_file_free(report);
    furi_record_close(RECORD_STORAGE);
#else
    FURI_LOG_I(TAG, "selftests disabled");
#endif
}
