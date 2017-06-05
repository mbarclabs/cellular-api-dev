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

// A number to send SMS messages to which will respond
// to the instruction given in the messages.
// IMPORTANT: spaces in the string are NOT allowed
#ifndef MBED_CONF_APP_SMS_DESTINATION
# error "Must define a destination number to use for SMS testing (and someone must be there to reply); the number must contain no spaces and should be in international format"
#endif

// The message to send.
#ifndef MBED_CONF_APP_SMS_SEND_CONTENTS
# define MBED_CONF_APP_SMS_SEND_CONTENTS "Please reply to this message within 60 seconds with the single word ACK (in upper case)."
#endif

// The number of milliseconds to wait for a reply to the sent SMS.
#ifndef MBED_CONF_APP_SMS_RECEIVE_TIMEOUT
# define MBED_CONF_APP_SMS_RECEIVE_TIMEOUT 60000
#endif

// The string that the reply must contain.
#ifndef MBED_CONF_APP_SMS_RECEIVE_CONTENTS
# define MBED_CONF_APP_SMS_RECEIVE_CONTENTS "ACK"
#endif

// ----------------------------------------------------------------
// PRIVATE VARIABLES
// ----------------------------------------------------------------

// Lock for debug prints
static Mutex mtx;

// An instance of the generic cellular class
static UbloxCellularDriverGen *pDriver =
       new UbloxCellularDriverGen(MDMTXD, MDMRXD,
                                  MBED_CONF_UBLOX_CELL_BAUD_RATE,
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

// ----------------------------------------------------------------
// TESTS
// ----------------------------------------------------------------

// Register with the network
void test_start() {
    TEST_ASSERT(pDriver->init(MBED_CONF_APP_DEFAULT_PIN));
    TEST_ASSERT(pDriver->nwk_registration());
}

// Send an SMS message
void test_send() {
    TEST_ASSERT(pDriver->smsSend(MBED_CONF_APP_SMS_DESTINATION,
                                 MBED_CONF_APP_SMS_SEND_CONTENTS));
}

// Receive an SMS message, check it and delete it
void test_receive() {
    int numSms = 0;
    int index;
    char num[17];
    char buf[SMS_BUFFER_SIZE];
    Timer timer;

    tr_error("!!! YOU HAVE %d SECONDS to reply to the text message !!!",
             MBED_CONF_APP_SMS_RECEIVE_TIMEOUT / 1000);
    timer.start();
    while ((numSms == 0) &&
           (timer.read_ms() < MBED_CONF_APP_SMS_RECEIVE_TIMEOUT)) {
        numSms = pDriver->smsList("REC UNREAD", &index, 1);
        if (numSms == 0) {
            wait_ms(1000);
        }
    }
    timer.stop();

    TEST_ASSERT (numSms > 0);

    TEST_ASSERT(pDriver->smsRead(index, num, buf, sizeof (buf)));
    tr_debug("Received: \"%.*s\"", sizeof (buf), buf);
    TEST_ASSERT (strstr(buf, MBED_CONF_APP_SMS_RECEIVE_CONTENTS) != NULL);

    // Delete the message and check that it's gone
    numSms = pDriver->smsList();
    TEST_ASSERT(numSms > 0);
    TEST_ASSERT(pDriver->smsDelete(index));
    TEST_ASSERT(pDriver->smsList() == numSms - 1);
}

// De-register from the network
void test_end() {
    TEST_ASSERT(pDriver->nwk_deregistration());
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
    Case("Register", test_start),
    Case("SMS send", test_send),
    Case("SMS receive and delete", test_receive),
    Case("Deregister", test_end)
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
