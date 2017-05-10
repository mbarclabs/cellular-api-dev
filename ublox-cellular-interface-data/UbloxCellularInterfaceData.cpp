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
 * Check for timeout.
 */
#define TIMEOUT(t, ms)  ((ms != TIMEOUT_BLOCKING) && (ms < t.read_ms()))

/**********************************************************************
 * PROTECTED METHODS: GENERIC
 **********************************************************************/

// Active a connection profile on board the modem.
// Note: the AT interface should be locked before this is called.
bool UbloxCellularInterfaceData::activateProfile(const char* apn,
                                                const char* username,
                                                const char* password,
                                                Auth auth)
{
    bool activated = false;
    bool success = false;
    SocketAddress address;

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
                            if (get_ip_address() != NULL) {
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
    char ip[NSAPI_IP_SIZE];
    SocketAddress address;
    int t;

    //+CGDCONT: <cid>,"IP","<apn name>","<ip adr>",0,0,0,0,0,0
    if (_at->send("AT+CGDCONT?")) {
        if (_at->recv("+CGDCONT: %d,\"IP\",\"%*[^\"]\",\"%" stringify(NSAPI_IP_SIZE) "[^\"]\",%*d,%*d,%*d,%*d,%*d,%*d",
                      &t, ip)) {
            // Check if the IP address is valid
            if (address.set_ip_address(ip)) {
                cid = t;
            }
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

    if (get_ip_address() != NULL) {
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

// Gain access to us.
NetworkStack *UbloxCellularInterfaceData::get_stack()
{
    return this;
}

// Get the IP address of the on-board modem IP stack.
const char * UbloxCellularInterfaceData::get_ip_address()
{
    LOCK();

    if (_ip == NULL) {
        // Temporary storage for an IP address string with terminator
        _ip = (char *) malloc(NSAPI_IP_SIZE);
        if (_ip != NULL) {
            memset(_ip, 0, NSAPI_IP_SIZE); // Ensure a terminator
            // +UPSND=<profile_id>,<param_tag>[,<dynamic_param_val>]
            if (!_at->send("AT+UPSND=" PROFILE ",0") ||
                !_at->recv("+UPSND: " PROFILE ",0,\"%" stringify(NSAPI_IP_SIZE) "[^\"]\"", _ip)) {
                free (_ip);
                _ip = NULL;
            }
        }
    }

    UNLOCK();
    return _ip;
}

// Get the IP address of a host.
nsapi_error_t UbloxCellularInterfaceData::gethostbyname(const char *host,
                                                        SocketAddress *address,
                                                        nsapi_version_t version)
{
    nsapi_error_t nsapiError = NSAPI_ERROR_DEVICE_ERROR;
    char ipAddress[NSAPI_IP_SIZE];

    if (address->set_ip_address(host)) {
        nsapiError = NSAPI_ERROR_OK;
    } else {
        LOCK();
        memset (ipAddress, 0, sizeof (ipAddress)); // Ensure terminator
        if (_at->send("AT+UDNSRN=0,\"%s\"", host) && _at->recv("+UDNSRN: \"%" stringify(NSAPI_IP_SIZE) "[^\"]\"", ipAddress)) {
            if (address->set_ip_address(ipAddress)) {
                nsapiError = NSAPI_ERROR_OK;
            }
        }
        UNLOCK();
    }

    return nsapiError;
}

// Create a socket.
nsapi_error_t UbloxCellularInterfaceData::socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto)
{
    nsapi_error_t nsapiError = NSAPI_ERROR_DEVICE_ERROR;
    bool success = false;
    int modemHandle;
    SockCtrl *socket;
    LOCK();

    // Find a free socket
    socket = findSocket();
    tr_debug("socket_open(%d)", proto);

    if (socket != NULL) {
        if (proto == NSAPI_UDP) {
            success = _at->send("AT+USOCR=17");
        } else if (proto == NSAPI_TCP) {
            success = _at->send("AT+USOCR=6\r\n");
        } else  {
            nsapiError = NSAPI_ERROR_UNSUPPORTED;
        }

        if (success) {
            if (_at->recv("+USOCR: %d", &modemHandle) && (modemHandle != SOCKET_UNUSED)) {
                tr_debug("Socket 0x%8x: handle %d was created", (unsigned int) socket, modemHandle);
                socket->modemHandle         = modemHandle;
                socket->timeoutMilliseconds = TIMEOUT_BLOCKING;
                socket->connected           = false;
                socket->pending             = 0;
                *handle = (nsapi_socket_t) socket;
            } else {
                nsapiError = NSAPI_ERROR_NO_SOCKET;
            }
        }
    } else {
        nsapiError = NSAPI_ERROR_NO_MEMORY;
    }

    UNLOCK();
    return nsapiError;
}

// Close a socket.
nsapi_error_t UbloxCellularInterfaceData::socket_close(nsapi_socket_t handle)
{
    nsapi_error_t nsapiError = NSAPI_ERROR_DEVICE_ERROR;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_close(0x%08x)", (unsigned int) handle);

    if (socket->connected) {
        if (_at->send("AT+USOCL=%d\r\n", socket->modemHandle) &&
            _at->recv("OK")) {
            socket->connected           = false;
            socket->modemHandle         = SOCKET_UNUSED;
            socket->timeoutMilliseconds = TIMEOUT_BLOCKING;
            socket->connected           = false;
            socket->pending             = 0;
            nsapiError = NSAPI_ERROR_OK;
        }
    } else {
        nsapiError = NSAPI_ERROR_OK;
    }

    return nsapiError;
}

// Bind a specific address to a socket.
nsapi_error_t UbloxCellularInterfaceData::socket_bind(nsapi_socket_t handle, const SocketAddress &address)
{
    nsapi_error_t nsapiError = NSAPI_ERROR_DEVICE_ERROR;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_bind(0x%08x, %s(:%d))", (unsigned int) handle, address.get_ip_address(), address.get_port());

    if (!socket->connected) {
        LOCK();
        socket->connected = _at->send("AT+USOCO=%d,\"%s\",%d", socket->modemHandle, address.get_ip_address(),
                                      address.get_port()) &&
                            _at->recv("OK");
        UNLOCK();
    } else {
        nsapiError = NSAPI_ERROR_IS_CONNECTED;
    }

    return nsapiError;
}

// Connect to a socket
nsapi_error_t UbloxCellularInterfaceData::socket_connect(nsapi_socket_t handle, const SocketAddress &address)
{
    nsapi_error_t nsapiError = NSAPI_ERROR_DEVICE_ERROR;
    SockCtrl *socket = (SockCtrl *) handle;
    LOCK();

    tr_debug("socket_connect(0x%08x, %s(:%d))", (unsigned int) handle, address.get_ip_address(), address.get_port());
    if (!socket->connected) {
        if (_at->send("AT+USOCO=%d,\"%s\",%d", socket->modemHandle, address.get_ip_address(), address.get_port()) &&
            _at->recv("OK")) {
            socket->connected = true;
            nsapiError = NSAPI_ERROR_OK;
        }
    }

    UNLOCK();
    return nsapiError;
}

// Send to a socket.
nsapi_size_or_error_t UbloxCellularInterfaceData::socket_send(nsapi_socket_t handle, const void *data, nsapi_size_t size)
{
    nsapi_size_or_error_t nsapiErrorSize = NSAPI_ERROR_DEVICE_ERROR;
    bool success = true;
    const char *buf = (const char *) data;
    nsapi_size_t blk = MAX_WRITE_SIZE;
    nsapi_size_t cnt = size;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_send(0x%08x, , %d)", (unsigned int) handle, size);

    while ((cnt > 0) && success) {
        if (cnt < blk) {
            blk = cnt;
        }
        LOCK();

        if (socket->connected) {
            if (_at->send("AT+USOWR=%d,%d", socket->modemHandle, blk) && _at->recv("@")) {
                wait_ms(50);
                if ((_at->write(buf, blk) >= (int) blk) &&
                     _at->recv("OK")) {
                } else {
                    success = false;
                }
            } else {
                success = false;
            }
        } else {
            nsapiErrorSize = NSAPI_ERROR_NO_CONNECTION;
            success = false;
        }

        UNLOCK();
        buf += blk;
        cnt -= blk;
    }

    if (success) {
        nsapiErrorSize = size - cnt;
    }

    return nsapiErrorSize;
}

// Send to an IP address.
nsapi_size_or_error_t UbloxCellularInterfaceData::socket_sendto(nsapi_socket_t handle, const SocketAddress &address,
                                                                const void *data, nsapi_size_t size)
{
    nsapi_size_or_error_t nsapiErrorSize = NSAPI_ERROR_DEVICE_ERROR;
    bool success = true;
    const char *buf = (const char *) data;
    nsapi_size_t blk = MAX_WRITE_SIZE;
    nsapi_size_t cnt = size;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_sendto(0x%8x, %s(:%d), , %d)", (unsigned int) handle, address.get_ip_address(), address.get_port(), size);

    while ((cnt > 0) && success) {
        if (cnt < blk) {
            blk = cnt;
        }
        LOCK();

        if (!_at->send("AT+USOST=%d,\"%s\",%d,%d", socket->modemHandle, address.get_ip_address(), address.get_port(), blk) ||
            !_at->recv("@")) {
            nsapiErrorSize = NSAPI_ERROR_NO_CONNECTION;
            success = false;
        }

        if (success) {
            wait_ms(50);
            if ((_at->write((const char *) buf, blk) >= (int) blk) &&
                _at->recv("OK")) {
                // Success case
            } else {
                nsapiErrorSize = NSAPI_ERROR_NO_CONNECTION;
                success = false;
            }
        }

        UNLOCK();
        buf += blk;
        cnt -= blk;
    }

    if (success) {
        nsapiErrorSize = size - cnt;
    }

    return nsapiErrorSize;
}

// Receive from a socket.
nsapi_size_or_error_t UbloxCellularInterfaceData::socket_recv(nsapi_socket_t handle,
                                                              void *data, nsapi_size_t size)
{
    nsapi_size_or_error_t nsapiErrorSize = NSAPI_ERROR_DEVICE_ERROR;
    bool success = true;
    char *buf = (char *) data;
    nsapi_size_t blk = MAX_READ_SIZE;
    nsapi_size_t cnt = 0;
    char * tmpBuf = NULL;
    unsigned int sz, sk, readSize;
    Timer timer;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_recv(0x%08x, , %d)", (unsigned int) handle, size);

    timer.start();
    tmpBuf = (char *) malloc(MAX_READ_SIZE);

    if (tmpBuf != NULL) {
        while (success && (size > 0)) {
            if (size < blk) {
                blk = size;
            }
            LOCK();

            if (socket->connected) {
                if (socket->pending < blk) {
                    blk = socket->pending;
                }
                if (blk > 0) {
                    if (_at->send("AT+USORD=%d,%d", socket->modemHandle, blk) &&
                        _at->recv("+USORD: %d,%d,", &sk, &sz)) {
                        MBED_ASSERT (sz <= sizeof (tmpBuf) + 2);
                        readSize = _at->read(tmpBuf, sz + 2); // +2 for leading and trailing quotes
                        if ((readSize > 0) &&
                            (*(tmpBuf + readSize - sz - 2) == '\"' && *(tmpBuf  + readSize - 1) == '\"')) {
                            memcpy(buf, (tmpBuf + readSize - sz - 1), sz);
                            socket->pending -= blk;
                            size -= blk;
                            cnt += blk;
                            buf += blk;
                        }
                    }
                } else if (!TIMEOUT(timer, socket->timeoutMilliseconds)) {
                    wait_ms(8000);  // wait for URCs
                } else {
                    size = 0;
                }
            } else {
                nsapiErrorSize = NSAPI_ERROR_NO_CONNECTION;
                success = false;
            }
        }

        UNLOCK();
        free(tmpBuf);
    } else {
        success = false;
        nsapiErrorSize = NSAPI_ERROR_NO_MEMORY;
    }
    timer.stop();

    if (success) {
        nsapiErrorSize = cnt;
    }
    tr_debug("socket_recv: %d \"%*s\"", cnt, cnt, buf - cnt);

    return nsapiErrorSize;
}

// Receive a packet over a UDP socket.
nsapi_size_or_error_t UbloxCellularInterfaceData::socket_recvfrom(nsapi_socket_t handle, SocketAddress *address,
                                                                  void *data, nsapi_size_t size)
{
    nsapi_size_or_error_t nsapiErrorSize = NSAPI_ERROR_DEVICE_ERROR;
    bool success = true;
    char *buf = (char *) data;
    nsapi_size_t blk = MAX_READ_SIZE;
    nsapi_size_t cnt = 0;
    char ipAddress[NSAPI_IP_SIZE];
    int port;
    char * tmpBuf = NULL;
    unsigned int sz,readSize;
    Timer timer;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_recvfrom(0x%08x, %s(:%d), , %d)", (unsigned int) handle, address->get_ip_address(), address->get_port(), size);

    timer.start();
    tmpBuf = (char *) malloc(MAX_READ_SIZE);

    if (tmpBuf != NULL) {
        while (success && (size > 0)) {
            if (size < blk) {
                blk = size;
            }
            LOCK();

            if (socket->connected) {
                if (socket->pending < blk) {
                    blk = socket->pending;
                }
                if (blk > 0) {
                    memset (ipAddress, 0, sizeof (ipAddress)); // Ensure terminator
                    if (_at->send("AT+USORF=%d,%d", socket->modemHandle, blk) &&
                        _at->recv("+USORF=%d,\"%" stringify(NSAPI_IP_SIZE) "[^\"]\",%d,%d", &(socket->modemHandle),
                                  ipAddress, &port, &sz)) {
                        MBED_ASSERT (sz <= sizeof (tmpBuf) + 2);
                        readSize = _at->read(tmpBuf, sz + 2); // +2 for leading and trailing quotes
                        if ((readSize > 0) &&
                            (*(tmpBuf + readSize - sz - 2) == '\"') && *(tmpBuf + readSize - 1) == '\"') {
                            memcpy(buf, (tmpBuf + readSize - sz - 1), sz);
                            socket->pending -= blk;
                            address->set_ip_address(ipAddress);
                            address->set_port(port);
                            cnt += blk;
                            buf += blk;
                            size = 0; // done
                        }
                    } else {
                        nsapiErrorSize = NSAPI_ERROR_NO_CONNECTION;
                        success = false;
                    }
                } else if (!TIMEOUT(timer, socket->timeoutMilliseconds)) {
                    wait_ms(8000);  // wait for URCs
                } else {
                    size = 0;
                }
            } else {
                nsapiErrorSize = NSAPI_ERROR_NO_CONNECTION;
                success = false;
            }

            UNLOCK();
        }
        free(tmpBuf);

    } else {
        success = false;
        nsapiErrorSize = NSAPI_ERROR_NO_MEMORY;
    }
    timer.stop();

    if (success) {
        nsapiErrorSize = cnt;
    }
    tr_debug("socket_recv: %d \"%*s\"", cnt, cnt, buf - cnt);

    return nsapiErrorSize;
}

// Unsupported (TCP server) functions.
nsapi_error_t UbloxCellularInterfaceData::socket_listen(nsapi_socket_t handle, int backlog)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
nsapi_error_t UbloxCellularInterfaceData::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}
void UbloxCellularInterfaceData::socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data)
{
}

// Determine if there's anything to read.
nsapi_size_or_error_t UbloxCellularInterfaceData::socket_readable(nsapi_socket_t handle)
{
    nsapi_size_or_error_t nsapiSizeError = NSAPI_ERROR_DEVICE_ERROR;
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_readable(0x%08x)", (unsigned int) handle);
    if (socket->connected) {
        // Allow time to receive unsolicited commands
        wait_ms(8000);
        if (socket->connected) {
            nsapiSizeError = socket->pending;
            nsapiSizeError = NSAPI_ERROR_OK;
        }
    } else {
        nsapiSizeError = NSAPI_ERROR_NO_CONNECTION;
    }

    return nsapiSizeError;
}

// Set a socket to be blocking.
nsapi_error_t UbloxCellularInterfaceData::set_blocking(nsapi_socket_t handle, bool blocking)
{
    nsapi_error_t nsapiError;

    if (blocking) {
        nsapiError = set_timeout(handle, -1);
    } else {
        nsapiError = set_timeout(handle, 0);
    }

    return nsapiError;
}

// Set a socket timeout.
nsapi_error_t UbloxCellularInterfaceData::set_timeout(nsapi_socket_t handle, int timeout)
{
    SockCtrl *socket = (SockCtrl *) handle;

    tr_debug("socket_set_timeout(0x%08x, %d)", (unsigned int) handle, timeout);
    socket->timeoutMilliseconds = timeout;

    return NSAPI_ERROR_OK;
}

// Find or create a socket from the list.
UbloxCellularInterfaceData::SockCtrl * UbloxCellularInterfaceData::findSocket(int modemHandle)
{
    UbloxCellularInterfaceData::SockCtrl *socket = NULL;

    for (unsigned int x = 0; (socket == NULL) && (x < sizeof(_sockets) / sizeof(_sockets[0])); x++) {
        if (_sockets[x].modemHandle == modemHandle) {
            socket = &(_sockets[x]);
        }
    }

    return socket;
}

// Callback for Socket Read URC.
void UbloxCellularInterfaceData::UUSORD_URC()
{
    int a;
    int b;
    SockCtrl *socket;

    // +UUSORD: <socket>,<length>
    if (_at->recv(": %d,%d", &a, &b)) {
        socket = findSocket(a);
        tr_debug("Socket 0x%08x: modem handle %d has %d bytes pending", (unsigned int) socket, a, b);
        if (socket != NULL) {
            socket->pending = b;
        }
    }
}

// Callback for Socket Read From URC.
void UbloxCellularInterfaceData::UUSORF_URC()
{
    int a;
    int b;
    SockCtrl *socket;

    // +UUSORF: <socket>,<length>
    if (_at->recv(": %d,%d", &a, &b)) {
        socket = findSocket(a);
        tr_debug("Socket 0x%08x: modem handle %d has %d bytes pending", (unsigned int) socket, a, b);
        if (socket != NULL) {
            socket->pending = b;
        }
    }
}

// Callback for Socket Close URC.
void UbloxCellularInterfaceData::UUSOCL_URC()
{
    int a;
    SockCtrl *socket;

    // +UUSOCL: <socket>
    if (_at->recv(": %d", &a)) {
        socket = findSocket(a);
        tr_debug("Socket 0x%08x: handle %d closed by remote host", (unsigned int) socket, a);
        if (socket != NULL) {
            socket->connected = false;
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
int UbloxCellularInterfaceData::findProfile(int modemHandle)
{
    for (unsigned int profile = 0; profile < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0])); profile++) {
        if (_httpProfiles[profile].modemHandle == modemHandle) {
            return profile;
        }
    }

    return HTTP_PROF_UNUSED;
}

/**********************************************************************
 * PUBLIC METHODS: GENERIC
 **********************************************************************/

// Constructor.
UbloxCellularInterfaceData::UbloxCellularInterfaceData(bool debugOn, PinName tx, PinName rx, int baud):
                            UbloxCellularInterfaceExt(debugOn, tx, rx, baud)
{
    // Initialise sockets storage
    memset(_sockets, 0, sizeof(_sockets));
    for (unsigned int socket = 0; socket < sizeof(_sockets) / sizeof(_sockets[0]); socket++) {
        _httpProfiles[socket].modemHandle = SOCKET_UNUSED;
    }

    // The authentication to use
    _auth = AUTH_DETECT;

    // Zero Cell Locate stuff
    _locRcvPos = 0;
    _locExpPos = 0;

    // Zero HTTP profiles
    memset(_httpProfiles, 0, sizeof(_httpProfiles));
    for (unsigned int profile = 0; profile < sizeof(_httpProfiles) / sizeof(_httpProfiles[0]); profile++) {
        _httpProfiles[profile].modemHandle = HTTP_PROF_UNUSED;
    }

    // Nullify the temporary IP address storage
    _ip = NULL;

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
    // Free _ip if it was ever allocated
    free(_ip);
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

/**********************************************************************
 * PUBLIC METHODS: HTTP
 **********************************************************************/

// Find a free profile.
int UbloxCellularInterfaceData::httpFindProfile()
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
bool UbloxCellularInterfaceData::httpSetBlocking(int profile, int timeout_ms)
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
bool UbloxCellularInterfaceData::httpSetProfileForCmdMng(int profile)
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
bool UbloxCellularInterfaceData::httpFreeProfile(int profile)
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
bool UbloxCellularInterfaceData::httpResetProfile(int httpProfile)
{
    bool success = false;
    LOCK();

    tr_debug("httpResetProfile(%d)", httpProfile);
    success = _at->send("AT+UHTTP=%d", httpProfile) && _at->recv("OK");

    UNLOCK();
    return success;
}

// Set HTTP parameters.
bool UbloxCellularInterfaceData::httpSetPar(int httpProfile, HttpOpCode httpOpCode, const char * httpInPar)
{
    bool success = false;
    int httpInParNum = 0;
    SocketAddress address;
    LOCK();

    tr_debug("httpSetPar(%d, %d, \"%s\")", httpProfile, httpOpCode, httpInPar);
    switch(httpOpCode) {
        case HTTP_IP_ADDRESS:   // 0
            if (gethostbyname(httpInPar, &address) == NSAPI_ERROR_OK) {
                success = _at->send("AT+UHTTP=%d,%d,\"%s\"", httpProfile, httpOpCode, address.get_ip_address()) && _at->recv("OK");
            }
            break;
        case HTTP_SERVER_NAME:  // 1
        case HTTP_USER_NAME:    // 2
        case HTTP_PASSWORD:     // 3
            success = _at->send("AT+UHTTP=%d,%d,\"%s\"", httpProfile, httpOpCode, httpInPar) && _at->recv("OK");
            break;

        case HTTP_AUTH_TYPE:    // 4
        case HTTP_SERVER_PORT:  // 5
            httpInParNum = atoi(httpInPar);
            success = _at->send("AT+UHTTP=%d,%d,%d", httpProfile, httpOpCode, httpInParNum) && _at->recv("OK");
            break;

        case HTTP_SECURE:       // 6
            httpInParNum = atoi(httpInPar);
            success = _at->send("AT+UHTTP=%d,%d,%d", httpProfile, httpOpCode, httpInParNum) && _at->recv("OK");
            break;

        default:
            tr_debug("httpSetPar: unknown httpOpCode %d", httpOpCode);
            break;
    }

    UNLOCK();
    return success;
}

// Perform an HTTP command.
bool UbloxCellularInterfaceData::httpCommand(int httpProfile, HttpCmd httpCmdCode, const char *httpPath, const char *httpOut,
                                             const char *httpIn, int httpContentType, const char *httpCustomPar, char *buf, int len)
{
    bool success = false;
    LOCK();

    tr_debug("%s", getHTTPcmd(httpCmdCode));
    switch (httpCmdCode) {
        case HTTP_HEAD:
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_HEAD, httpPath, httpOut) &&
                      _at->recv("OK");
            break;
        case HTTP_GET:
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_GET, httpPath, httpOut) &&
                      _at->recv("OK");
            break;
        case HTTP_DELETE:
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\"", httpProfile, HTTP_DELETE, httpPath, httpOut) &&
                      _at->recv("OK");
            break;
        case HTTP_PUT:
            // In this case the parameter httpIn is a filename
            success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\"", httpProfile, HTTP_PUT, httpPath, httpOut, httpIn) &&
                 _at->recv("OK");
            break;
        case HTTP_POST_FILE:
            // In this case the parameter httpIn is a filename
            if (httpContentType != 6) {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                               httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType) &&
                          _at->recv("OK");
            } else {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                               httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType, httpCustomPar) &&
                          _at->recv("OK");
            }
            break;
        case HTTP_POST_DATA:
            // In this case the parameter httpIn is a string containing data
            if (httpContentType != 6) {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d",
                               httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType) &&
                          _at->recv("OK");
            } else {
                success = _at->send("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%s",
                               httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType, httpCustomPar) &&
                          _at->recv("OK");
            }
            break;
        default:
            tr_debug("HTTP command not recognised");
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
                tr_debug("%s: ERROR", getHTTPcmd(httpCmdCode));
                // No more while loops
                _httpProfiles[httpProfile].pending = false;
            }
        }
    }

    UNLOCK();
    return success;
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
