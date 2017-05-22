#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "UbloxCellularDriverGen.h"
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

// http://www.geckobeach.com/cellular/secrets/gsmcodes.php
// https://en.wikipedia.org/wiki/Unstructured_Supplementary_Service_Data

// A few USSD commands to try, set to "" to skip
#ifndef MBED_CONF_APP_USSD_COMMAND_1
#  define MBED_CONF_APP_USSD_COMMAND_1 "*100#"
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_2
#  define MBED_CONF_APP_USSD_COMMAND_2 "*#21#"
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_3
#  define MBED_CONF_APP_USSD_COMMAND_3 "*#30#"
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_4
#  define MBED_CONF_APP_USSD_COMMAND_4 "*#31#"
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_5
#  define MBED_CONF_APP_USSD_COMMAND_5 "*#43#"
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_6
#  define MBED_CONF_APP_USSD_COMMAND_6 "*#61#"
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_7
#  define MBED_CONF_APP_USSD_COMMAND_7 ""
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_8
#  define MBED_CONF_APP_USSD_COMMAND_8 ""
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_9
#  define MBED_CONF_APP_USSD_COMMAND_9 ""
#endif

#ifndef MBED_CONF_APP_USSD_COMMAND_10
#  define MBED_CONF_APP_USSD_COMMAND_10 ""
#endif

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex mtx;

// An instance of the generic cellular class
static UbloxCellularDriverGen *pDriver =
       new UbloxCellularDriverGen(MDMTXD, MDMRXD,
                                  MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
                                  true);
// A general purpose buffer
static char buf[USSD_STRING_LENGTH + 1];

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

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test USSD
void test_ussd() {
    TEST_ASSERT(pDriver->init(MBED_CONF_APP_DEFAULT_PIN));
    TEST_ASSERT(pDriver->nwk_registration());

    if (strlen(MBED_CONF_APP_USSD_COMMAND_1) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_1);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_1, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_2) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_2);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_2, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_3) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_3);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_3, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_4) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_4);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_4, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_5) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_5);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_5, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_6) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_6);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_6, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_7) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_7);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_7, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_8) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_8);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_8, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_9) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_9);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_9, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    if (strlen(MBED_CONF_APP_USSD_COMMAND_10) > 0) {
        tr_debug("Sending : \"%s\".", MBED_CONF_APP_USSD_COMMAND_10);
        TEST_ASSERT(pDriver->ussdCommand(MBED_CONF_APP_USSD_COMMAND_10, buf, sizeof (buf)));
        tr_debug("USSD answer: \"%s\".", buf);
    }

    TEST_ASSERT(pDriver->nwk_deregistration());
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
    Case("USSD test", test_ussd)

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
