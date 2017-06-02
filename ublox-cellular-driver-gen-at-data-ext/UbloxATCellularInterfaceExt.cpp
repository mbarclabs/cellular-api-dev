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

#include "UbloxATCellularInterfaceExt.h"
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
void UbloxATCellularInterfaceExt::UUHTTPCR_URC()
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
int UbloxATCellularInterfaceExt::findProfile(int modemHandle)
{
    for (unsigned int profile = 0; profile < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0]));
         profile++) {
        if (_httpProfiles[profile].modemHandle == modemHandle) {
            return profile;
        }
    }

    return HTTP_PROF_UNUSED;
}

// Return a string representing an HTTP AT command.
const char *UbloxATCellularInterfaceExt::getHttpCmd(HttpCmd httpCmd)
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
 * PROTECTED METHODS: FTP
 **********************************************************************/

// Callback for FTP result code handling.
void UbloxATCellularInterfaceExt::UUFTPCR_URC()
{
    char buf[64];
    char md5[32];

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +UUFTPCR: <op_code>,<ftp_result>[,<md5_sum>]
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        if (sscanf(buf, ": %d,%d,%32[^\n]\n", &_lastFtpOpCodeResult, &_lastFtpResult, md5) == 3) {
            // Store the MD5 sum if we can
            if ((_ftpBuf != NULL) && (_ftpBufLen >= 32)) {
                memcpy (_ftpBuf, md5, 32);
                if (_ftpBufLen == 33) {
                    *(buf + 32) = 0; // Add a terminator if there's room
                }
            }
        }
        debug_if(_debug_trace_on, "%s result code is %d\n",
                 getFtpCmd((FtpCmd) _lastFtpOpCodeResult), _lastFtpResult);
    }
}

// Callback for FTP data handling.
void UbloxATCellularInterfaceExt::UUFTPCD_URC()
{
    char buf[32];
    char *ftpBufPtr = _ftpBuf;
    int ftpDataLen;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +UUFTPCD: <op_code>,<ftp_data_len>,<ftp_data_in_quotes>
    if (read_at_to_char(buf, sizeof(buf), '\"') > 0) {
        if (sscanf(buf, ": %d,%d,\"", &_lastFtpOpCodeData, &ftpDataLen) == 2) {
            if ((ftpBufPtr != NULL) && (_ftpBufLen > 0)) {
                if (ftpDataLen + 1 > _ftpBufLen) { // +1 for terminator
                    ftpDataLen = _ftpBufLen - 1;
                }
                ftpBufPtr += _at->read(ftpBufPtr, ftpDataLen);
                *ftpBufPtr = 0; // Add terminator
            }
        }
    }
}

// Return a string representing an FTP AT command.
const char *UbloxATCellularInterfaceExt::getFtpCmd(FtpCmd ftpCmd)
{
    const char * str = "FTP command not recognised";

    switch (ftpCmd) {
        case FTP_LOGOUT:
            str = "FTP log out command";
            break;
        case FTP_LOGIN:
            str = "FTP log in command";
            break;
        case FTP_DELETE_FILE:
            str = "FTP delete file command";
            break;
        case FTP_RENAME_FILE:
            str = "FTP rename file command";
            break;
        case FTP_GET_FILE:
            str = "FTP get file command";
            break;
        case FTP_PUT_FILE:
            str = "FTP put file command";
            break;
        case FTP_CD:
            str = "FTP change directory command";
            break;
        case FTP_MKDIR:
            str = "FTP make directory command";
            break;
        case FTP_RMDIR:
            str = "FTP remove directory command";
            break;
        case FTP_FILE_INFO:
            str = "FTP file info command";
            break;
        case FTP_LS:
            str = "FTP directory list command";
            break;
        case FTP_FOTA_FILE:
            str = "FTP FOTA file command";
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
void UbloxATCellularInterfaceExt::UULOCIND_URC()
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
void UbloxATCellularInterfaceExt::UULOC_URC()
{
    int a, b;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()

    // +UHTTPCR: <profile_id>,<op_code>,<param_val>
    if (read_at_to_char(urcBuf, sizeof (urcBuf), '\n') > 0) {
        // +UULOC: <date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,<sensor_used>,<SV_used>,<antenna_status>, <jamming_status>
        if (sscanf(urcBuf, ": %d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%d,%*d,%*d",
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
UbloxATCellularInterfaceExt::UbloxATCellularInterfaceExt(PinName tx,
                                                         PinName rx,
                                                         int baud,
                                                         bool debugOn):
                             UbloxATCellularInterface(tx, rx, baud, debugOn)
{
    // Zero HTTP stuff
    memset(_httpProfiles, 0, sizeof(_httpProfiles));
    for (unsigned int profile = 0; profile < sizeof(_httpProfiles) / sizeof(_httpProfiles[0]);
         profile++) {
        _httpProfiles[profile].modemHandle = HTTP_PROF_UNUSED;
    }

    // Zero FTP stuff
    _ftpTimeout = TIMEOUT_BLOCKING;
    _lastFtpOpCodeResult = FTP_OP_CODE_UNUSED;
    _lastFtpResult = 0;
    _lastFtpOpCodeData = FTP_OP_CODE_UNUSED;
    _ftpBuf = NULL;
    _ftpBufLen = 0;
    _ftpError.eClass = 0;
    _ftpError.eCode = 0;

    // Zero Cell Locate stuff
    _locRcvPos = 0;
    _locExpPos = 0;

    // URC handler for HTTP
    _at->oob("+UUHTTPCR", callback(this, &UbloxATCellularInterfaceExt::UUHTTPCR_URC));

    // URC handlers for FTP
    _at->oob("+UUFTPCR", callback(this, &UbloxATCellularInterfaceExt::UUFTPCR_URC));
    _at->oob("+UUFTPCD", callback(this, &UbloxATCellularInterfaceExt::UUFTPCD_URC));

    // URC handlers for Cell Locate
    _at->oob("+UULOCIND", callback(this, &UbloxATCellularInterfaceExt::UULOCIND_URC));
    _at->oob("+UULOC", callback(this, &UbloxATCellularInterfaceExt::UULOC_URC));
}

// Destructor.
UbloxATCellularInterfaceExt::~UbloxATCellularInterfaceExt()
{
}

/**********************************************************************
 * PUBLIC METHODS: HTTP
 **********************************************************************/

// Find a free profile.
int UbloxATCellularInterfaceExt::httpAllocProfile()
{
    int profile = HTTP_PROF_UNUSED;
    LOCK();

    // Find a free HTTP profile
    profile = findProfile();
    debug_if(_debug_trace_on, "httpFindProfile: profile is %d\n", profile);

    if (profile != HTTP_PROF_UNUSED) {
        _httpProfiles[profile].modemHandle = 1;
        _httpProfiles[profile].timeout     = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending     = false;
        _httpProfiles[profile].cmd         = -1;
        _httpProfiles[profile].result      = -1;
    }

    UNLOCK();
    return profile;
}

// Free a profile.
bool UbloxATCellularInterfaceExt::httpFreeProfile(int profile)
{
    bool success = false;
    LOCK();

    if (IS_PROFILE(profile)) {
        debug_if(_debug_trace_on, "httpFreeProfile(%d)\n", profile);
        _httpProfiles[profile].modemHandle = HTTP_PROF_UNUSED;
        _httpProfiles[profile].timeout     = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending     = false;
        _httpProfiles[profile].cmd         = -1;
        _httpProfiles[profile].result      = -1;
        success = _at->send("AT+UHTTP=%d", profile) && _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Set the blocking/timeout state of a profile.
bool UbloxATCellularInterfaceExt::httpSetTimeout(int profile, int timeout)
{
    bool success = false;
    LOCK();

    debug_if(_debug_trace_on, "httpSetTimeout(%d, %d)\n", profile, timeout);

    if (IS_PROFILE(profile)) {
        _httpProfiles[profile].timeout = timeout;
        success = true;
    }

    UNLOCK();
    return success;
}

// Set a profile back to defaults.
bool UbloxATCellularInterfaceExt::httpResetProfile(int httpProfile)
{
    bool success = false;
    LOCK();

    debug_if(_debug_trace_on, "httpResetProfile(%d)\n", httpProfile);
    success = _at->send("AT+UHTTP=%d", httpProfile) && _at->recv("OK");

    UNLOCK();
    return success;
}

// Set HTTP parameters.
bool UbloxATCellularInterfaceExt::httpSetPar(int httpProfile,
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
UbloxATCellularInterfaceExt::Error * UbloxATCellularInterfaceExt::httpCommand(int httpProfile,
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
    int at_timeout;
    char defaultFilename[] = "http_last_response_x";

    debug_if(_debug_trace_on, "%s\n", getHttpCmd(httpCmd));

    if (IS_PROFILE(httpProfile)) {
        LOCK();
        at_timeout = _at_timeout; // Has to be inside LOCK()s

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
            timer.start();
            while (_httpProfiles[httpProfile].pending) {
                if (_httpProfiles[httpProfile].result != -1) {
                    // Received unsolicited: starting its analysis
                    _httpProfiles[httpProfile].pending = false;
                    if (_httpProfiles[httpProfile].result == 1) {
                        // HTTP command successfully executed
                        if (readFile(rspFile, buf, len) >= 0) {
                            success = true;
                        }
                    } else {
                        // Retrieve the error class and code
                        if (_at->send("AT+UHTTPER=%d", httpProfile) &&
                            _at->recv("AT+UHTTPER=%*d,%d,%d",
                                      &(_httpProfiles[httpProfile].httpError.eClass),
                                      &(_httpProfiles[httpProfile].httpError.eCode)) &&
                            _at->recv("OK")) {
                            debug_if(_debug_trace_on, "HTTP error class %d, code %d\n",
                                    _httpProfiles[httpProfile].httpError.eClass,
                                    _httpProfiles[httpProfile].httpError.eCode);
                        }
                    }
                } else if (!TIMEOUT(timer, _httpProfiles[httpProfile].timeout)) {
                    // Wait for URCs
                    _at->recv(UNNATURAL_STRING);
                } else  {
                    _httpProfiles[httpProfile].pending = false;
                }
            }
            timer.stop();

            at_set_timeout(at_timeout);

            if (!success) {
                debug_if(_debug_trace_on, "%s: ERROR\n", getHttpCmd(httpCmd));
            }

        }

        UNLOCK();
    }

    return success ? NULL : &(_httpProfiles[httpProfile].httpError);
}

/**********************************************************************
 * PUBLIC METHODS: FTP
 **********************************************************************/

// Set the blocking/timeout for FTP.
bool UbloxATCellularInterfaceExt::ftpSetTimeout(int timeout)
{
    LOCK();
    debug_if(_debug_trace_on, "ftpSetTimeout(%d)\n", timeout);
    _ftpTimeout = timeout;
    UNLOCK();

    return true;
}

// Reset the FTP configuration back to defaults.
bool UbloxATCellularInterfaceExt::ftpResetPar()
{
    bool success = true;
    LOCK();

    debug_if(_debug_trace_on, "ftpResetPar()\n");
    for (int x = 0; success && (x < NUM_FTP_OP_CODES); x++) {
        success = _at->send("AT+UFTP=%d", x) && _at->recv("OK");
    }

    UNLOCK();
    return success;
}

// Set FTP parameters.
bool UbloxATCellularInterfaceExt::ftpSetPar(FtpOpCode ftpOpCode,
                                            const char * ftpInPar)
{
    bool success = false;
    int ftpInParNum = 0;
    LOCK();

    debug_if(_debug_trace_on, "ftpSetPar(%d, %s)\n", ftpOpCode, ftpInPar);
    switch (ftpOpCode) {
        case FTP_IP_ADDRESS:         // 0
        case FTP_SERVER_NAME:        // 1
        case FTP_USER_NAME:          // 2
        case FTP_PASSWORD:           // 3
        case FTP_ACCOUNT:            // 4
            success = _at->send("AT+UFTP=%d,\"%s\"", ftpOpCode, ftpInPar) &&
                      _at->recv("OK");
            break;
        case FTP_INACTIVITY_TIMEOUT: // 5
        case FTP_MODE:               // 6
        case FTP_SERVER_PORT:        // 7
        case FTP_SECURE:             // 8
            ftpInParNum = atoi(ftpInPar);
            success = _at->send("AT+UFTP=%d,%d", ftpOpCode, ftpInParNum) &&
                      _at->recv("OK");
            break;
        default:
            debug_if(_debug_trace_on, "ftpSetPar: unknown ftpOpCode %d\n", ftpOpCode);
            break;
    }

    UNLOCK();
    return success;
}

// Perform an FTP command.
UbloxATCellularInterfaceExt::Error * UbloxATCellularInterfaceExt::ftpCommand(FtpCmd ftpCmd,
                                                                             const char* file1,
                                                                             const char* file2,
                                                                             int offset,
                                                                             char* buf, int len)
{
    bool atSuccess = false;
    bool success = false;
    int at_timeout;
    LOCK();
    at_timeout = _at_timeout; // Has to be inside LOCK()s

    debug_if(_debug_trace_on, "%s\n", getFtpCmd(ftpCmd));
    switch (ftpCmd) {
        case FTP_LOGOUT:
        case FTP_LOGIN:
            atSuccess = _at->send("AT+UFTPC=%d", ftpCmd) && _at->recv("OK");
            break;
        case FTP_DELETE_FILE:
        case FTP_CD:
        case FTP_MKDIR:
        case FTP_RMDIR:
        case FTP_FOTA_FILE:
            atSuccess = _at->send("AT+UFTPC=%d,\"%s\"", ftpCmd, file1) &&
                        _at->recv("OK");
            break;
        case FTP_RENAME_FILE:
            atSuccess = _at->send("AT+UFTPC=%d,\"%s\",\"%s\"",
                                  ftpCmd, file1, file2) &&
                        _at->recv("OK");
            break;
        case FTP_GET_FILE:
        case FTP_PUT_FILE:
            if (file2 == NULL) {
                file2 = file1;
            }
            atSuccess = _at->send("AT+UFTPC=%d,\"%s\",\"%s\",%d",
                                  ftpCmd, file1, file2, offset) &&
                        _at->recv("OK");
            break;
        case FTP_FILE_INFO:
        case FTP_LS:
            if (file1 == NULL) {
                atSuccess = _at->send("AT+UFTPC=%d", ftpCmd) &&
                            _at->recv("OK");
            } else {
                atSuccess = _at->send("AT+UFTPC=%d,\"%s\"", ftpCmd, file1) &&
                            _at->recv("OK");
            }
            break;
        default:
            debug_if(_debug_trace_on, "FTP command not recognised/supported\n");
            break;
    }

    // Wait for the result to arrive back
    if (atSuccess) {
        Timer timer;

        at_set_timeout(1000);
        _lastFtpOpCodeData = FTP_OP_CODE_UNUSED;
        _lastFtpOpCodeResult = FTP_OP_CODE_UNUSED;
        _lastFtpResult = -1; // just for safety

        // Waiting for result to arrive
        timer.start();
        while ((_lastFtpOpCodeResult == FTP_OP_CODE_UNUSED) &&
                !TIMEOUT(timer, _ftpTimeout)) {
            _at->recv(UNNATURAL_STRING);
        }
        timer.stop();

        if ((_lastFtpOpCodeResult == ftpCmd) && (_lastFtpResult == 1)) {
            // Got a result for our FTP op code and it is good
            success = true;
        } else {
            // Retrieve the error class and code
            if (_at->send("AT+UFTPER") &&
                _at->recv("+UFTPER:%d,%d", &(_ftpError.eClass), &(_ftpError.eCode)) &&
                _at->recv("OK")) {
                debug_if(_debug_trace_on, "FTP Error class %d, code %d\n",
                         _ftpError.eClass, _ftpError.eCode);
            }
        }

        at_set_timeout(at_timeout);

        if (!success) {
            debug_if(_debug_trace_on, "%s: ERROR\n", getFtpCmd(ftpCmd));
        }
    }

    UNLOCK();
    return success ? NULL : &_ftpError;
}

/**********************************************************************
 * PUBLIC METHODS: Cell Locate
 **********************************************************************/

// Configure CellLocate TCP Aiding server.
bool UbloxATCellularInterfaceExt::cellLocSrvTcp(const char* token,
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
bool UbloxATCellularInterfaceExt::cellLocSrvUdp(const char* server_1,
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
bool UbloxATCellularInterfaceExt::cellLocConfig(int scanMode)
{
    bool success;
    LOCK();

    success = _at->send("AT+ULOCCELL=%d", scanMode) &&
              _at->recv("OK");

    UNLOCK();
    return success;
}

// Request CellLocate.
bool UbloxATCellularInterfaceExt::cellLocRequest(CellSensType sensor,
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
bool UbloxATCellularInterfaceExt::cellLocGetData(CellLocData *data, int index)
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
int UbloxATCellularInterfaceExt::cellLocGetRes()
{
    int at_timeout;
    LOCK();

    at_timeout = _at_timeout; // Has to be inside LOCK()s
    at_set_timeout(1000);
    // Wait for URCs
    _at->recv(UNNATURAL_STRING);
    at_set_timeout(at_timeout);

    UNLOCK();
    return _locRcvPos;
}

// Get number of positions records expected to be received.
int UbloxATCellularInterfaceExt::cellLocGetExpRes()
{
    int numRecords = 0;
    LOCK();

    _at->recv("OK");

    if (_locRcvPos > 0) {
        numRecords = _locExpPos;
    }

    UNLOCK();
    return numRecords;
}

// End of file
