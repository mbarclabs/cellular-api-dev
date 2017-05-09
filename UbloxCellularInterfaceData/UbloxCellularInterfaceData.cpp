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

#include "UbloxCellularInterfaceData.h"
#include "UbloxCellularInterfaceExt.h"
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
 * MACROS
 **********************************************************************/

/**
 * The profile to use.
 */
#define PROFILE "0"

/**
 * Test if an HTTP profile is OK to use.
 */
#define IS_PROFILE(p) (((p) >= 0) && (((unsigned int) p) < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0]))) \
                       && (_httpProfiles[p].handle != HTTP_PROF_ERROR))

/**
 * Check for timeout.
 */
#define TIMEOUT(t, ms)  ((ms != TIMEOUT_BLOCKING) && (ms < t.read_ms()))

/**********************************************************************
 * PROTECTED METHODS: GENERIC
 **********************************************************************/

// Get the IP address of the on-board modem IP stack.
// Note: the AT interface should be locked before this is called.
UbloxCellularInterfaceData::IP UbloxCellularInterfaceData::getIpAddress()
{
    int a, b, c, d;
    IP ip = NOIP;

    _at->setTimeout(10000);
    // +UPSND=<profile_id>,<param_tag>[,<dynamic_param_val>]
    if (_at->send("AT+UPSND=" PROFILE ",0") &&
        _at->recv("+UPSND: " PROFILE ",0,\"" IPSTR "\"", &a, &b, &c, &d)) {
        ip = IPADR(a, b, c, d);
    }
    _at->setTimeout(1000);

    return ip;
}

// Active a connection profile on board the modem.
// Note: the AT interface should be locked before this is called.
bool UbloxCellularInterfaceData::activateProfile(const char* apn,
                                                const char* username,
                                                const char* password,
                                                Auth auth)
{
    bool activated = false;
    bool success = false;

    // Set up the APN
    if (*apn) {
        success = _at->send("AT+UPSD=" PROFILE ",1,\"%s\"", apn) && _at->recv("OK");
    }
    if (success && *username) {
        success = _at->send("AT+UPSD=" PROFILE ",2,\"%s\"", username) && _at->recv("OK");
    }
    if (success && *password) {
        success = _at->send("AT+UPSD=" PROFILE ",3,\"%s\"", password) && _at->recv("OK");
    }

    if (success) {
        // Set up the dynamic IP address assignment.
        success = _at->send("AT+UPSD=" PROFILE ",7,\"0.0.0.0\"") && _at->recv("OK");
        // Set up the authentication protocol
        // 0 = none
        // 1 = PAP (Password Authentication Protocol)
        // 2 = CHAP (Challenge Handshake Authentication Protocol)
        for (int protocol = AUTH_NONE; success && (protocol <= AUTH_CHAP); protocol++) {
            if ((auth == AUTH_DETECT) || (auth == protocol)) {
                if (_at->send("AT+UPSD=" PROFILE ",6,%d", protocol) && _at->recv("OK")) {
                    // Activate, waiting 180 seconds for the connection to be made
                    if (_at->send("AT+UPSDA=" PROFILE ",3") && _at->recv("OK")) {
                        for (int retries = 0; !activated && (retries < 18); retries++) {
                            if (getIpAddress() != NOIP) {
                                activated = true;
                            }
                        }
                    }
                }
            }
        }
    }

    return activated;
}

// Activate a profile by reusing an external PDP context.
// Note: the AT interface should be locked before this is called.
bool UbloxCellularInterfaceData::activateProfileReuseExternal(void)
{
    bool success = false;
    int cid = -1;
    int a, b, c, d, t;

    //+CGDCONT: <cid>,"IP","<apn name>","<ip adr>",0,0,0,0,0,0
    if (_at->send("AT+CGDCONT?") &&
        _at->recv("+CGDCONT: %d,\"IP\",\"%*[^\"]\",\"" IPSTR "\",%*d,%*d,%*d,%*d,%*d,%*d",
                  &t, &a, &b, &c, &d)) {
        if (IPADR(a, b, c, d) != NOIP) {
            cid = t;
        }
    }

    // If a context has been found, use it
    if ((cid != -1) && (_at->send("AT+UPSD=" PROFILE ",100,%d", cid) && _at->recv("OK"))) {
        // Activate, waiting 30 seconds for the connection to be made
        _at->setTimeout(30000);
        success = _at->send("AT+UPSDA=" PROFILE ",3") && _at->recv("OK");
        _at->setTimeout(1000);
    }

    return success;
}

// Activate a profile by context ID.
// Note: the AT interface should be locked before this is called.
bool UbloxCellularInterfaceData::activateProfileByCid(int cid,
                                                     const char* apn,
                                                     const char* username,
                                                     const char* password,
                                                     Auth auth)
{
    bool success = false;

    if (_at->send("AT+CGDCONT=%d,\"IP\",\"%s\"", cid, apn) && _at->recv("OK") &&
        _at->send("AT+UAUTHREQ=%d,%d,\"%s\",\"%s\"", cid, auth, username, password) && _at->recv("OK") &&
        _at->send("AT+UPSD=" PROFILE ",100,%d", cid) && _at->recv("OK")) {

        // Wait 30 seconds for the connection to be made
        _at->setTimeout(30000);
        // Activate the protocol
        success = _at->send("AT+UPSDA=" PROFILE ",3") && _at->recv("OK");
        _at->setTimeout(1000);
    }

    return success;
}

// Connect the on board IP stack of the modem.
bool UbloxCellularInterfaceData::connectModemStack()
{
    bool success = false;
    int active = 0;
    const char * config = NULL;
    LOCK();

    // Check the profile
    if (_at->send("AT+UPSND=" PROFILE ",8") && _at->recv("+UPSND: %*d,%*d,%d", &active)) {
        if (active == 1) {
            // Disconnect the profile already if it is connected
            if (_at->send("AT+UPSDA=" PROFILE ",4") && _at->recv("OK")) {
                active = 0;
            }
        }
    }

    // Use the profile
    if (_at->send("AT+UPSND=" PROFILE ",8") && _at->recv("+UPSND: %*d,%*d,%d", &active)) {
        if (active == 0) {
            // If the caller hasn't entered an APN, try to find it
            if (_apn == NULL) {
                config = apnconfig(_dev_info->imsi);
            }

            // Attempt to connect
            do {
                // Set up APN and IP protocol for PDP context
                get_next_credentials(config);
                _auth = (*_uname && *_pwd) ? _auth : AUTH_NONE;
                if ((_dev_info->dev != DEV_TOBY_L2) && (_dev_info->dev != DEV_MPCI_L2)) {
                    success = activateProfile(_apn, _uname, _pwd, _auth);
                } else {
                    success = activateProfileReuseExternal();
                    if (success) {
                        tr_debug("Reusing external context\r\n");
                    } else {
                        success = activateProfileByCid(1, _apn, _uname, _pwd, _auth);
                    }
                }
            } while (!success && config && *config);
        }
    }

    UNLOCK();
    return success;
}

// Disconnect the on board IP stack of the modem.
bool UbloxCellularInterfaceData::disconnectModemStack()
{
    bool success = false;
    LOCK();

    if (getIpAddress() != NOIP) {
        if (_at->send("AT+UPSDA=" PROFILE ",4") && _at->recv("OK")) {
            success = true;
        }
    }

    UNLOCK();
    return success;
}


/**********************************************************************
 * PROTECTED METHODS: SOCKETS
 **********************************************************************/

// Find or create a socket from the list.
int UbloxCellularInterfaceData::findSocket(int handle)
{
    int socket = SOCKET_ERROR;

    for (unsigned int x = 0; (socket == SOCKET_ERROR) && (x < sizeof(_sockets) / sizeof(_sockets[0])); x++) {
        if (_sockets[x].handle == handle) {
            socket = x;
        }
    }

    return socket;
}

// Callback for Socket Read URC.
void UbloxCellularInterfaceData::UUSORD_URC()
{
    int a, b;

    // +UUSORD: <socket>,<length>
    if (_at->recv(": %d,%d", &a, &b)) {
        int socket = findSocket(a);
        tr_debug("Socket %d: handle %d has %d bytes pending", socket, a, b);
        if (socket != SOCKET_ERROR) {
            _sockets[socket].pending = b;
        }
    }
}

// Callback for Socket Read From URC.
void UbloxCellularInterfaceData::UUSORF_URC()
{
    int a, b;

    // +UUSORF: <socket>,<length>
    if (_at->recv(": %d,%d", &a, &b)) {
        int socket = findSocket(a);
        tr_debug("Socket %d: handle %d has %d bytes pending", socket, a, b);
        if (socket != SOCKET_ERROR) {
            _sockets[socket].pending = b;
        }
    }
}

// Callback for Socket Close URC.
void UbloxCellularInterfaceData::UUSOCL_URC()
{
    int a;

    // +UUSOCL: <socket>
    if (_at->recv(": %d", &a)) {
        int socket = findSocket(a);
        tr_debug("Socket %d: handle %d closed by remote host", socket, a);
        if (socket != SOCKET_ERROR) {
            _sockets[socket].connected = false;
        }
    }
}

/**********************************************************************
 * PROTECTED METHODS: HTTP
 **********************************************************************/

// Callback for HTTP result code handling.
void UbloxCellularInterfaceData::UUHTTPCR_URC()
{
    int a, b, c;

    // +UHTTPCR: <profile_id>,<op_code>,<param_val>
    if (_at->recv(": %d,%d,%d", &a, &b, &c)) {
        _httpProfiles[a].cmd = b;          // Command
        _httpProfiles[a].result = c;       // Result
    }

    tr_debug("%s for profile %d: result code is %d", getHTTPcmd(b), a, c);
}

// Find a given profile.  NOTE: LOCK() before calling.
int UbloxCellularInterfaceData::findProfile(int handle)
{
    for (unsigned int profile = 0; profile < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0])); profile++) {
        if (_httpProfiles[profile].handle == handle) {
            return profile;
        }
    }

    return HTTP_PROF_ERROR;
}

/**********************************************************************
 * PUBLIC METHODS: GENERIC
 **********************************************************************/

// Constructor.
UbloxCellularInterfaceData::UbloxCellularInterfaceData(bool debugOn, PinName tx, PinName rx, int baud):
                           UbloxCellularInterfaceExt(debugOn, tx, rx, baud)
{
    // The authentication to use
    _auth = AUTH_DETECT;

    // Zero Cell Locate stuff
    _locRcvPos = 0;
    _locExpPos = 0;

    // Zero HTTP profiles
    memset(_httpProfiles, 0, sizeof(_httpProfiles));
    for (unsigned int profile = 0; profile < sizeof(_httpProfiles) / sizeof(_httpProfiles[0]); profile++) {
        _httpProfiles[profile].handle = HTTP_PROF_ERROR;
    }

    // URC handlers for sockets
    _at->oob("+UUSORD", callback(this, &UbloxCellularInterfaceData::UUSORD_URC));
    _at->oob("+UUSORF", callback(this, &UbloxCellularInterfaceData::UUSORF_URC));
    _at->oob("+UUSOCL", callback(this, &UbloxCellularInterfaceData::UUSOCL_URC));

    // URC handler for HTTP
    _at->oob("+UUHTTPCR", callback(this, &UbloxCellularInterfaceData::UUHTTPCR_URC));
}

// Destructor.
UbloxCellularInterfaceData::~UbloxCellularInterfaceData()
{
    // TODO
}

// Make a cellular connection using the IP stack on board the cellular modem
nsapi_error_t UbloxCellularInterfaceData::join(const char *apn, const char *uname,
                                              const char *pwd, Auth auth)
{
    nsapi_error_t nsapi_error = NSAPI_ERROR_ALREADY;

    if (apn != NULL) {
        _apn = apn;
    }

    if ((uname != NULL) && (pwd != NULL)) {
        _uname = uname;
        _pwd = pwd;
        _auth = auth;
    } else {
        _uname = NULL;
        _pwd = NULL;
        _auth = AUTH_DETECT;
    }

    // Can only continue if the PPP connection is not already occupying the modem
    if (!_dev_info->ppp_connection_up) {

        // Set up modem and then register with the network
        nsapi_error = init();
        if (nsapi_error == NSAPI_ERROR_OK) {
            nsapi_error = NSAPI_ERROR_NO_CONNECTION;
            for (int retries = 0; (nsapi_error == NSAPI_ERROR_NO_CONNECTION) && (retries < 3); retries++) {
                if (nwk_registration(_dev_info->dev)) {
                    nsapi_error = NSAPI_ERROR_OK;
                }
            }
        }

        // Attempt to establish a connection
        if (nsapi_error == NSAPI_ERROR_OK) {
            if (connectModemStack()) {
            } else {
                nsapi_error = NSAPI_ERROR_NO_CONNECTION;
            }
        }

        // If we were unable to connect, power down the modem
        if (nsapi_error != NSAPI_ERROR_OK) {
            power_down_modem();
        }
    }

    return nsapi_error;
}

// User initiated disconnect.
nsapi_error_t UbloxCellularInterfaceData::disconnect()
{
    nsapi_error_t nsapi_error = NSAPI_ERROR_DEVICE_ERROR;

    if (_dev_info->ppp_connection_up) {
        nsapi_error = UbloxCellularInterface::disconnect();
    }

    if (disconnectModemStack()) {
        nsapi_error = NSAPI_ERROR_OK;
    }

    return nsapi_error;
}

// Get the IP address of a host.
UbloxCellularInterfaceData::IP UbloxCellularInterfaceData::gethostbyname(const char *host)
{
    IP ip = NOIP;
    int a, b, c, d;

    if (sscanf(host, IPSTR, &a, &b, &c, &d) == 4) {
        ip = IPADR(a, b, c, d);
    } else {
        LOCK();
        if (_at->send("AT+UDNSRN=0,\"%s\"", host) && _at->recv("+UDNSRN: \"" IPSTR "\"", &a, &b, &c, &d)) {
            ip = IPADR(a, b, c, d);
        }
        UNLOCK();
    }

    return ip;
}

/**********************************************************************
 * PUBLIC METHODS: SOCKETS
 **********************************************************************/

// Create a socket.
int UbloxCellularInterfaceData::socketSocket(IpProtocol ipproto, int port)
{
    bool success = false;
    int socket;
    LOCK();

    // Find a free socket
    socket = findSocket();
    tr_debug("socketSocket(%d)", ipproto);

    if (socket != SOCKET_ERROR) {
        if (ipproto == IPPROTO_UDP) {
            // Sending port can only be set on 2G/3G modules
            if ((port != -1) && (_dev_info->dev != DEV_LISA_C2)) {
                success = _at->send("AT+USOCR=17,%d", port);
            } else {
                success = _at->send("AT+USOCR=17");
            }
        } else  { // IPPROTO_TCP
            success = _at->send("AT+USOCR=6\r\n");
        }

        int handle = SOCKET_ERROR;

        if (success && _at->recv("+USOCR: %d", &handle) && (handle != SOCKET_ERROR)) {
            tr_debug("Socket %d: handle %d was created", socket, handle);
            _sockets[socket].handle     = handle;
            _sockets[socket].timeoutMilliseconds = TIMEOUT_BLOCKING;
            _sockets[socket].connected  = false;
            _sockets[socket].pending    = 0;
        } else {
            socket = SOCKET_ERROR;
        }
    }

    UNLOCK();
    return socket;
}

// Connect a socket.
bool UbloxCellularInterfaceData::socketConnect(int socket, const char * host, int port)
{
    bool success = false;
    IP ip = gethostbyname(host);

    if (ip != NOIP) {
        LOCK();

        if (IS_SOCKET(socket) && (!_sockets[socket].connected)) {
            tr_debug("socketConnect(%d, %s, %d)", socket, host, port);
            if (_at->send("AT+USOCO=%d,\"" IPSTR "\",%d", _sockets[socket].handle, IPNUM(ip), port) &&
                _at->recv("OK")) {
                _sockets[socket].connected == true;
                success = true;
            }
        }

        UNLOCK();
    }

    return success;
}

// Determine whether a socket is connected.
bool UbloxCellularInterfaceData::socketIsConnected(int socket)
{
    bool success = false;
    LOCK();

    success = IS_SOCKET(socket) && _sockets[socket].connected;
    tr_debug("socketIsConnected(%d) %s", socket, success ? "yes":"no");

    UNLOCK();
    return success;
}

// Set a socket timeout.
bool UbloxCellularInterfaceData::socketSetBlocking(int socket, int timeoutMilliseconds)
{
    bool success = false;
    LOCK();

    tr_debug("socketSetBlocking(%d, %d)", socket, timeoutMilliseconds);
    if (IS_SOCKET(socket)) {
        _sockets[socket].timeoutMilliseconds = timeoutMilliseconds;
        success = true;
    }

    UNLOCK();
    return success;
}

// Close a socket.
bool UbloxCellularInterfaceData::socketClose(int socket)
{
    bool success = false;
    LOCK();

    if (IS_SOCKET(socket) && _sockets[socket].connected) {
        tr_debug("socketClose(%d)", socket);
        if (_at->send("AT+USOCL=%d\r\n", _sockets[socket].handle) &&
            _at->recv("OK")) {
            _sockets[socket].connected = false;
            success = true;
        }
    }

    UNLOCK();
    return success;
}

// Free a socket.
bool UbloxCellularInterfaceData::socketFree(int socket)
{
    bool success = false;

    // Make sure it is closed first
    if (IS_SOCKET(socket)) {
        if (socketClose(socket)) {
            tr_debug("socketFree(%d)", socket);
            _sockets[socket].handle     = SOCKET_ERROR;
            _sockets[socket].timeoutMilliseconds = TIMEOUT_BLOCKING;
            _sockets[socket].connected  = false;
            _sockets[socket].pending    = 0;
            success = true;
        }
    }
    return success;
}

// Send to a socket.
int UbloxCellularInterfaceData::socketSend(int socket, const char * buf, int len)
{
    int returnValue = SOCKET_ERROR;
    bool success = true;
    int blk = MAX_WRITE_SIZE;
    int cnt = len;

    tr_debug("socketSend(%d,,%d)", socket, len);

    while ((cnt > 0) && success) {
        if (cnt < blk) {
            blk = cnt;
        }
        LOCK();

        if (IS_SOCKET(socket)) {
            if (_at->send("AT+USOWR=%d,%d", _sockets[socket].handle, blk) && _at->recv("@")) {
                wait_ms(50);
                if ((_at->write(buf, blk) >= blk) &&
                    _at->recv("OK")) {
                    success = true;
                }
            }
        }

        UNLOCK();
        buf += blk;
        cnt -= blk;
    }

    if (success) {
        returnValue = len - cnt;
    }

    return returnValue;
}

// Send to an IP address.
int UbloxCellularInterfaceData::socketSendTo(int socket, IP ip, int port, const char * buf, int len)
{
    int returnValue = SOCKET_ERROR;
    bool success = true;
    int blk = MAX_WRITE_SIZE;
    int cnt = len;

    tr_debug("socketSendTo(%d," IPSTR ",%d,,%d)", socket, IPNUM(ip), port, len);

    while ((cnt > 0) && success) {
        if (cnt < blk) {
            blk = cnt;
        }
        LOCK();

        if (IS_SOCKET(socket)) {
            if (_at->send("AT+USOST=%d,\"" IPSTR "\",%d,%d",_sockets[socket].handle, IPNUM(ip), port, blk) &&
                _at->recv("@")) {
                wait_ms(50);
                if ((_at->write(buf, blk) >= blk) &&
                    _at->recv("OK")) {
                    success = true;
                }
            }
        }
        UNLOCK();
        buf += blk;
        cnt -= blk;
    }

    if (success) {
        returnValue = len - cnt;
    }

    return returnValue;
}

// Determine if there's anything to read.
int UbloxCellularInterfaceData::socketReadable(int socket)
{
    int pending = SOCKET_ERROR;
    LOCK();

    if (IS_SOCKET(socket) && _sockets[socket].connected) {
        tr_debug("socketReadable(%d)", socket);
        // Allow time to receive unsolicited commands
        wait_ms(8000);
        if (_sockets[socket].connected) {
           pending = _sockets[socket].pending;
        }
    }

    UNLOCK();
    return pending;
}

// Receive from a socket.
int UbloxCellularInterfaceData::socketRecv(int socket, char* buf, int len)
{
    int returnValue = SOCKET_ERROR;
    bool success = true;
    int blk = MAX_READ_SIZE;
    char * tmpBuf = NULL;
    int cnt = 0;
    unsigned int sz, sk, readSize;
    Timer timer;

    tr_debug("socketRecv(%d,,%d)", socket, len);

    timer.start();
    while (success && (len > 0)) {
        if (len < blk) {
            blk = len;
        }
        LOCK();

        tmpBuf = (char *) malloc(MAX_READ_SIZE);

        if ((tmpBuf != NULL) && IS_SOCKET(socket)) {
            if (_sockets[socket].connected) {
                if (_sockets[socket].pending < blk) {
                    blk = _sockets[socket].pending;
                }
                if (blk > 0) {
                    if (_at->send("AT+USORD=%d,%d",_sockets[socket].handle, blk) &&
                        _at->recv("+USORD: %d,%d,", &sk, &sz)) {
                        MBED_ASSERT (sz <= sizeof (tmpBuf) + 2);
                        readSize = _at->read(tmpBuf, sz + 2); // +2 for leading and trailing quotes
                        if ((readSize > 0) &&
                            (*(tmpBuf + readSize - sz - 2) == '\"' && *(tmpBuf  + readSize - 1) == '\"')) {
                            memcpy(buf, (tmpBuf + readSize - sz - 1), sz);
                            _sockets[socket].pending -= blk;
                            len -= blk;
                            cnt += blk;
                            buf += blk;
                        }
                    }
                } else if (!TIMEOUT(timer, _sockets[socket].timeoutMilliseconds)) {
                    wait_ms(8000);  // wait for URCs
                } else {
                    len = 0;
                }
            } else {
                len = 0;
            }
        } else {
            success = false;
        }

        free(tmpBuf);

        UNLOCK();
    }
    timer.stop();

    if (success) {
        returnValue = cnt;
    }

    tr_debug("socketRecv: %d \"%*s\"", cnt, cnt, buf - cnt);

    return returnValue;
}

int UbloxCellularInterfaceData::socketRecvFrom(int socket, IP* ip, int* port, char* buf, int len)
{
    int returnValue = SOCKET_ERROR;
    bool success = true;
    int blk = MAX_READ_SIZE;
    char * tmpBuf = NULL;
    int cnt = 0;
    unsigned int sz, sk, a, b, c, d, readSize;
    Timer timer;

    tr_debug("socketRecvFrom(%d,,%d)", socket, len);

    timer.start();
    while (success && (len > 0)) {
        if (len < blk) {
            blk = len;
        }
        LOCK();

        tmpBuf = (char *) malloc(MAX_READ_SIZE);

        if ((tmpBuf != NULL) && IS_SOCKET(socket)) {
            if (_sockets[socket].connected) {
                if (_sockets[socket].pending < blk) {
                    blk = _sockets[socket].pending;
                }
                if (blk > 0) {
                    if (_at->send("AT+USORF=%d,%d",_sockets[socket].handle, blk) &&
                        _at->recv("\r\n+USORF: %d,\"" IPSTR "\",%d,%d,",
                                  &sk, &a, &b, &c, &d, port, &sz)) {
                        MBED_ASSERT (sz <= sizeof (tmpBuf) + 2);
                        readSize = _at->read(tmpBuf, sz + 2); // +2 for leading and trailing quotes
                        if ((readSize > 0) &&
                            (*(tmpBuf + readSize - sz - 2) == '\"') && *(tmpBuf + readSize - 1) == '\"') {
                            memcpy(buf, (tmpBuf + readSize - sz - 1), sz);
                            _sockets[socket].pending -= blk;
                            *ip = IPADR(a, b, c, d);
                            len -= blk;
                            cnt += blk;
                            buf += blk;
                            len = 0; // done
                        }
                    }
                } else if (!TIMEOUT(timer, _sockets[socket].timeoutMilliseconds)) {
                    wait_ms(8000);  // wait for URCs
                } else {
                    len = 0;
                }
            } else {
                len = 0;
            }
        } else {
            success = false;
        }

        free(tmpBuf);

        UNLOCK();
    }
    timer.stop();

    if (success) {
        returnValue = cnt;
    }

    tr_debug("socketRecvFrom: %d \"%*s\"", cnt, cnt, buf - cnt);

    return returnValue;
}

/**********************************************************************
 * PUBLIC METHODS: HTTP
 **********************************************************************/

// Find a free profile.
int UbloxCellularInterfaceData::httpFindProfile()
{
    int profile = HTTP_PROF_ERROR;
    LOCK();

    // Find a free HTTP profile
    profile = findProfile();
    tr_debug("httpFindProfile: profile is %d", profile);

    if (profile != HTTP_PROF_ERROR) {
        _httpProfiles[profile].handle     = 1;
        _httpProfiles[profile].timeout_ms = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending    = false;
        _httpProfiles[profile].cmd        = -1;
        _httpProfiles[profile].result     = -1;
    }

    UNLOCK();
    return profile;
}

// Set the blocking/timeout state of a profile.
bool UbloxCellularInterfaceData::httpSetBlocking(int profile, int timeout_ms)
{
    bool ok = false;
    LOCK();

    tr_debug("httpSetBlocking(%d, %d)", profile, timeout_ms);
    if (IS_PROFILE(profile)) {
        _httpProfiles[profile].timeout_ms = timeout_ms;
        ok = true;
    }

    UNLOCK();
    return ok;
}

// Set the profile so that it's suitable for commands.
bool UbloxCellularInterfaceData::httpSetProfileForCmdMng(int profile)
{
    bool ok = false;
    LOCK();

    tr_debug("httpSetProfileForCmdMng(%d)", profile);
    if (IS_PROFILE(profile)) {
        _httpProfiles[profile].pending = true;
        _httpProfiles[profile].result = -1;
        ok = true;
    }

    UNLOCK();
    return ok;
}

// Free a profile.
bool UbloxCellularInterfaceData::httpFreeProfile(int profile)
{
    LOCK();

    if (IS_PROFILE(profile)) {
        tr_debug("httpFreeProfile(%d)", profile);
        _httpProfiles[profile].handle     = HTTP_PROF_ERROR;
        _httpProfiles[profile].timeout_ms = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending    = false;
        _httpProfiles[profile].cmd        = -1;
        _httpProfiles[profile].result     = -1;
    }

    UNLOCK();
    return true;
}

// Set a profile back to defaults.
bool UbloxCellularInterfaceData::httpResetProfile(int httpProfile)
{
    bool ok = false;
    LOCK();

    tr_debug("httpResetProfile(%d)", httpProfile);
    ok = _at->send("AT+UHTTP=%d", httpProfile) && _at->recv("OK");

    UNLOCK();
    return ok;
}

// Set HTTP parameters.
bool UbloxCellularInterfaceData::httpSetPar(int httpProfile, HttpOpCode httpOpCode, const char * httpInPar)
{
    bool ok = false;
    IP ip = NOIP;
    int httpInParNum = 0;
    LOCK();

    tr_debug("httpSetPar(%d,%d,\"%s\")", httpProfile, httpOpCode, httpInPar);
    switch(httpOpCode) {
        case HTTP_IP_ADDRESS:   // 0
            ip = gethostbyname(httpInPar);
            if (ip != NOIP) {
                ok = _at->send("AT+UHTTP=%d,%d,\"" IPSTR "\"", httpProfile, httpOpCode, IPNUM(ip)) && _at->recv("OK");
            }
            break;
        case HTTP_SERVER_NAME:  // 1
        case HTTP_USER_NAME:    // 2
        case HTTP_PASSWORD:     // 3
            ok = _at->send("AT+UHTTP=%d,%d,\"%s\"", httpProfile, httpOpCode, httpInPar) && _at->recv("OK");
            break;

        case HTTP_AUTH_TYPE:    // 4
        case HTTP_SERVER_PORT:  // 5
            httpInParNum = atoi(httpInPar);
            ok = _at->send("AT+UHTTP=%d,%d,%d", httpProfile, httpOpCode, httpInParNum) && _at->recv("OK");
            break;

        case HTTP_SECURE:       // 6
            if (_dev_info->dev != DEV_LISA_C2) {
                httpInParNum = atoi(httpInPar);
                ok = _at->send("AT+UHTTP=%d,%d,%d", httpProfile, httpOpCode, httpInParNum) && _at->recv("OK");
            } else {
                tr_debug("httpSetPar: HTTP secure option not supported by module");
            }
            break;

        default:
            tr_debug("httpSetPar: unknown httpOpCode %d", httpOpCode);
            break;
    }

    UNLOCK();
    return ok;
}

// Perform an HTTP command.
bool UbloxCellularInterfaceData::httpCommand(int httpProfile, HttpCmd httpCmdCode, const char *httpPath, const char *httpOut,
                                            const char *httpIn, int httpContentType, const char *httpCustomPar, char *buf, int len)
{
    bool ok = false;
    LOCK();

    tr_debug("%s", getHTTPcmd(httpCmdCode));
    switch (httpCmdCode) {
        case HTTP_HEAD:
            ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_HEAD, httpPath, httpOut) &&
                 _at->recv("OK");
            break;
        case HTTP_GET:
            ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_GET, httpPath, httpOut) &&
                 _at->recv("OK");
            break;
        case HTTP_DELETE:
            ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_DELETE, httpPath, httpOut) &&
                 _at->recv("OK");
            break;
        case HTTP_PUT:
            // In this case the parameter httpIn is a filename
            ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\"", httpProfile, HTTP_PUT, httpPath, httpOut, httpIn) &&
                 _at->recv("OK");
            break;
        case HTTP_POST_FILE:
            // In this case the parameter httpIn is a filename
            if (_dev_info->dev != DEV_LISA_C2) {
                if (httpContentType != 6) {
                    ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                                   httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType) &&
                         _at->recv("OK");
                } else {
                    ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                                   httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType, httpCustomPar) &&
                         _at->recv("OK");
                }
            } else {
                if ((httpContentType != 5) && (httpContentType != 6) && (httpCustomPar == NULL)) {
                    // Parameter values consistent with the AT commands specs of LISA-C200
                    // (in particular httpCustomPar has to be not defined)
                    ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                                   httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType) &&
                         _at->recv("OK");
                } else {
                    tr_debug("httpCommand: command not supported by module");
                }
            }
            break;
        case HTTP_POST_DATA:
            // In this case the parameter httpIn is a string containing data
            if (_dev_info->dev != DEV_LISA_C2) {
                if (httpContentType != 6) {
                    ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                                   httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType) &&
                         _at->recv("OK");
           } else {
                    ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                                   httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType, httpCustomPar) &&
                         _at->recv("OK");
                }
            } else {
                if ((httpContentType != 5) && (httpContentType != 6) && (httpCustomPar == NULL)) {
                    // Parameters values consistent with the AT commands specs of LISA-C200
                    // (in particular httpCustomPar has to be not defined)
                    ok = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                                   httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType) &&
                         _at->recv("OK");
                } else {
                    tr_debug("httpCommand: command not supported by module");
                }
            }
            break;
        default:
            tr_debug("HTTP command not recognised");
    }

    if (ok) {
        Timer timer;

        timer.start();
        httpSetProfileForCmdMng(httpProfile);

        // Waiting for unsolicited result codes
        while (_httpProfiles[httpProfile].pending) {
            ok = false;  // Reset variable
            if (_httpProfiles[httpProfile].result != -1) {
                // Received unsolicited: starting its analysis
                _httpProfiles[httpProfile].pending = false;
                if (_httpProfiles[httpProfile].result == 1) {
                    // HTTP command successfully executed
                    if (_dev_info->dev != DEV_LISA_C2) {
                        tr_debug("httpCommand: reading files with a dimension also greater than MAX_SIZE bytes");
                        if (readFileNew(httpOut, buf, len) >= 0) {
                            ok = true;
                        }
                    } else {
                        tr_debug("httpCommand: reading files with a dimension less than MAX_SIZE bytes, otherwise error");
                        if (readFile(httpOut, buf, len) >= 0) {
                            ok = true;
                        }
                    }
                } else {
                    // HTTP command not successfully executed
                    ok = false;
                }
            } else if (!TIMEOUT(timer, _httpProfiles[httpProfile].timeout_ms)) {
                // Wait for URCs
                wait_ms (1000);
            } else  {
                // Not received unsolicited and expired timer
                tr_debug("httpCommand: URC not received in time");
                ok = false;
            }

            if (!ok) {
                tr_debug("%s: ERROR", getHTTPcmd(httpCmdCode));
                // No more while loops
                _httpProfiles[httpProfile].pending = false;
            }
        }
    }

    UNLOCK();
    return ok;
}

// Return a string representing an AT command.
const char * UbloxCellularInterfaceData::getHTTPcmd(int httpCmdCode)
{
    switch (httpCmdCode) {
        case HTTP_HEAD:
            return "HTTP HEAD command";
        case HTTP_GET:
            return "HTTP GET command";
        case HTTP_DELETE:
            return "HTTP DELETE command";
        case HTTP_PUT:
            return "HTTP PUT command";
        case HTTP_POST_FILE:
            return "HTTP POST file command";
        case HTTP_POST_DATA:
            return "HTTP POST data command";
        default:
            return "HTTP command not recognised";
    }
}

// End of file
