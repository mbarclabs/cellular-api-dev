#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "UbloxCellularDriverGenAtDataExt.h"
#include "UDPSocket.h"
#include "FEATURE_COMMON_PAL/nanostack-libservice/mbed-client-libservice/common_functions.h"
#include "mbed_trace.h"
#define TRACE_GROUP "TEST"

using namespace utest::v1;

// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

// These macros can be overridden with an mbed_app.json file and
// contents of the following form:
//
//{
//    "config": {
//        "apn": {
//            "value": "\"my_apn\""
//        },
//        "run-tcp-server-test": {
//            "value": 1
//        },
//        "mga-token": {
//            "value": "\"my_token\""
//        }
//}

// The credentials of the SIM in the board.
#ifndef MBED_CONF_APP_DEFAULT_PIN
// Note: this is the PIN for the SIM with ICCID
// 8944501104169548380.
# define MBED_CONF_APP_DEFAULT_PIN "5134"
#endif

// Network credentials.
#ifndef MBED_CONF_APP_APN
# define MBED_CONF_APP_APN         NULL
#endif
#ifndef MBED_CONF_APP_USERNAME
# define MBED_CONF_APP_USERNAME    NULL
#endif
#ifndef MBED_CONF_APP_PASSWORD
# define MBED_CONF_APP_PASSWORD    NULL
#endif

#ifndef MBED_CONF_APP_RUN_TCP_SERVER_TEST
#  define MBED_CONF_APP_RUN_TCP_SERVER_TEST 0
#endif

// The authentication token for TCP access to the MGA server.
#if MBED_CONF_APP_RUN_TCP_SERVER_TEST
#  ifndef MBED_CONF_APP_MGA_TOKEN
#    error "You must have a token for MGA server access to run Cell Locate with a TCP server"
#  endif
#endif

// The type of response requested
#ifndef MBED_CONF_APP_RESP_TYPE
#  define MBED_CONF_APP_RESP_TYPE 1 // CELL_DETAILED
#endif

// The maximum number of hypotheses requested
#ifndef MBED_CONF_APP_RESP_MAX_NUM_HYPOTHESIS
#  if MBED_CONF_APP_RESP_TYPE == 2 // CELL_MULTIHYP
#    define MBED_CONF_APP_RESP_MAX_NUM_HYPOTHESIS 16
#  else
#    define MBED_CONF_APP_RESP_MAX_NUM_HYPOTHESIS 1
#  endif
#endif

#ifndef MBED_CONF_APP_MGA_TOKEN
#  define MBED_CONF_APP_MGA_TOKEN "0"
#endif

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex mtx;

// An instance of the cellular interface
static UbloxCellularDriverGenAtDataExt *pDriver =
       new UbloxCellularDriverGenAtDataExt(MDMTXD, MDMRXD,
                                           MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
                                           true);

// ----------------------------------------------------------------
// PRIVATE FUNCTIONS
// ----------------------------------------------------------------

// Locks for debug prints
static void lock()
{
    mtx.lock();
}

static void unlock()
{
    mtx.unlock();
}

static void printCellLocateData(UbloxCellularDriverGenAtDataExt::CellLocData *pData)
{
    char timeString[25];

    tr_debug("Cell Locate data:");
    if (strftime(timeString, sizeof(timeString), "%a %b %d %H:%M:%S %Y", (const tm *) &(pData->time)) > 0) {
        tr_debug("  time:               %s", timeString);
    }
    tr_debug("  longitude:          %.6f", pData->longitude);
    tr_debug("  latitude:           %.6f", pData->latitude);
    tr_debug("  altitude:           %d metres", pData->altitude);
    tr_debug("  uncertainty:        %d metres", pData->uncertainty);
    tr_debug("  speed:              %d metres/second", pData->speed);
    tr_debug("  vertical accuracy:  %d metres/second", pData->speed);
    switch (pData->sensor) {
        case UbloxCellularDriverGenAtDataExt::CELL_LAST:
            tr_debug("  sensor type:        last");
            break;
        case UbloxCellularDriverGenAtDataExt::CELL_GNSS:
            tr_debug("  sensor type:        GNSS");
            break;
        case UbloxCellularDriverGenAtDataExt::CELL_LOCATE:
            tr_debug("  sensor type:        Cell Locate");
            break;
        case UbloxCellularDriverGenAtDataExt::CELL_HYBRID:
            tr_debug("  sensor type:        hybrid");
            break;
        default:
            tr_debug("  sensor type:        unknown");
            break;
    }
    tr_debug("  satellites used:    %d", pData->svUsed);
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test Cell Locate talking to a UDP server
void test_udp_server() {
    int numRes = 0;
    UbloxCellularDriverGenAtDataExt::CellLocData data;

    memset(&data, 0, sizeof(data));
    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    TEST_ASSERT(pDriver->cellLocSrvUdp());
    TEST_ASSERT(pDriver->cellLocConfig(1));
    TEST_ASSERT(pDriver->cellLocRequest(UbloxCellularDriverGenAtDataExt::CELL_HYBRID, 10, 100,
                                        (UbloxCellularDriverGenAtDataExt::CellRespType) MBED_CONF_APP_RESP_TYPE,
                                        MBED_CONF_APP_RESP_MAX_NUM_HYPOTHESIS));

    for (int x = 0; (numRes == 0) && (x < 10); x++) {
        numRes = pDriver->cellLocGetRes();
    }

    TEST_ASSERT(numRes > 0);
    TEST_ASSERT(pDriver->cellLocGetData(&data));

    TEST_ASSERT(data.validData);

    printCellLocateData(&data);

    TEST_ASSERT(pDriver->disconnect() == 0);
}

// Test Cell Locate talking to a TCP server
void test_tcp_server() {
    int numRes = 0;
    UbloxCellularDriverGenAtDataExt::CellLocData data;

    memset(&data, 0, sizeof(data));
    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    TEST_ASSERT(pDriver->cellLocSrvTcp(MBED_CONF_APP_MGA_TOKEN));
    TEST_ASSERT(pDriver->cellLocConfig(1));
    TEST_ASSERT(pDriver->cellLocRequest(UbloxCellularDriverGenAtDataExt::CELL_HYBRID, 10, 100,
                                        (UbloxCellularDriverGenAtDataExt::CellRespType) MBED_CONF_APP_RESP_TYPE,
                                        MBED_CONF_APP_RESP_MAX_NUM_HYPOTHESIS));

    for (int x = 0; (numRes == 0) && (x < 10); x++) {
        numRes = pDriver->cellLocGetRes();
    }

    TEST_ASSERT(numRes > 0);
    TEST_ASSERT(pDriver->cellLocGetData(&data));

    TEST_ASSERT(data.validData);

    printCellLocateData(&data);

    TEST_ASSERT(pDriver->disconnect() == 0);
}

// Tidy up after testing so as not to screw with the test output strings
void test_tidy_up() {
    pDriver->deinit();
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a timeout
    GREENTEA_SETUP(540, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Cell Locate with UDP server", test_udp_server),
#if MBED_CONF_APP_RUN_TCP_SERVER_TEST
    Case("Cell Locate with TCP server", test_tcp_server),
#endif
    Case("Tidy up", test_tidy_up)
};

Specification specification(test_setup, cases);

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------

int main() {
    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(lock);
    mbed_trace_mutex_release_function_set(unlock);
    
    // Run tests
    return !Harness::run(specification);
}

// End Of File
