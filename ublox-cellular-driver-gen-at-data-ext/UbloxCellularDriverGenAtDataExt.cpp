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
#define TRACE_GROUP "UCAD"
#else
#define debug_if(_debug_trace_on, ...) (void(0)) // dummies if feature common pal is not added
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
    // +UUHTTPCR: <profile_id>,<op_code>,<param_val>
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        if (sscanf(buf, ": %d,%d,%d", &a, &b, &c) == 3) {
            _httpProfiles[a].cmd = b;          // Command
            _httpProfiles[a].result = c;       // Result
            debug_if(_debug_trace_on, "%s on profile %d, result code is %d\n", getHttpCmd((HttpCmd) b), a, c);
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

// Return a string representing an AT command.
const char * UbloxCellularDriverGenAtDataExt::getHttpCmd(HttpCmd httpCmd)
{
    const char * str = "HTTP command not recognised";

    switch (httpCmd) {
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
 * PROTECTED METHODS: Cell Locate
 **********************************************************************/

// Callback for UULOCIND handling.
void UbloxCellularDriverGenAtDataExt::UULOCIND_URC()
{
    char buf[32];
    int a, b;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +UULOCIND: <step>,<result>
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        if (sscanf(buf, " %d,%d", &a, &b) == 2) {
            switch (a) {
                case 0:
                    debug_if(_debug_trace_on, "Network scan start\n");
                    break;
                case 1:
                    debug_if(_debug_trace_on, "Network scan end\n");
                    break;
                case 2:
                    debug_if(_debug_trace_on, "Requesting data from server\n");
                    break;
                case 3:
                    debug_if(_debug_trace_on, "Received data from server\n");
                    break;
                case 4:
                    debug_if(_debug_trace_on, "Sending feedback to server\n");
                    break;
                default:
                    debug_if(_debug_trace_on, "Unknown step\n");
                    break;
            }
            switch (b) {
                case 0:
                    // No error
                    break;
                case 1:
                    debug_if(_debug_trace_on, "Wrong URL!\n");
                    break;
                case 2:
                    debug_if(_debug_trace_on, "HTTP error!\n");
                    break;
                case 3:
                    debug_if(_debug_trace_on, "Create socket error!\n");
                    break;
                case 4:
                    debug_if(_debug_trace_on, "Close socket error!\n");
                    break;
                case 5:
                    debug_if(_debug_trace_on, "Write to socket error!\n");
                    break;
                case 6:
                    debug_if(_debug_trace_on, "Read from socket error!\n");
                    break;
                case 7:
                    debug_if(_debug_trace_on, "Connection/DNS error!\n");
                    break;
                case 8:
                    debug_if(_debug_trace_on, "Authentication token problem!\n");
                    break;
                case 9:
                    debug_if(_debug_trace_on, "Generic error!\n");
                    break;
                case 10:
                    debug_if(_debug_trace_on, "User terminated!\n");
                    break;
                case 11:
                    debug_if(_debug_trace_on, "No data from server!\n");
                    break;
                default:
                    debug_if(_debug_trace_on, "Unknown result!\n");
                    break;
            }
        }
    }
}

// Callback for UULOC URC handling.
void UbloxCellularDriverGenAtDataExt::UULOC_URC()
{
    int a, b;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()

    // +UHTTPCR: <profile_id>,<op_code>,<param_val>
    if (read_at_to_char(urcBuf, sizeof (urcBuf), '\n') > 0) {
        // +UULOC: <date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,<sensor_used>,<SV_used>,<antenna_status>, <jamming_status>
        if (sscanf(urcBuf, " %d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%d,%*d,%*d",
                   &_loc[0].time.tm_mday, &_loc[0].time.tm_mon,
                   &_loc[0].time.tm_year, &_loc[0].time.tm_hour,
                   &_loc[0].time.tm_min, &_loc[0].time.tm_sec,
                   &_loc[0].latitude, &_loc[0].longitude, &_loc[0].altitude,
                   &_loc[0].uncertainty, &_loc[0].speed, &_loc[0].direction,
                   &_loc[0].verticalAcc,
                   &b, &_loc[0].svUsed) == 15) {
            debug_if(_debug_trace_on, "Position found at index 0\n");
            _loc[0].sensor = (b == 0) ? CELL_LAST : (b == 1) ? CELL_GNSS :
                             (b == 2) ? CELL_LOCATE : (b == 3) ? CELL_HYBRID : CELL_LAST;
            _loc[0].time.tm_mon -= 1;
            _loc[0].time.tm_wday = 0;
            _loc[0].time.tm_yday = 0;
            _loc[0].validData = true;
            _locExpPos=1;
            _locRcvPos++;
        // +UULOC: <sol>,<num>,<sensor_used>,<date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,,<SV_used>,<antenna_status>, <jamming_status>
        } else if (sscanf(urcBuf, " %d,%d,%d,%d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%*d,%*d",
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
                debug_if(_debug_trace_on, "Position found at index %d\n", a);

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
        } else if (sscanf(urcBuf, " %d,%d,%d,%d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%*f,%*f,%d,%*d,%*d,%*d",
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

                debug_if(_debug_trace_on, "Position found at index %d\n", a);

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
 * PUBLIC METHODS: GENERAL
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

    // URC handlers for Cell Locate
    _at->oob("+UULOCIND:", callback(this, &UbloxCellularDriverGenAtDataExt::UULOCIND_URC));
    _at->oob("+UULOC:", callback(this, &UbloxCellularDriverGenAtDataExt::UULOC_URC));
}

// Destructor.
UbloxCellularDriverGenAtDataExt::~UbloxCellularDriverGenAtDataExt()
{
}

/**********************************************************************
 * PUBLIC METHODS: HTTP
 **********************************************************************/

// Find a free profile.
int UbloxCellularDriverGenAtDataExt::httpAllocProfile()
{
    int profile = HTTP_PROF_UNUSED;
    LOCK();

    // Find a free HTTP profile
    profile = findProfile();
    debug_if(_debug_trace_on, "httpFindProfile: profile is %d\n", profile);

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

// Free a profile.
bool UbloxCellularDriverGenAtDataExt::httpFreeProfile(int profile)
{
    bool success = false;
    LOCK();

    if (IS_PROFILE(profile)) {
        debug_if(_debug_trace_on, "httpFreeProfile(%d)\n", profile);
        _httpProfiles[profile].modemHandle = HTTP_PROF_UNUSED;
        _httpProfiles[profile].timeout_ms  = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending     = false;
        _httpProfiles[profile].cmd         = -1;
        _httpProfiles[profile].result      = -1;
        success = _at->send("AT+UHTTP=%d", profile) && _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Set the blocking/timeout state of a profile.
bool UbloxCellularDriverGenAtDataExt::httpSetTimeout(int profile, int timeout_ms)
{
    bool success = false;
    LOCK();

    debug_if(_debug_trace_on, "httpSetTimeout(%d, %d)\n", profile, timeout_ms);

    if (IS_PROFILE(profile)) {
        _httpProfiles[profile].timeout_ms = timeout_ms;
        success = true;
    }

    UNLOCK();
    return success;
}

// Set a profile back to defaults.
bool UbloxCellularDriverGenAtDataExt::httpResetProfile(int httpProfile)
{
    bool success = false;
    LOCK();

    debug_if(_debug_trace_on, "httpResetProfile(%d)\n", httpProfile);
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

    debug_if(_debug_trace_on, "httpSetPar(%d, %d, \"%s\")\n", httpProfile, httpOpCode, httpInPar);
    if (IS_PROFILE(httpProfile)) {
        LOCK();

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
                debug_if(_debug_trace_on, "httpSetPar: unknown httpOpCode %d\n", httpOpCode);
                break;
        }

        UNLOCK();
    }

    return success;
}

// Perform an HTTP command.
bool UbloxCellularDriverGenAtDataExt::httpCommand(int httpProfile,
                                                  HttpCmd httpCmd,
                                                  const char *httpPath,
                                                  const char *rspFile,
                                                  const char *sendStr,
                                                  int httpContentType,
                                                  const char *httpCustomPar,
                                                  char *buf, int len)
{
    bool atSuccess = false;
    bool success = false;
    int at_timeout = _at_timeout;
    char defaultFilename[] = "http_last_response_x";

    debug_if(_debug_trace_on, "%s\n", getHttpCmd(httpCmd));

    if (IS_PROFILE(httpProfile)) {
        LOCK();

        if (rspFile == NULL) {
            sprintf(defaultFilename + sizeof (defaultFilename) - 2, "%1d", httpProfile);
            rspFile = defaultFilename;
        }

        switch (httpCmd) {
            case HTTP_HEAD:
                atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, httpCmd,
                                      httpPath, rspFile) &&
                            _at->recv("OK");
                break;
            case HTTP_GET:
                atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, httpCmd,
                                      httpPath, rspFile) &&
                            _at->recv("OK");
                break;
            case HTTP_DELETE:
                atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, httpCmd,
                                      httpPath, rspFile) &&
                            _at->recv("OK");
                break;
            case HTTP_PUT:
                // In this case the parameter sendStr is a filename
                atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\"", httpProfile, httpCmd,
                                      httpPath, rspFile, sendStr) &&
                            _at->recv("OK");
                break;
            case HTTP_POST_FILE:
                // In this case the parameter sendStr is a filename
                if (httpContentType != 6) {
                    atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                                          httpProfile, httpCmd, httpPath, rspFile, sendStr,
                                          httpContentType) &&
                                _at->recv("OK");
                } else {
                    atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                                          httpProfile, httpCmd, httpPath, rspFile, sendStr,
                                          httpContentType,
                                          httpCustomPar) &&
                                _at->recv("OK");
                }
                break;
            case HTTP_POST_DATA:
                // In this case the parameter sendStr is a string containing data
                if (httpContentType != 6) {
                    atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                                          httpProfile, httpCmd, httpPath, rspFile, sendStr,
                                          httpContentType) &&
                                _at->recv("OK");
                } else {
                    atSuccess = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                                          httpProfile, httpCmd, httpPath, rspFile, sendStr,
                                          httpContentType,
                                          httpCustomPar) &&
                                _at->recv("OK");
                }
                break;
            default:
                debug_if(_debug_trace_on, "HTTP command not recognised\n");
                break;
        }

        if (atSuccess) {
            Timer timer;

            at_set_timeout(1000);
            _httpProfiles[httpProfile].pending = true;
            _httpProfiles[httpProfile].result = -1;

            // Waiting for unsolicited result code
            while (_httpProfiles[httpProfile].pending) {
                if (_httpProfiles[httpProfile].result != -1) {
                    // Received unsolicited: starting its analysis
                    _httpProfiles[httpProfile].pending = false;
                    if (_httpProfiles[httpProfile].result == 1) {
                        // HTTP command successfully executed
                        if (readFile(rspFile, buf, len) >= 0) {
                            success = true;
                        }
                    }
                } else if (!TIMEOUT(timer, _httpProfiles[httpProfile].timeout_ms)) {
                    // Wait for URCs
                    _at->recv(UNNATURAL_STRING);
                } else  {
                    _httpProfiles[httpProfile].pending = false;
                }
            }

            at_set_timeout(at_timeout);

            if (!success) {
                debug_if(_debug_trace_on, "%s: ERROR\n", getHttpCmd(httpCmd));
            }

        }

        UNLOCK();
    }

    return success;
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

// Configure Cell Locate location sensor.
bool UbloxCellularDriverGenAtDataExt::cellLocConfig(int scanMode)
{
    bool success;
    LOCK();

    success = _at->send("AT+ULOCCELL=%d", scanMode) &&
              _at->recv("OK");

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

        // Switch on the URC
        if (_at->send("AT+ULOCIND=1") && _at->recv("OK")) {
            // Switch on Cell Locate
            success = _at->send("AT+ULOC=2,%d,%d,%d,%d,%d", sensor, type, timeout, accuracy, hypothesis) &&
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
    int at_timeout = _at_timeout;

    at_set_timeout(1000);
    // Wait for URCs
    _at->recv(UNNATURAL_STRING);
    at_set_timeout(at_timeout);

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
