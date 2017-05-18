/* Copyright (c) 2017 ublox Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "UbloxCellularDriverGenAtDataExt.h"
#include "APN_db.h"
#if defined(FEATURE_COMMON_PAL)
#include "mbed_trace.h"
#define TRACE_GROUP "UCID"
#else
#define tr_debug(...) (void(0)) // dummies if feature common pal is not added
#define tr_info(...)  (void(0)) // dummies if feature common pal is not added
#define tr_error(...) (void(0)) // dummies if feature common pal is not added
#endif

/**********************************************************************
 * PROTECTED METHODS: HTTP
 **********************************************************************/

// Callback for HTTP result code handling.
void UbloxCellularDriverGenAtDataExt::UUHTTPCR_URC()
{
    char buf[32];
    int a, b, c;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +UHTTPCR: <profile_id>,<op_code>,<param_val>
    if (read_at_to_newline(buf, sizeof (buf)) > 0) {
        if (sscanf(buf, ": %d,%d,%d", &a, &b, &c) == 3) {
            _httpProfiles[a].cmd = b;          // Command
            _httpProfiles[a].result = c;       // Result
            tr_debug("%s for profile %d: result code is %d", getHttpCmd(b), a, c);
        }
    }
}

// Find a given profile.  NOTE: LOCK() before calling.
int UbloxCellularDriverGenAtDataExt::findProfile(int modemHandle)
{
    for (unsigned int profile = 0; profile < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0]));
         profile++) {
        if (_httpProfiles[profile].modemHandle == modemHandle) {
            return profile;
        }
    }

    return HTTP_PROF_UNUSED;
}

/**********************************************************************
 * PROTECTED METHODS: Cell Locate
 **********************************************************************/

// Callback for UULOC URC handling.
void UbloxCellularDriverGenAtDataExt::UULOC_URC()
{
    int a, b;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()

    // +UHTTPCR: <profile_id>,<op_code>,<param_val>
    if (read_at_to_newline(urcBuf, sizeof (urcBuf)) > 0) {
        // +UULOC: <date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,<sensor_used>,<SV_used>,<antenna_status>, <jamming_status>
        if (sscanf(urcBuf, ": %d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%d,%*d,%*d",
                   &_loc[0].time.tm_mday, &_loc[0].time.tm_mon,
                   &_loc[0].time.tm_year, &_loc[0].time.tm_hour,
                   &_loc[0].time.tm_min, &_loc[0].time.tm_sec,
                   &_loc[0].latitude, &_loc[0].longitude, &_loc[0].altitude,
                   &_loc[0].uncertainty, &_loc[0].speed, &_loc[0].direction,
                   &_loc[0].verticalAcc,
                   &b, &_loc[0].svUsed) == 15) {
            tr_debug ("Position found at index 0");
            _loc[0].sensor = (b == 0) ? CELL_LAST : (b == 1) ? CELL_GNSS :
                             (b == 2) ? CELL_LOCATE : (b == 3) ? CELL_HYBRID : CELL_LAST;
            _loc[0].time.tm_mon -= 1;
            _loc[0].time.tm_wday = 0;
            _loc[0].time.tm_yday = 0;
            _loc[0].validData = true;
            _locExpPos=1;
            _locRcvPos++;
        // +UULOC: <sol>,<num>,<sensor_used>,<date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,,<SV_used>,<antenna_status>, <jamming_status>
        } else if (sscanf(urcBuf, ": %d,%d,%d,%d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%*d,%*d",
                   &a, &_locExpPos, &b,
                   &_loc[CELL_MAX_HYP - 1].time.tm_mday,
                   &_loc[CELL_MAX_HYP - 1].time.tm_mon,
                   &_loc[CELL_MAX_HYP - 1].time.tm_year,
                   &_loc[CELL_MAX_HYP - 1].time.tm_hour,
                   &_loc[CELL_MAX_HYP - 1].time.tm_min,
                   &_loc[CELL_MAX_HYP - 1].time.tm_sec,
                   &_loc[CELL_MAX_HYP - 1].latitude,
                   &_loc[CELL_MAX_HYP - 1].longitude,
                   &_loc[CELL_MAX_HYP - 1].altitude,
                   &_loc[CELL_MAX_HYP - 1].uncertainty,
                   &_loc[CELL_MAX_HYP - 1].speed,
                   &_loc[CELL_MAX_HYP - 1].direction,
                   &_loc[CELL_MAX_HYP - 1].verticalAcc,
                   &_loc[CELL_MAX_HYP - 1].svUsed) == 17) {
            if (--a >= 0) {
                tr_debug ("Position found at index %d", a);

                memcpy(&_loc[a], &_loc[CELL_MAX_HYP - 1], sizeof(*_loc));

                _loc[a].sensor = (b == 0) ? CELL_LAST : (b == 1) ? CELL_GNSS :
                                 (b == 2) ? CELL_LOCATE : (b == 3) ? CELL_HYBRID : CELL_LAST;
                _loc[a].time.tm_mon -= 1;
                _loc[a].time.tm_wday = 0;
                _loc[a].time.tm_yday = 0;
                _loc[a].validData = true;
                _locRcvPos++;
            }
        //+UULOC: <sol>,<num>,<sensor_used>,<date>,<time>,<lat>,<long>,<alt>,<lat50>,<long50>,<major50>,<minor50>,<orientation50>,<confidence50>[,<lat95>,<long95>,<major95>,<minor95>,<orientation95>,<confidence95>]
        } else if (sscanf(urcBuf, ": %d,%d,%d,%d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%*f,%*f,%d,%*d,%*d,%*d",
                   &a, &_locExpPos, &b,
                   &_loc[CELL_MAX_HYP - 1].time.tm_mday,
                   &_loc[CELL_MAX_HYP - 1].time.tm_mon,
                   &_loc[CELL_MAX_HYP - 1].time.tm_year,
                   &_loc[CELL_MAX_HYP - 1].time.tm_hour,
                   &_loc[CELL_MAX_HYP - 1].time.tm_min,
                   &_loc[CELL_MAX_HYP - 1].time.tm_sec,
                   &_loc[CELL_MAX_HYP - 1].latitude,
                   &_loc[CELL_MAX_HYP - 1].longitude,
                   &_loc[CELL_MAX_HYP - 1].altitude,
                   &_loc[CELL_MAX_HYP - 1].uncertainty) == 13) {
            if (--a >= 0) {

                tr_debug ("Position found at index %d", a);

                memcpy(&_loc[a], &_loc[CELL_MAX_HYP - 1], sizeof(*_loc));

                _loc[a].sensor = (b == 0) ? CELL_LAST : (b == 1) ? CELL_GNSS :
                                 (b == 2) ? CELL_LOCATE : (b == 3) ? CELL_HYBRID : CELL_LAST;
                _loc[a].time.tm_mon -= 1;
                _loc[a].time.tm_wday = 0;
                _loc[a].time.tm_yday = 0;
                _loc[a].validData = true;
                _locRcvPos++;
            }
        }
    }
}

/**********************************************************************
 * PUBLIC METHODS: GENERIC
 **********************************************************************/

// Constructor.
UbloxCellularDriverGenAtDataExt::UbloxCellularDriverGenAtDataExt(PinName tx,
                                                                 PinName rx,
                                                                 int baud,
                                                                 bool debugOn):
                                 UbloxCellularDriverGenAtData(tx, rx, baud, debugOn)
{
    // Zero Cell Locate stuff
    _locRcvPos = 0;
    _locExpPos = 0;

    // Zero HTTP profiles
    memset(_httpProfiles, 0, sizeof(_httpProfiles));
    for (unsigned int profile = 0; profile < sizeof(_httpProfiles) / sizeof(_httpProfiles[0]);
         profile++) {
        _httpProfiles[profile].modemHandle = HTTP_PROF_UNUSED;
    }

    // URC handler for HTTP
    _at->oob("+UUHTTPCR", callback(this, &UbloxCellularDriverGenAtDataExt::UUHTTPCR_URC));

    // URC handler for Cell Locate
    _at->oob("+ULOC", callback(this, &UbloxCellularDriverGenAtDataExt::UULOC_URC));
}

// Destructor.
UbloxCellularDriverGenAtDataExt::~UbloxCellularDriverGenAtDataExt()
{
}

/**********************************************************************
 * PUBLIC METHODS: HTTP
 **********************************************************************/

// Find a free profile.
int UbloxCellularDriverGenAtDataExt::httpFindProfile()
{
    int profile = HTTP_PROF_UNUSED;
    LOCK();

    // Find a free HTTP profile
    profile = findProfile();
    tr_debug("httpFindProfile: profile is %d", profile);

    if (profile != HTTP_PROF_UNUSED) {
        _httpProfiles[profile].modemHandle = 1;
        _httpProfiles[profile].timeout_ms  = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending     = false;
        _httpProfiles[profile].cmd         = -1;
        _httpProfiles[profile].result      = -1;
    }

    UNLOCK();
    return profile;
}

// Set the blocking/timeout state of a profile.
bool UbloxCellularDriverGenAtDataExt::httpSetBlocking(int profile, int timeout_ms)
{
    bool success = false;
    LOCK();

    tr_debug("httpSetBlocking(%d, %d)", profile, timeout_ms);
    if (IS_PROFILE(profile)) {
        _httpProfiles[profile].timeout_ms = timeout_ms;
        success = true;
    }

    UNLOCK();
    return success;
}

// Set the profile so that it's suitable for commands.
bool UbloxCellularDriverGenAtDataExt::httpSetProfileForCmdMng(int profile)
{
    bool success = false;
    LOCK();

    tr_debug("httpSetProfileForCmdMng(%d)", profile);
    if (IS_PROFILE(profile)) {
        _httpProfiles[profile].pending = true;
        _httpProfiles[profile].result = -1;
        success = true;
    }

    UNLOCK();
    return success;
}

// Free a profile.
bool UbloxCellularDriverGenAtDataExt::httpFreeProfile(int profile)
{
    LOCK();

    if (IS_PROFILE(profile)) {
        tr_debug("httpFreeProfile(%d)", profile);
        _httpProfiles[profile].modemHandle = HTTP_PROF_UNUSED;
        _httpProfiles[profile].timeout_ms  = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending     = false;
        _httpProfiles[profile].cmd         = -1;
        _httpProfiles[profile].result      = -1;
    }

    UNLOCK();
    return true;
}

// Set a profile back to defaults.
bool UbloxCellularDriverGenAtDataExt::httpResetProfile(int httpProfile)
{
    bool success = false;
    LOCK();

    tr_debug("httpResetProfile(%d)", httpProfile);
    success = _at->send("AT+UHTTP=%d", httpProfile) && _at->recv("OK");

    UNLOCK();
    return success;
}

// Set HTTP parameters.
bool UbloxCellularDriverGenAtDataExt::httpSetPar(int httpProfile,
                                                 HttpOpCode httpOpCode,
                                                 const char * httpInPar)
{
    bool success = false;
    int httpInParNum = 0;
    SocketAddress address;
    LOCK();

    tr_debug("httpSetPar(%d, %d, \"%s\")", httpProfile, httpOpCode, httpInPar);
    switch(httpOpCode) {
        case HTTP_IP_ADDRESS:   // 0
            if (gethostbyname(httpInPar, &address) == NSAPI_ERROR_OK) {
                success = _at->send("AT+UHTTP=%d,%d,\"%s\"",
                                    httpProfile, httpOpCode, address.get_ip_address()) &&
                          _at->recv("OK");
            }
            break;
        case HTTP_SERVER_NAME:  // 1
        case HTTP_USER_NAME:    // 2
        case HTTP_PASSWORD:     // 3
            success = _at->send("AT+UHTTP=%d,%d,\"%s\"", httpProfile, httpOpCode, httpInPar) &&
                      _at->recv("OK");
            break;

        case HTTP_AUTH_TYPE:    // 4
        case HTTP_SERVER_PORT:  // 5
            httpInParNum = atoi(httpInPar);
            success = _at->send("AT+UHTTP=%d,%d,%d", httpProfile, httpOpCode, httpInParNum) &&
                      _at->recv("OK");
            break;

        case HTTP_SECURE:       // 6
            httpInParNum = atoi(httpInPar);
            success = _at->send("AT+UHTTP=%d,%d,%d", httpProfile, httpOpCode, httpInParNum) &&
                      _at->recv("OK");
            break;

        default:
            tr_debug("httpSetPar: unknown httpOpCode %d", httpOpCode);
            break;
    }

    UNLOCK();
    return success;
}

// Perform an HTTP command.
bool UbloxCellularDriverGenAtDataExt::httpCommand(int httpProfile,
                                                  HttpCmd httpCmdCode,
                                                  const char *httpPath,
                                                  const char *httpOut,
                                                  const char *httpIn,
                                                  int httpContentType,
                                                  const char *httpCustomPar,
                                                  char *buf, int len)
{
    bool success = false;
    LOCK();

    tr_debug("%s", getHttpCmd(httpCmdCode));
    switch (httpCmdCode) {
        case HTTP_HEAD:
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_HEAD,
                                httpPath, httpOut) &&
                      _at->recv("OK");
            break;
        case HTTP_GET:
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_GET,
                                httpPath, httpOut) &&
                      _at->recv("OK");
            break;
        case HTTP_DELETE:
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_DELETE,
                                httpPath, httpOut) &&
                      _at->recv("OK");
            break;
        case HTTP_PUT:
            // In this case the parameter httpIn is a filename
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\"", httpProfile, HTTP_PUT,
                                httpPath, httpOut, httpIn) &&
                      _at->recv("OK");
            break;
        case HTTP_POST_FILE:
            // In this case the parameter httpIn is a filename
            if (httpContentType != 6) {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                               httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn,
                               httpContentType) &&
                          _at->recv("OK");
            } else {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                               httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn,
                               httpContentType,
                               httpCustomPar) &&
                          _at->recv("OK");
            }
            break;
        case HTTP_POST_DATA:
            // In this case the parameter httpIn is a string containing data
            if (httpContentType != 6) {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                               httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn,
                               httpContentType) &&
                          _at->recv("OK");
            } else {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                               httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn,
                               httpContentType,
                               httpCustomPar) &&
                          _at->recv("OK");
            }
            break;
        default:
            tr_debug("HTTP command not recognised");
            break;
    }

    if (success) {
        Timer timer;

        timer.start();
        httpSetProfileForCmdMng(httpProfile);

        // Waiting for unsolicited result codes
        while (_httpProfiles[httpProfile].pending) {
            success = false;  // Reset variable
            if (_httpProfiles[httpProfile].result != -1) {
                // Received unsolicited: starting its analysis
                _httpProfiles[httpProfile].pending = false;
                if (_httpProfiles[httpProfile].result == 1) {
                    // HTTP command successfully executed
                    tr_debug("httpCommand: reading files with a dimension also greater than MAX_SIZE bytes");
                    if (readFileNew(httpOut, buf, len) >= 0) {
                        success = true;
                    }
                } else {
                    // HTTP command not successfully executed
                    success = false;
                }
            } else if (!TIMEOUT(timer, _httpProfiles[httpProfile].timeout_ms)) {
                // Wait for URCs
                wait_ms (1000);
            } else  {
                // Not received unsolicited and expired timer
                tr_debug("httpCommand: URC not received in time");
                success = false;
            }

            if (!success) {
                tr_debug("%s: ERROR", getHttpCmd(httpCmdCode));
                // No more while loops
                _httpProfiles[httpProfile].pending = false;
            }
        }
    }

    UNLOCK();
    return success;
}

// Return a string representing an AT command.
const char * UbloxCellularDriverGenAtDataExt::getHttpCmd(int httpCmdCode)
{
    const char * str = "HTTP command not recognised";

    switch (httpCmdCode) {
        case HTTP_HEAD:
            str = "HTTP HEAD command";
            break;
        case HTTP_GET:
            str = "HTTP GET command";
            break;
        case HTTP_DELETE:
            str = "HTTP DELETE command";
            break;
        case HTTP_PUT:
            str = "HTTP PUT command";
            break;
        case HTTP_POST_FILE:
            str = "HTTP POST file command";
            break;
        case HTTP_POST_DATA:
            str = "HTTP POST data command";
            break;
        default:
            break;
    }

    return str;
}

/**********************************************************************
 * PUBLIC METHODS: Cell Locate
 **********************************************************************/

// Configure CellLocate TCP Aiding server.
bool UbloxCellularDriverGenAtDataExt::cellLocSrvTcp(const char* token,
                                                    const char* server_1,
                                                    const char* server_2,
                                                    int days, int period,
                                                    int resolution)
{
    bool success = false;
    LOCK();

    if ((_dev_info.dev == DEV_LISA_U2_03S) || (_dev_info.dev  == DEV_SARA_U2)){
        success = _at->send("AT+UGSRV=\"%s\",\"%s\",\"%s\",%d,%d,%d",
                            server_1, server_2, token, days, period, resolution) &&
                  _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Configure CellLocate UDP Aiding server.
bool UbloxCellularDriverGenAtDataExt::cellLocSrvUdp(const char* server_1,
                                                    int port, int latency,
                                                    int mode)
{

    bool success = false;
    LOCK();

    if (_dev_info.dev != DEV_TOBY_L2) {
        success = _at->send("AT+UGAOP=\"%s\",%d,%d,%d", server_1, port, latency, mode) &&
                  _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Configure CellLocate URCs in the case of +ULOC operations.
bool UbloxCellularDriverGenAtDataExt::cellLocUnsol(int mode)
{
    bool success = false;
    LOCK();

    if (_dev_info.dev == DEV_LISA_U2_03S) {
        success = _at->send("AT+ULOCIND=%d", mode) &&
                  _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Configure Cell Locate location sensor.
bool UbloxCellularDriverGenAtDataExt::cellLocConfig(int scanMode)
{
    bool success = false;
    LOCK();

    if (_dev_info.dev != DEV_TOBY_L2) {
        success = _at->send("AT+ULOCCELL=%d", scanMode) &&
                  _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Request CellLocate.
bool UbloxCellularDriverGenAtDataExt::cellLocRequest(CellSensType sensor,
                                                     int timeout,
                                                     int accuracy,
                                                     CellRespType type,
                                                     int hypothesis)
{
    bool success = false;

    if ((hypothesis <= CELL_MAX_HYP) &&
        !((hypothesis > 1) && (type != CELL_MULTIHYP))) {

        LOCK();

        _locRcvPos = 0;
        _locExpPos = 0;

        for (int i = 0; i < hypothesis; i++) {
            _loc[i].validData = false;
        }

        if (_dev_info.dev == DEV_LISA_U2_03S) {
            success = _at->send("AT+ULOC=2,%d,%d,%d,%d,%d", sensor, type, timeout, accuracy, hypothesis) &&
                      _at->recv("OK");
            // Answers are picked up by the URC
        } else if (_dev_info.dev != DEV_TOBY_L2) {
            success = _at->send("AT+ULOC=2,%d,1,%d,%d",  sensor, timeout, accuracy) &&
                      _at->recv("OK");
            // Answers are picked up by the URC
        }

        UNLOCK();
    }

    return success;
}

// Get a position record.
bool UbloxCellularDriverGenAtDataExt::cellLocGetData(CellLocData *data, int index)
{
    bool success = false;

    if (_loc[index].validData) {
        LOCK();
        memcpy(data, &_loc[index], sizeof(*_loc));
        success = true;
        UNLOCK();
    }

    return success;
}

// Get number of position records received.
int UbloxCellularDriverGenAtDataExt::cellLocGetRes()
{
    return _locRcvPos;
}

// Get number of positions records expected to be received.
int UbloxCellularDriverGenAtDataExt::cellLocGetExpRes()
{
    int numRecords = 0;

    _at->recv("OK");

    LOCK();
    if (_locRcvPos > 0) {
        numRecords = _locExpPos;
    }
    UNLOCK();

    return numRecords;
}

// End of file
