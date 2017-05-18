#include "mbed.h"
#include "BufferedSerial.h"

//#include "TARGET_GENERIC_MODEM/generic_modem_driver/ReferenceCellularDriver.h"
#include "TARGET_UBLOX_MODEM_GENERIC_AT_DATA/ublox_modem_driver/UbloxCellularDriverGenAtData.h"
//#include "TARGET_UBLOX_MODEM_GENERIC/ublox_modem_driver/UbloxCellularDriverGenPppData.h"
#include "UDPSocket.h"
#include "FEATURE_COMMON_PAL/nanostack-libservice/mbed-client-libservice/common_functions.h"
#if defined(FEATURE_COMMON_PAL)
#include "mbed_trace.h"
#define TRACE_GROUP "MAIN"
#else
#define tr_debug(...) (void(0)) //dummies if feature common pal is not added
#define tr_info(...)  (void(0)) //dummies if feature common pal is not added
#define tr_error(...) (void(0)) //dummies if feature common pal is not added
#endif //defined(FEATURE_COMMON_PAL)

//static const char *host_name = "echo.u-blox.com";
//static const int port = 7;
static const char *host_name = "2.pool.ntp.org";
static const int port = 123;
static Mutex mtx;
static nsapi_error_t connection_down_reason = 0;

void connection_down_cb(nsapi_error_t error)
{
    switch (error) {
        case NSAPI_ERROR_CONNECTION_LOST:
        case NSAPI_ERROR_NO_CONNECTION:
            tr_debug("Carrier/Connection lost");
            break;
        case NSAPI_ERROR_CONNECTION_TIMEOUT:
            tr_debug("Connection timed out.");
            break;
        case NSAPI_ERROR_AUTH_FAILURE:
            tr_debug("Authentication failure");
            break;
    }

    connection_down_reason = error;
}

static void lock()
{
    mtx.lock();
}

static void unlock()
{
    mtx.unlock();
}

//int do_ntp(ReferenceCellularDriver *pDriver)
int do_ntp(UbloxCellularDriverGenAtData *pDriver)
//int do_ntp(UbloxCellularDriverGenPppData *pDriver)
{
    int ntp_values[12] = { 0 };
    time_t TIME1970 = 2208988800U;

    UDPSocket sock;

    int ret = sock.open(pDriver);
    if (ret) {
        tr_error("UDPSocket.open() fails, code: %d", ret);
        return -1;
    }

    SocketAddress nist;
    ret = pDriver->gethostbyname(host_name, &nist);
    if (ret) {
        tr_error("Couldn't resolve remote host: %s, code: %d", host_name, ret);
        return -1;
    }
    nist.set_port(port);

    tr_info("UDP: NIST server %s address: %s on port %d", host_name,
           nist.get_ip_address(), nist.get_port());

    memset(ntp_values, 0x00, sizeof(ntp_values));
    ntp_values[0] = '\x1b';

    sock.set_timeout(5000);

    int ret_send = sock.sendto(nist, (void*) ntp_values, sizeof(ntp_values));
    tr_debug("UDP: Sent %d Bytes to NTP server", ret_send);

    const int n = sock.recvfrom(&nist, (void*) ntp_values, sizeof(ntp_values));
    sock.close();

    if (n > 0) {
        tr_debug("UDP: Recved from NTP server %d Bytes", n);
        tr_debug("UDP: Values returned by NTP server:");
        for (size_t i = 0; i < sizeof(ntp_values) / sizeof(ntp_values[0]); ++i) {
            tr_debug("\t[%02d] 0x%08x", i,
                     (unsigned int) common_read_32_bit((uint8_t*) &(ntp_values[i])));
            if (i == 10) {
                const time_t timestamp = common_read_32_bit(
                        (uint8_t*) &(ntp_values[i])) - TIME1970;
                struct tm *local_time = localtime(&timestamp);
                if (local_time) {
                    char time_string[25];
                    if (strftime(time_string, sizeof(time_string),
                                 "%a %b %d %H:%M:%S %Y", local_time) > 0) {
                        tr_info("NTP timestamp is %s", time_string);
                    }
                }
            }
        }
        return 0;
    }

    if (n == NSAPI_ERROR_WOULD_BLOCK) {
        return -1;
    }

    return -1;
}

#if 1
int main()
{
    bool exit = false;

    //ReferenceCellularDriver *pDriver = new ReferenceCellularDriver(MDMTXD, MDMRXD,
    //                                       MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
    //                                       true);
    //UbloxCellularDriverGenPppData *pDriver = new UbloxCellularDriverGenPppData(MDMTXD, MDMRXD,
    //                                             MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
    //                                             true);
    UbloxCellularDriverGenAtData *pDriver = new UbloxCellularDriverGenAtData(MDMTXD, MDMRXD,
                                                MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
                                                true);

    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(lock);
    mbed_trace_mutex_release_function_set(unlock);

    //pDriver->set_credentials("giffgaff.com", "giffgaff");
    pDriver->set_credentials("jtm2m");
    pDriver->connection_status_cb(connection_down_cb);

    while (!exit) {
        pDriver->connect();
        if (pDriver->is_connected()) {
            do_ntp(pDriver);
        } else {
            tr_warn("Connection down: %d", connection_down_reason);

            if (connection_down_reason == NSAPI_ERROR_AUTH_FAILURE) {
                tr_debug("Authentication Error");
                exit = true;
            } else if (connection_down_reason == NSAPI_ERROR_NO_CONNECTION || NSAPI_ERROR_CONNECTION_LOST) {
                tr_debug("Carrier lost");
            } else if (connection_down_reason == NSAPI_ERROR_CONNECTION_TIMEOUT) {
                tr_debug("Connection timed out");
            }
        }

        if (!exit) {
            wait_ms(10000);
        }
    }
}
#endif
