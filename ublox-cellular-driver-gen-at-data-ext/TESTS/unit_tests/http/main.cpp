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

// The time to wait for a HTTP command to complete
#define HTTP_TIMEOUT  10000

// The HTTP echo server, as described in the
// first answer here:
// http://stackoverflow.com/questions/5725430/http-test-server-that-accepts-get-post-calls
#define HTTP_ECHO_SERVER "httpbin.org"

// The size of the test file
#define TEST_FILE_SIZE 100

// The maximum number of HTTP profiles
#define MAX_PROFILES 4

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
// A few buffers for general use
static char buf[1024];
static char buf1[sizeof(buf)];

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

// Test HTTP commands
void test_http_cmd() {
    int profile;
    char * pData;

    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    profile = pDriver->httpAllocProfile();
    TEST_ASSERT(profile >= 0);

    pDriver->httpSetTimeout(profile, HTTP_TIMEOUT);

    // Set up the server to talk to
    TEST_ASSERT(pDriver->httpSetPar(profile, UbloxATCellularInterfaceExt::HTTP_SERVER_NAME, HTTP_ECHO_SERVER));

    // Check HTTP head request
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_HEAD,
                                     "/headers",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "Content-Length:") != NULL);

    // Check HTTP get request
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_GET,
                                     "/get",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "\"http://httpbin.org/get\"") != NULL);

    // Check HTTP delete request
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_DELETE,
                                     "/delete",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "\"http://httpbin.org/delete\"") != NULL);

    // Check HTTP put request (this will fail as the echo server doesn't support it)
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_PUT,
                                     "/put",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) != NULL);

    // Check HTTP post request with data
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_POST_DATA,
                                     "/post",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "\"http://httpbin.org/post\"") != NULL);

    // Check HTTP post request with a file, also checking that writing the response
    // to a named file works
    for (int x = 0; x < TEST_FILE_SIZE; x++) {
        buf[x] = (x % 10) + 0x30;
    }
    pDriver->delFile("post_test.txt");
    TEST_ASSERT(pDriver->writeFile("post_test.txt", buf, TEST_FILE_SIZE) == TEST_FILE_SIZE);

    // This may fail if rsp.txt happens to be sitting around from a previous run
    // so don't check the return value
    pDriver->delFile("rsp.txt");

    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_POST_FILE,
                                     "/post",
                                     "rsp.txt", "post_test.txt",
                                     UbloxATCellularInterfaceExt::HTTP_CONTENT_TEXT, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    // Find the data in the response and check it
    pData = strstr(buf, "\"data\": \"");
    TEST_ASSERT(pData != NULL);
    pData += 9;
    for (int x = 0; x < TEST_FILE_SIZE; x++) {
        TEST_ASSERT(*(pData + x) == (x % 10) + 0x30);
    }

    // Also check that rsp.txt exists and is the same as buf
    pDriver->readFile("rsp.txt", buf1, sizeof (buf1));
    memcmp(buf1, buf, sizeof (buf1));
    TEST_ASSERT(pDriver->delFile("rsp.txt"));
    TEST_ASSERT(!pDriver->delFile("rsp.txt")); // Should fail

    TEST_ASSERT(pDriver->httpFreeProfile(profile));
    TEST_ASSERT(pDriver->disconnect() == 0);
    // Wait for printfs to leave the building or the test result string gets messed up
    wait_ms(500);
}

// Test HTTP with TLS
void test_http_tls() {
    int profile;
    SocketAddress address;

    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    profile = pDriver->httpAllocProfile();
    TEST_ASSERT(profile >= 0);

    pDriver->httpSetTimeout(profile, HTTP_TIMEOUT);

    // Set up the server to talk to and TLS, using the IP address this time just for variety
    TEST_ASSERT(pDriver->gethostbyname("amazon.com", &address) == 0);
    TEST_ASSERT(pDriver->httpSetPar(profile, UbloxATCellularInterfaceExt::HTTP_IP_ADDRESS, address.get_ip_address()));
    TEST_ASSERT(pDriver->httpSetPar(profile, UbloxATCellularInterfaceExt::HTTP_SECURE, "1"));

    // Check HTTP get request
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_GET,
                                     "/",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    // This is what amazon.com returns if TLS is set
    TEST_ASSERT(strstr(buf, "302 MovedTemporarily") != NULL);

    // Reset the profile and check that this now fails
    TEST_ASSERT(pDriver->httpResetProfile(profile));
    TEST_ASSERT(pDriver->httpSetPar(profile, UbloxATCellularInterfaceExt::HTTP_IP_ADDRESS, address.get_ip_address()));
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profile, UbloxATCellularInterfaceExt::HTTP_GET,
                                     "/",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    // This is what amazon.com returns if TLS is NOT set
    TEST_ASSERT(strstr(buf, "301 Moved Permanently") != NULL);

    TEST_ASSERT(pDriver->httpFreeProfile(profile));
    TEST_ASSERT(pDriver->disconnect() == 0);
    // Wait for printfs to leave the building or the test result string gets messed up
    wait_ms(500);
}

// Allocate max profiles
void test_alloc_profiles() {
    int profiles[MAX_PROFILES];

    TEST_ASSERT(pDriver->connect(MBED_CONF_APP_DEFAULT_PIN, MBED_CONF_APP_APN,
                                 MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD) == 0);

    // Allocate first profile and use it
    profiles[0] = pDriver->httpAllocProfile();
    TEST_ASSERT(profiles[0] >= 0);
    TEST_ASSERT(pDriver->httpSetPar(profiles[0], UbloxATCellularInterfaceExt::HTTP_SERVER_NAME, "developer.mbed.org"));

    // Check HTTP get request
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profiles[0], UbloxATCellularInterfaceExt::HTTP_GET,
                                     "/media/uploads/mbed_official/hello.txt",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "Hello world!") != NULL);

    // Check that we stop being able to get profiles at the max number
    for (int x = 1; x < sizeof (profiles) / sizeof (profiles[0]); x++) {
        profiles[x] = pDriver->httpAllocProfile();
        TEST_ASSERT(profiles[0] >= 0);
    }
    TEST_ASSERT(pDriver->httpAllocProfile() < 0);

    // Now use the last one and check that it doesn't affect the first one
    TEST_ASSERT(pDriver->httpSetPar(profiles[sizeof (profiles) / sizeof (profiles[0]) - 1],
                                    UbloxATCellularInterfaceExt::HTTP_SERVER_NAME, HTTP_ECHO_SERVER));

    // Check HTTP head request on last profile
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profiles[sizeof (profiles) / sizeof (profiles[0]) - 1],
                                     UbloxATCellularInterfaceExt::HTTP_HEAD,
                                     "/headers",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "Content-Length:") != NULL);

    // Check HTTP get request on first profile once more
    memset(buf, 0, sizeof (buf));
    TEST_ASSERT(pDriver->httpCommand(profiles[0], UbloxATCellularInterfaceExt::HTTP_GET,
                                     "/media/uploads/mbed_official/hello.txt",
                                     NULL, NULL, 0, NULL,
                                     buf, sizeof (buf)) == NULL);
    tr_debug("Received: %s", buf);
    TEST_ASSERT(strstr(buf, "Hello world!") != NULL);

    // Free the profiles again
    for (int x = 0; x < sizeof (profiles) / sizeof (profiles[0]); x++) {
        TEST_ASSERT(pDriver->httpFreeProfile(profiles[x]));
    }

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
    Case("HTTP commands", test_http_cmd),
    Case("HTTP with TLS", test_http_tls),
    Case("Alloc max profiles", test_alloc_profiles)
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
