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

// IMPORTANT: these tests require

// ----------------------------------------------------------------
// COMPILE-TIME MACROS
// ----------------------------------------------------------------

// These macros can be overridden with an mbed_app.json file and
// contents of the following form:
//
//{
//    "config": {
//        "default-pin": {
//            "value": "\"my_pin\""
//        }
//}

// The credentials of the SIM in the board.
#ifndef MBED_CONF_APP_DEFAULT_PIN
// Note: this is the PIN for the SIM with ICCID
// 8944501104169548380.
# define MBED_CONF_APP_DEFAULT_PIN "5134"
#endif

// The size of file to use.
#ifndef MBED_CONF_APP_FILE_SIZE
# define MBED_CONF_APP_FILE_SIZE 42000
#endif

// The name of the file to use.
#ifndef MBED_CONF_APP_FILE_NAME
# define MBED_CONF_APP_FILE_NAME "test_file"
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
char buf[MBED_CONF_APP_FILE_SIZE];

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

// Initialise the module
void test_start() {
    TEST_ASSERT(pDriver->init(MBED_CONF_APP_DEFAULT_PIN));
}

// Write a file to the module's file system with known contents
void test_write() {

    for (int x = 0; x < sizeof (buf); x++) {
        buf[x] = (char) x;
    }

    TEST_ASSERT(pDriver->writeFile(MBED_CONF_APP_FILE_NAME, buf, sizeof (buf)) == sizeof (buf));
    TEST_ASSERT(pDriver->fileSize(MBED_CONF_APP_FILE_NAME) >= sizeof (buf));
    tr_debug("%d bytes written to file \"%s\"", sizeof (buf), MBED_CONF_APP_FILE_NAME);
}

// Read a file back from the module's file system and check the contents
void test_read() {
    memset(buf, 0, sizeof (buf));

    TEST_ASSERT(pDriver->readFile(MBED_CONF_APP_FILE_NAME, buf, sizeof (buf)) == sizeof (buf));

    tr_debug("%d bytes read from file \"%s\"", sizeof (buf), MBED_CONF_APP_FILE_NAME);

    for (int x = 0; x < sizeof (buf); x++) {
        TEST_ASSERT(buf[x] == (char) x);
    }
}

// Delete a file from the module's file system
void test_delete() {
    TEST_ASSERT(pDriver->delFile(MBED_CONF_APP_FILE_NAME));
    tr_debug("File \"%s\" deleted", MBED_CONF_APP_FILE_NAME);
}

// ----------------------------------------------------------------
// TEST ENVIRONMENT
// ----------------------------------------------------------------

// Setup the test environment
utest::v1::status_t test_setup(const size_t number_of_cases) {
    // Setup Greentea with a timeout
    GREENTEA_SETUP(180, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

// Test cases
Case cases[] = {
    Case("Start", test_start),
    Case("Write file", test_write),
    Case("Read file", test_read),
    Case("Delete file", test_delete)
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
