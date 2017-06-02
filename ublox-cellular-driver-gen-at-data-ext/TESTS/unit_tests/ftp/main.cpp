#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "UbloxATCellularInterfaceExt.h"
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

// Whether to use SFTP or not
#ifndef MBED_CONF_APP_FTP_SECURE
# define MBED_CONF_APP_FTP_SECURE false
#endif

// Port to use on the remote server
#ifndef MBED_CONF_APP_FTP_SERVER_PORT
# if MBED_CONF_APP_FTP_SECURE
#   define MBED_CONF_APP_FTP_SERVER_PORT 22
# else
#   define MBED_CONF_APP_FTP_SERVER_PORT 21
# endif
#endif

// Whether to use passive or active node
#ifndef MBED_CONF_APP_FTP_USE_PASSIVE
# define MBED_CONF_APP_FTP_USE_PASSIVE false
#endif

// The name of the file to GET or PUT
#ifndef MBED_CONF_APP_FTP_FILENAME
# define MBED_CONF_APP_FTP_FILENAME "ftp_test_file"
#endif

// The size of file when testing PUT
#ifndef MBED_CONF_APP_FTP_FILE_SIZE
# define MBED_CONF_APP_FTP_FILE_SIZE 42000
#endif

// Whether the server supports FTP put or not
#ifndef MBED_CONF_APP_FTP_SERVER_SUPPORTS_PUT
# define MBED_CONF_APP_FTP_SERVER_SUPPORTS_PUT false
#endif

// Whether the server supports file delete or not
#ifndef MBED_CONF_APP_FTP_SERVER_SUPPORTS_DELETE
# define MBED_CONF_APP_FTP_SERVER_SUPPORTS_DELETE false
#endif

// Whether the server supports file renaming or not
#ifndef MBED_CONF_APP_FTP_SERVER_SUPPORTS_RENAME
# define MBED_CONF_APP_FTP_SERVER_SUPPORTS_RENAME false
#endif

// Whether the server supports MKDIR or not
#ifndef MBED_CONF_APP_FTP_SERVER_SUPPORTS_MKDIR
# define MBED_CONF_APP_FTP_SERVER_SUPPORTS_MKDIR false
#endif

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex mtx;

// An instance of the cellular interface
static UbloxATCellularInterfaceExt *pDriver =
       new UbloxATCellularInterfaceExt(MDMTXD, MDMRXD,
                                       MBED_CONF_UBLOX_CELL_BAUD_RATE,
                                       true);
// A buffer for general use
static char buf[MBED_CONF_APP_FTP_FILE_SIZE];

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

// Test the setting up of parameters, connection and login to an FTP session
void test_ftp_login() {
    SocketAddress address;
    char portString[10];

    sprintf(portString, "%d", MBED_CONF_APP_FTP_SERVER_PORT);

    TEST_ASSERT(pDriver->init(MBED_CONF_APP_DEFAULT_PIN));

    // Reset parameters to default to begin with
    TEST_ASSERT(pDriver->ftpResetPar());

    // Set a timeout for FTP commands
    TEST_ASSERT(pDriver->ftpSetTimeout(60000));

    // Set up the FTP server parameters
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_SERVER_NAME,
                                   MBED_CONF_APP_FTP_SERVER));
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_USER_NAME,
                                   MBED_CONF_APP_FTP_USERNAME));
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_PASSWORD,
                                   MBED_CONF_APP_FTP_PASSWORD));
#ifdef MBED_CONF_APP_FTP_ACCOUNT
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_ACCOUNT,
                                   MBED_CONF_APP_FTP_ACCOUNT));
#endif

#if MBED_CONF_APP_FTP_SECURE
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_SECURE, "1"));
#endif

    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_SERVER_PORT,
                                   portString));
#if MBED_CONF_APP_FTP_USE_PASSIVE
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_MODE, "1"));
#endif

    // Now connect to the network
    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    // Get the server IP address, purely to make sure it's there
    TEST_ASSERT(pDriver->gethostbyname(MBED_CONF_APP_FTP_SERVER, &address) == 0);
    tr_debug ("Using FTP \"%s\", which is at %s", MBED_CONF_APP_FTP_SERVER,
              address.get_ip_address());

    // Log into the FTP server
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LOGIN) == NULL);
}

// Test FTP directory listing
void test_ftp_dir() {
    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);
}

// Test FTP file information
void test_ftp_fileinfo() {
    // Get the info
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_FILE_INFO,
                                    MBED_CONF_APP_FTP_FILENAME, NULL, 0,
                                    buf, sizeof (buf)) == NULL);
    tr_debug("File info:\n%s", buf);
}

// Test FTP put and then get
void test_ftp_put_get() {
    // Make sure that the 'get' filename we're going to use
    // isn't already here (but don't assert on this one
    // as, if the file isn't there, we will get an error)
    pDriver->delFile(MBED_CONF_APP_FTP_FILENAME "_1");

    // Create the file
    createFile(MBED_CONF_APP_FTP_FILENAME);

    // Put the file
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_PUT_FILE,
                                    MBED_CONF_APP_FTP_FILENAME) == NULL);

    // Get the file
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_GET_FILE,
                                    MBED_CONF_APP_FTP_FILENAME,
                                    MBED_CONF_APP_FTP_FILENAME "_1") == NULL);

    // Check that it is the same as we sent
    checkFile(MBED_CONF_APP_FTP_FILENAME "_1");
}

// Test FTP get
void test_ftp_get() {
    // Make sure that the 'get' filename we're going to use
    // isn't already here (but don't assert on this one
    // as, if the file isn't there, we will get an error)
    pDriver->delFile(MBED_CONF_APP_FTP_FILENAME);

    // Get the file
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_GET_FILE,
                                    MBED_CONF_APP_FTP_FILENAME) == NULL);

    // Check that it has arrived
    TEST_ASSERT(pDriver->fileSize(MBED_CONF_APP_FTP_FILENAME) > 0);
}

// Test FTP change directory
void test_ftp_cd() {
// TODO
}

// Test FTP delete file
void test_ftp_delete() {
// TODO
}

// Test FTP rename file
void test_ftp_rename() {
// TODO
}

// Test FTP MKDIR
void test_ftp_mkdir() {
    // TODO
}

// Test FTP FOTA
void test_ftp_fota() {
    // TODO
}

// Test logout and disconnect from an FTP session
void test_ftp_logout() {
    // Log out from the FTP server
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LOGOUT) == NULL);

    TEST_ASSERT(pDriver->disconnect() == 0);

    // Wait for printfs to leave the building or the test result string gets messed up
    wait_ms(500);
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
    Case("FTP login", test_ftp_login),
    Case("FTP dir", test_ftp_dir),
    Case("FTP file info", test_ftp_fileinfo),
#if MBED_CONF_APP_FTP_SERVER_SUPPORTS_PUT
    Case("FTP put", test_ftp_put_get),
#else
    Case("FTP get", test_ftp_get),
#endif
#ifdef MBED_CONF_APP_FTP_CD_DIRNAME
    Case("FTP CD", test_ftp_cd),
#endif
#ifdef MBED_CONF_APP_FTP_DELETE_FILENAME
    Case("FTP delete", test_ftp_delete),
#endif
#ifdef MBED_CONF_APP_FTP_RENAME_FILENAME
    Case("FTP rename", test_ftp_rename),
#endif
#ifdef MBED_CONF_APP_FTP_MKDIR_DIRNAME
    Case("FTP MKDIR", test_ftp_mkdir),
#endif
    Case("FTP FOTA", test_ftp_fota),
    Case("FTP logout", test_ftp_logout)
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
