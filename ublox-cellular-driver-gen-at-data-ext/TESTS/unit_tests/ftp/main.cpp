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
//      "apn": {
//          "value": "\"my_apn\""
//      },
//      "ftp-server": {
//          "value": "\"test.rebex.net\""
//      },
//      "ftp-username": {
//          "value": "\"demo\""
//      },
//      "ftp-password": {
//          "value": "\"password\""
//      },
//      "ftp-use-passive": {
//          "value": true
//      },
//      "ftp-server-supports-write": {
//          "value": false
//      },
//      "ftp-filename": {
//          "value": "\"readme.txt\""
//      },
//      "ftp-dirname": {
//          "value": "\"pub\""
//      }
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

// Whether to use passive or active mode
// default to true as many servers/networks
// require this
#ifndef MBED_CONF_APP_FTP_USE_PASSIVE
# define MBED_CONF_APP_FTP_USE_PASSIVE true
#endif

// Whether the server supports FTP write operations
#ifndef MBED_CONF_APP_FTP_SERVER_SUPPORTS_WRITE
# define MBED_CONF_APP_FTP_SERVER_SUPPORTS_WRITE false
#endif

#if MBED_CONF_APP_FTP_SERVER_SUPPORTS_WRITE
// The name of the file to PUT, GET and then delete
# ifndef MBED_CONF_APP_FTP_FILENAME
#  define MBED_CONF_APP_FTP_FILENAME "test_file_delete_me"
# endif
// The name of the directory to create, CD to and then remove
# ifndef MBED_CONF_APP_FTP_DIRNAME
#  define MBED_CONF_APP_FTP_DIRNAME "test_dir_delete_me"
# endif
#else
// The name of the file to GET
# ifndef MBED_CONF_APP_FTP_FILENAME
# error "Must define the name of a file you know exists on the FTP server"
# endif
// The name of the directory to CD to
# ifndef MBED_CONF_APP_FTP_DIRNAME
# error "Must define the name of a directory you know exists on the FTP server"
# endif
#endif

// The size of file when testing PUT/GET
#ifndef MBED_CONF_APP_FTP_FILE_SIZE
#  define MBED_CONF_APP_FTP_FILE_SIZE 42000
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

    int x = pDriver->readFile(filename, buf, sizeof (buf));
    tr_debug ("File is %d bytes big", x);
    TEST_ASSERT(x == sizeof (buf));

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
    TEST_ASSERT(pDriver->ftpSetPar(UbloxATCellularInterfaceExt::FTP_SERVER_PORT,
                                   portString));
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

    // The file we will GET should appear in the directory listing
    TEST_ASSERT(strstr(buf, MBED_CONF_APP_FTP_FILENAME) > NULL);
    // As should the directory name we will change to
    TEST_ASSERT(strstr(buf, MBED_CONF_APP_FTP_DIRNAME) > NULL);
}

// Test FTP file information
void test_ftp_fileinfo() {
    // Get the info
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_FILE_INFO,
                                    MBED_CONF_APP_FTP_FILENAME, NULL, 0,
                                    buf, sizeof (buf)) == NULL);
    tr_debug("File info:\n%s", buf);

    // The file info string should at least include the file name
    TEST_ASSERT(strstr(buf, MBED_CONF_APP_FTP_FILENAME) > NULL);
}

#if MBED_CONF_APP_FTP_SERVER_SUPPORTS_WRITE

// In case a previous test failed half way, do some cleaning up first
// Note: don't check return values as these operations will fail
// if there's nothing to clean up
void test_ftp_write_cleanup() {
    pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_DELETE_FILE,
                        MBED_CONF_APP_FTP_FILENAME);
    pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_DELETE_FILE,
                        MBED_CONF_APP_FTP_FILENAME "_2");
    pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_RMDIR,
                        MBED_CONF_APP_FTP_DIRNAME);
    pDriver->delFile(MBED_CONF_APP_FTP_FILENAME);
    pDriver->delFile(MBED_CONF_APP_FTP_FILENAME "_1");
}

// Test FTP put and then get
void test_ftp_put_get() {
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

// Test FTP rename file
void test_ftp_rename() {
    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The file we are renaming to should not appear
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_FILENAME "_2") == NULL);

    // Rename the file
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_RENAME_FILE,
                                    MBED_CONF_APP_FTP_FILENAME,
                                    MBED_CONF_APP_FTP_FILENAME "_2") == NULL);

    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The new file should now exist
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_FILENAME "_2") > NULL);

}

// Test FTP delete file
void test_ftp_delete() {
    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The file we are to delete should appear in the list
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_FILENAME "_2") > NULL);

    // Delete the file
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_DELETE_FILE,
                                    MBED_CONF_APP_FTP_FILENAME "_2") == NULL);

    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The file we deleted should no longer appear in the list
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_FILENAME "_2") == NULL);
}

// Test FTP MKDIR
void test_ftp_mkdir() {
    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The directory we are to create should not appear in the list
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_DIRNAME) == NULL);

    // Create the directory
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_MKDIR,
                                    MBED_CONF_APP_FTP_DIRNAME) == NULL);

    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The directory we created should now appear in the list
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_DIRNAME) > NULL);
}

// Test FTP RMDIR
void test_ftp_rmdir() {
    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The directory we are to remove should appear in the list
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_DIRNAME) > NULL);

    // Remove the directory
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_RMDIR,
                                    MBED_CONF_APP_FTP_DIRNAME) == NULL);

    // Get a directory listing
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The directory we removed should no longer appear in the list
    TEST_ASSERT(strstr(buf,  MBED_CONF_APP_FTP_DIRNAME) == NULL);
}

#endif // MBED_CONF_APP_FTP_SERVER_SUPPORTS_WRITE

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
    // Get a directory listing
    *buf = 0;
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);

    tr_debug("Listing:\n%s", buf);

    // The listing should include the directory name we are going to move to
    TEST_ASSERT(strstr(buf, MBED_CONF_APP_FTP_DIRNAME) > NULL);

    // Change directories
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_CD,
                                    MBED_CONF_APP_FTP_DIRNAME) == NULL);
    // Get a directory listing
    *buf = 0;
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The listing should no longer include the directory name we have moved
    TEST_ASSERT(strstr(buf, MBED_CONF_APP_FTP_DIRNAME) == NULL);

    // Go back to where we were
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_CD, "..")
                == NULL);

    // Get a directory listing
    *buf = 0;
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_LS,
                                    NULL, NULL, 0, buf, sizeof (buf)) == NULL);
    tr_debug("Listing:\n%s", buf);

    // The listing should include the directory name we went to once more
    TEST_ASSERT(strstr(buf, MBED_CONF_APP_FTP_DIRNAME) > NULL);
}

#ifdef MBED_CONF_APP_FTP_FOTA_FILENAME
// Test FTP FOTA
// TODO: test not tested as I don't have a module that supports the FTP FOTA operation
void test_ftp_fota() {
    *buf = 0;
    // Do FOTA on a file
    TEST_ASSERT(pDriver->ftpCommand(UbloxATCellularInterfaceExt::FTP_FOTA_FILE,
                                    MBED_CONF_APP_FTP_FOTA_FILENAME, NULL,
                                    0, buf, sizeof (buf)) == NULL);
    tr_debug("MD5 sum: %s\n", buf);

    // Check that the 128 bit MD5 sum is now there
    TEST_ASSERT(strlen (buf) == 32);
}
#endif

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
    Case("FTP log in", test_ftp_login),
#if MBED_CONF_APP_FTP_SERVER_SUPPORTS_WRITE
    Case("Clean-up", test_ftp_write_cleanup),
    Case("FTP put and get", test_ftp_put_get),
    Case("FTP file info", test_ftp_fileinfo),
    Case("FTP rename", test_ftp_rename),
    Case("FTP make directory", test_ftp_mkdir),
    Case("FTP directory list", test_ftp_dir),
    Case("FTP delete", test_ftp_delete),
    Case("FTP change directory", test_ftp_cd),
    Case("FTP delete directory", test_ftp_rmdir),
#else
    Case("FTP directory list", test_ftp_dir),
    Case("FTP file info", test_ftp_fileinfo),
    Case("FTP get", test_ftp_get),
    Case("FTP change directory", test_ftp_cd),
#endif
#ifdef MBED_CONF_APP_FTP_FOTA_FILENAME
    Case("FTP FOTA", test_ftp_fota),
#endif
    Case("FTP log out", test_ftp_logout)
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
