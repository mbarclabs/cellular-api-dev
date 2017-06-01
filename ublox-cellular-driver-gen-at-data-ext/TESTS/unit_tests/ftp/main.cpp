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

// FTP server name
#ifndef MBED_CONF_APP_FTP_SERVER
# error "Must define an FTP server name to run these tests"
#endif

// User name on the FTP server
#ifndef MBED_CONF_APP_FTP_USERNAME
# define MBED_CONF_APP_FTP_SERVER_USERNAME ""
#endif

// Password on the FTP server
#ifndef MBED_CONF_APP_FTP_PASSWORD
# define MBED_CONF_APP_FTP_SERVER_PASSWORD ""
#endif

// Additional user account on the FTP server, if required
#ifndef MBED_CONF_APP_FTP_ADDITIONAL_ACCOUNT
# define MBED_CONF_APP_FTP_ADDITIONAL_ACCOUNT ""
#endif

// The size of file to use.
#ifndef MBED_CONF_APP_FILE_SIZE
# define MBED_CONF_APP_FILE_SIZE 42000
#endif

// The name of the file to use.
#ifndef MBED_CONF_APP_FILE_NAME
# define MBED_CONF_APP_FILE_NAME "ftp_test_file"
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
// A buffer for general use
static char buf[MBED_CONF_APP_FILE_SIZE];

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

// Write a file to the module's file system with known contents
void createFile(const char * filename) {

    for (unsigned int x = 0; x < sizeof (buf); x++) {
        buf[x] = (char) x;
    }

    TEST_ASSERT(pDriver->writeFile(filename, buf, sizeof (buf)) == sizeof (buf));
    tr_debug("%d bytes written to file \"%s\"", sizeof (buf), filename);
}

// Read a file back from the module's file system and check the contents
void checkFile(const char * filename) {
    memset(buf, 0, sizeof (buf));

    TEST_ASSERT(pDriver->readFile(filename, buf, sizeof (buf)) == sizeof (buf));

    tr_debug("%d bytes read from file \"%s\"", sizeof (buf), filename);

    for (unsigned int x = 0; x < sizeof (buf); x++) {
        TEST_ASSERT(buf[x] == (char) x);
    }
}

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Test FTP commands
void test_ftp_cmd() {
    SocketAddress address;

    TEST_ASSERT(pDriver->init(MBED_CONF_APP_DEFAULT_PIN));

    // Reset parameters to default to begin with
    TEST_ASSERT(pDriver->ftpResetPar());

    // Create a file in the modem file system
    createFile(MBED_CONF_APP_FILE_NAME);

    // Set up the FTP server parameters
    TEST_ASSERT(pDriver->ftpSetPar(UbloxCellularDriverGenAtDataExt::FTP_SERVER_NAME,
                                   MBED_CONF_APP_FTP_SERVER));
    TEST_ASSERT(pDriver->ftpSetPar(UbloxCellularDriverGenAtDataExt::FTP_USER_NAME,
                                   MBED_CONF_APP_FTP_USERNAME));
    TEST_ASSERT(pDriver->ftpSetPar(UbloxCellularDriverGenAtDataExt::FTP_PASSWORD,
                                   MBED_CONF_APP_FTP_PASSWORD));
    TEST_ASSERT(pDriver->ftpSetPar(UbloxCellularDriverGenAtDataExt::FTP_MODE,
                                   "1"));  // Use passive as active is often
                                           // not allowed over cellular

    // Now connect to the network
    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    // Log in to the FTP server
    TEST_ASSERT(pDriver->ftpCommand(UbloxCellularDriverGenAtDataExt::FTP_LOGIN));

    // Put the file
    //TEST_ASSERT(pDriver->ftpCommand(UbloxCellularDriverGenAtDataExt::FTP_PUT_FILE,
    //                                MBED_CONF_APP_FILE_NAME));

    // Log out from the FTP server
    TEST_ASSERT(pDriver->ftpCommand(UbloxCellularDriverGenAtDataExt::FTP_LOGOUT));

    TEST_ASSERT(pDriver->disconnect() == 0);
    wait_ms(500); // Wait for printfs to leave the building
                  // or the test output strings can get messed up
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
    Case("FTP commands", test_ftp_cmd)
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
