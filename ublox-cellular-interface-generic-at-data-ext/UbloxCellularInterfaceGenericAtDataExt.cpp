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

#include "UbloxCellularInterfaceGenericAtDataExt.h"
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
void UbloxCellularInterfaceGenericAtDataExt::UUHTTPCR_URC()
{
    char buf[32];
    int a, b, c;

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
int UbloxCellularInterfaceGenericAtDataExt::findProfile(int modemHandle)
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

/**********************************************************************
 * PUBLIC METHODS: GENERIC
 **********************************************************************/

// Constructor.
UbloxCellularInterfaceGenericAtDataExt::UbloxCellularInterfaceGenericAtDataExt(bool debugOn,
                                                                               PinName tx,
                                                                               PinName rx,
                                                                               int baud):
                                        UbloxCellularInterfaceGenericAtData(debugOn, tx, rx, baud)
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
    _at->oob("+UUHTTPCR", callback(this, &UbloxCellularInterfaceGenericAtDataExt::UUHTTPCR_URC));
}

// Destructor.
UbloxCellularInterfaceGenericAtDataExt::~UbloxCellularInterfaceGenericAtDataExt()
{
}

/**********************************************************************
 * PUBLIC METHODS: HTTP
 **********************************************************************/

// Find a free profile.
int UbloxCellularInterfaceGenericAtDataExt::httpFindProfile()
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
bool UbloxCellularInterfaceGenericAtDataExt::httpSetBlocking(int profile, int timeout_ms)
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
bool UbloxCellularInterfaceGenericAtDataExt::httpSetProfileForCmdMng(int profile)
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
bool UbloxCellularInterfaceGenericAtDataExt::httpFreeProfile(int profile)
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
bool UbloxCellularInterfaceGenericAtDataExt::httpResetProfile(int httpProfile)
{
    bool success = false;
    LOCK();

    tr_debug("httpResetProfile(%d)", httpProfile);
    success = _at->send("AT+UHTTP=%d", httpProfile) && _at->recv("OK");

    UNLOCK();
    return success;
}

// Set HTTP parameters.
bool UbloxCellularInterfaceGenericAtDataExt::httpSetPar(int httpProfile,
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
bool UbloxCellularInterfaceGenericAtDataExt::httpCommand(int httpProfile,
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
const char * UbloxCellularInterfaceGenericAtDataExt::getHttpCmd(int httpCmdCode)
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

// TODO

// End of file
