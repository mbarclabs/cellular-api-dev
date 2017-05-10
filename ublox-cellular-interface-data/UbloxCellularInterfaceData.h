/* Copyright (c) 2017 ARM Limited
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

#ifndef _UBLOX_CELLULAR_INTERFACE_DATA_
#define _UBLOX_CELLULAR_INTERFACE_DATA_

#include "NetworkStack.h"
#include "UbloxCellularInterfaceExt.h"

/** UbloxCellularInterfaceData class.
 *
 * This interface extends the UbloxCellularInterface to
 * include features that use the IP stack on board the
 * cellular modem: HTTP and Cell Locate.
 */
class UbloxCellularInterfaceData : public NetworkStack, public UbloxCellularInterfaceExt {

public:
     UbloxCellularInterfaceData(bool debugOn = false, PinName tx = MDMTXD, PinName rx = MDMRXD, int baud = MBED_CONF_UBLOX_MODEM_GENERIC_BAUD_RATE);
    ~UbloxCellularInterfaceData();

    /**********************************************************************
     * PUBLIC: Generic
     **********************************************************************/

    /** Types of authentication.
     */
    typedef enum {
        AUTH_NONE,
        AUTH_PAP,
        AUTH_CHAP,
        AUTH_DETECT
    } Auth;

    /** Similar to connect(), but using the IP stack that is
     *  on-board the cellular modem instead of the LWIP stack
     *  on the mbed MCU.  Once this function has returned
     *  successfully the HTTP and Cell Locate services can be
     *  used.
     *
     *  There are two advantages to using this mechanism:
     *
     *  1.  Since the modem interface remains in AT mode
     *      throughout, it is possible to continue using
     *      any AT commands (e.g. send SMS, use the module's
     *      file system, etc.) while the connection is up.
     *
     *  2.  LWIP is not required (and hence RAM is saved).
     *
     *  The disadvantage is that some additional parsing
     *  (at the AT interface) has to go on in order to exchange
     *  IP packets.
     *
     *  Note: init() must be called first if a SIM PIN has to be
     *  entered.
     *
     *  @param apn         optionally, access point name.
     *  @param uname       optionally, Username.
     *  @param pwd         optionally, password.
     *  @param auth        optionally, the type of authentication to use.
     *  @return            NSAPI_ERROR_OK on success, or negative error code on failure.
     */
    virtual nsapi_error_t join(const char *apn = 0, const char *uname = 0,
                               const char *pwd = 0, Auth auth = AUTH_DETECT);

    /** Attempt to disconnect from the network.
     *
     *  Brings down the network interface. If the PPP interface
     *  is connected, this will also be disconnected.
     *  Does not bring down the Radio network.
     *
     *  @return         0 on success, negative error code on failure.
     */
    virtual nsapi_error_t disconnect();

    /**********************************************************************
     * PUBLIC: HTTP
     **********************************************************************/

    /** HTTP Profile error return codes.
     */
    #define HTTP_PROF_UNUSED -1

    /** Type of HTTP Operational Codes (reference to HTTP control +UHTTP).
     */
    typedef enum {
        HTTP_IP_ADDRESS,
        HTTP_SERVER_NAME,
        HTTP_USER_NAME,
        HTTP_PASSWORD,
        HTTP_AUTH_TYPE,
        HTTP_SERVER_PORT,
        HTTP_SECURE
    } HttpOpCode;

    /** Type of HTTP Commands (reference to HTTP command +UHTTPC).
     */
    typedef enum {
        HTTP_HEAD,
        HTTP_GET,
        HTTP_DELETE,
        HTTP_PUT,
        HTTP_POST_FILE,
        HTTP_POST_DATA
    } HttpCmd;

    /** Find HTTP profile.
     *
     * @return true if successfully, false otherwise.
     */
    int httpFindProfile();
    
    /** Get the number of bytes pending for reading for this HTTP profile.
     *
     * @param profile the HTTP profile handle.
     * @param timeout_ms -1 blocking, else non blocking timeout in ms.
     * @return 0 if successful or SOCKET_ERROR on failure.
     */
    bool httpSetBlocking(int profile, int timeout_ms);
    
    /** Set the HTTP profile for commands management.
     *
     * @param profile the HTTP profile handle.
     * @return true if successfully, false otherwise.
     */
    bool httpSetProfileForCmdMng(int profile);
    
    /** Free the HTTP profile.
     *
     * @param profile the HTTP profile handle.
     * @return true if successfully, false otherwise.
     */
    bool httpFreeProfile(int profile);
    
    /** Reset HTTP profile.
     *
     * @param httpProfile the HTTP profile to be reset.
     * @return true if successfully, false otherwise.
     */
    bool httpResetProfile(int httpProfile);
    
    /** Set HTTP parameters.
     *
     * @param httpProfile the HTTP profile identifier.
     * @param httpOpCode the HTTP operation code.
     * @param httpInPar the HTTP input parameter.
     * @return true if successfully, false otherwise.
     */
    bool httpSetPar(int httpProfile, HttpOpCode httpOpCode, const char * httpInPar);
    
    /** HTTP commands management.
     *
     * @param httpProfile the HTTP profile identifier.
     * @param httpCmdCode the HTTP command code.
     * @param httpPath the path of HTTP server resource.
     * @param httpOut the filename where the HTTP server response will be stored.
     * @param httpIn the input data (filename or string) to be sent 
                     to the HTTP server with the command request.
     * @param httpContentType the HTTP Content-Type identifier.
     * @param httpCustomPar the parameter for an user defined HTTP Content-Type.
     * @param buf the buffer to read into.
     * @param len the size of the buffer to read into.
     * @return true if successfully, false otherwise.
     */
    bool httpCommand(int httpProfile, HttpCmd httpCmdCode, const char* httpPath, \
                     const char* httpOut, const char* httpIn, int httpContentType, \
                     const char* httpCustomPar, char* buf, int len);
    
    /** Get HTTP command as a string.
     *
     * @param httpCmdCode the HTTP command code (reference also the enum format).
     * @return HTTP command in string format.
     */
    const char* getHTTPcmd(int httpCmdCode);

    /**********************************************************************
     * PUBLIC: Cell Locate
     **********************************************************************/
 
    #define CELL_MAX_HYP    (16 + 1)

    /** Which form of Cell Locate sensing to use.
     */
    typedef enum {
        CELL_LAST = 0,
        CELL_GNSS,
        CELL_LOCATE,
        CELL_HYBRID
    } CellSensType;

    /**  Types of Cell Locate response.
     */
    typedef enum {
        CELL_DETAILED = 1,
        CELL_MULTIHYP = 2
    } CellRespType;

    int _locRcvPos;     //!< Received positions
    int _locExpPos;     //!< Expected positions

    /** Cell Locate Data.
     */
    typedef struct {
        bool validData;      //!< Flag for indicating if data is valid
        struct tm time;      //!< GPS Timestamp
        float longitude;     //!< Estimated longitude, in degrees
        float latitude;      //!< Estimated latitude, in degrees
        int altitutude;      //!< Estimated altitude, in meters^2
        int uncertainty;     //!< Maximum possible error, in meters
        int speed;           //!< Speed over ground m/s^2
        int direction;       //!< Course over ground in degrees
        int verticalAcc;     //!< Vertical accuracy, in meters^2
        CellSensType sensor; //!< Sensor used for last calculation
        int svUsed;          //!< number of satellite used
    } CellLocData;

    /** Configures CellLocate Tcp Aiding server.
     *
     * @param server_1   Host name of the primary MGA server.
     * @param server_2   Host name of the secondary MGA server.
     * @param token      Authentication Token for MGA server access.
     * @param days       The number of days into the future the Offline data for the u-blox 7.
     * @param period     The number of weeks into the future the Offline data for u-blox M8.
     * @param resolution Resolution of offline data for u-blox M8: 1 everyday, 0 every other day.
     */
    int cellLocSrvTcp(const char* token, const char* server_1 = "cell-live1.services.u-blox.com", \
                      const char* server_2 = "cell-live2.services.u-blox.com", int days = 14, int period = 4, int resolution = 1);
 
    /** Configures  CellLocate Udp Aiding server.
     * @param server_1   Host name of the primary MGA server.
     * @param port       Server port.
     * @param latency    Expected network latency in seconds from 0 to 10000ms.
     * @param mode       Assist now management, mode of operation: 0 data downloaded at GNSS power up,
     *                   1 automatically kept alive, manual download.
     */
    int cellLocSrvUdp(const char* server_1 = "cell-live1.services.u-blox.com", int port = 46434, int latency = 1000, int mode = 0);
 
    /** Configures CellLocate URCs in the case of +ULOC operations.
     *
     * @param mode Urc configuration: 0 disabled, 1 enabled.
     */
    int cellLocUnsol(int mode);
 
    /** Configures CellLocate location sensor.
     *
     * @param scanMode Network scan mode: 0 normal, 1 deep scan.
     */
    int cellLocConfig(int scanMode);
 
    /** Request CellLocate.
     * This function is non-blocking, the result has to be retrived using cellLocGet.
     *   
     * @param sensor     Sensor selection.
     * @param timeout    Timeout period in seconds (1 - 999).
     * @param accuracy   Target accuracy in meters (1 - 999999).
     * @param type
     * @param hypotesis  Maximum desired number of responses from CellLocate® (up to 16).
     */
    int cellLocRequest(CellSensType sensor, int timeout, int accuracy, CellRespType type = CELL_DETAILED,int hypotesis = 1);
 
    /** Get a position record.
     *
     * @param data pointer to a CellLocData struct where the location will be copied in.
     * @param index of the position to retrive.
     * @return 1 if data has been retrived and copied, 0 otherwise.
     */
    int cellLocGetData(CellLocData *data, int index =0);
    
    /** Get number of position records received.
     *
     * @return number of position received.
     */
    int cellLocGetRes();
    
    /** Get expected number of position to be received.
     *
     * @return number of expected position to be received.
     */
    int cellLocGetExpRes();
    
protected:

    /**********************************************************************
     * PROTECTED: Generic
     **********************************************************************/

    /** Activate one of the on-board modem's connection profiles.
     *
     * @param apn      the apn to use.
     * @param username the username to use.
     * @param password the password to use.
     * @param auth     the authentication method to use.
     * @return true if successful, otherwise false.
     */
    bool activateProfile(const char* apn, const char* username, const char* password, Auth auth);

    /** Activate a profile using the existing external connection.
     *
     * @return true if successful, otherwise false.
     */
    bool activateProfileReuseExternal(void);

    /** Activate a profile based on connection ID.
     *
     * @param cid       the connection ID.
     * @param apn       the apn to use.
     * @param username  the username to use.
     * @param password  the password to use.
     * @param auth      the authentication method to use.
     * @return true if successful, otherwise false.
     */
    bool activateProfileByCid(int cid, const char* apn, const char* username, const char* password, Auth auth);

    /** Connect the on board IP stack of the modem.
     *
     * @return true if successful, otherwise false.
     */
    bool connectModemStack();

    /** Disconnect the on board IP stack of the modem.
     *
     * @return true if successful, otherwise false.
     */
    bool disconnectModemStack();

    /** The type of authentication to use.
     */
    Auth _auth;

    /**********************************************************************
     * PROTECTED: Sockets
     **********************************************************************/

    /** Infinite timeout.
     */
    #define TIMEOUT_BLOCKING -1

    /** Socket "unused" value
     */
    #define SOCKET_UNUSED - 1

    /** Maximum number of bytes that can be written to a socket.
     */
    #define MAX_WRITE_SIZE 1024

    /** Maximum number of bytes that can be read from a socket.
     */
    #define MAX_READ_SIZE 128

    /** Management structure for sockets.
     */
    typedef struct {
        int modemHandle;
        int timeoutMilliseconds;
        volatile bool connected;
        volatile nsapi_size_t pending;
    } SockCtrl;

    /** Socket storage.
     */
    SockCtrl _sockets[12];

    /** Storage for a single IP address.
     */
    char *_ip;

    /** Provide access to the NetworkStack object
     *
     *  @return The underlying NetworkStack object
     */
     virtual NetworkStack *get_stack();

    /** Get the local IP address.
     *
     *  @return         Null-terminated representation of the local IP address
     *                  or null if not yet connected.
     */
    const char *get_ip_address();

    /** Translates a hostname to an IP address with specific version.
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param host     Hostname to resolve.
     *  @param address  Destination for the host SocketAddress.
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC).
     *  @return         0 on success, negative error code on failure.
     */
    virtual nsapi_error_t gethostbyname(const char *host,
                                        SocketAddress *address,
                                        nsapi_version_t version = NSAPI_UNSPEC);

    /** Open a socket.
     *
     *  Creates a network socket and stores it in the specified handle.
     *  The handle must be passed to following calls on the socket.
     *
     *  @param handle   Destination for the handle to a newly created socket.
     *  @param proto    Protocol of socket to open, NSAPI_TCP or NSAPI_UDP.
     *  @param port     Optional port to bind to.
     *  @return         0 on success, negative error code on failure.
     */
    virtual nsapi_error_t socket_open(nsapi_socket_t *handle, nsapi_protocol_t proto);

    /** Close a socket.
     *
     *  Closes any open connection and deallocates any memory associated
     *  with the socket.
     *
     *  @param handle   Socket handle.
     *  @return         0 on success, negative error code on failure.
     */
    virtual nsapi_error_t socket_close(nsapi_socket_t handle);

    /** Bind a specific address to a socket.
     *
     *  Binding a socket specifies the address and port on which to receive
     *  data. If the IP address is zeroed, only the port is bound.
     *
     *  @param handle   Socket handle
     *  @param address  Local address to bind
     *  @return         0 on success, negative error code on failure.
     */
    virtual nsapi_error_t socket_bind(nsapi_socket_t handle, const SocketAddress &address);

    /** Connects TCP socket to a remote host.
     *
     *  Initiates a connection to a remote server specified by the
     *  indicated address.
     *
     *  @param handle   Socket handle
     *  @param address  The SocketAddress of the remote host
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t socket_connect(nsapi_socket_t handle, const SocketAddress &address);

    /** Send data over a TCP socket.
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes sent from the buffer.
     *
     *  @param handle   Socket handle.
     *  @param data     Buffer of data to send to the host.
     *  @param size     Size of the buffer in bytes.
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure.
     */
    virtual nsapi_size_or_error_t socket_send(nsapi_socket_t handle,
                                              const void *data, nsapi_size_t size);

    /** Send a packet over a UDP socket.
     *
     *  Sends data to the specified address. Returns the number of bytes
     *  sent from the buffer.
     *
     *  @param handle   Socket handle.
     *  @param address  The SocketAddress of the remote host.
     *  @param data     Buffer of data to send to the host.
     *  @param size     Size of the buffer in bytes.
     *  @return         Number of sent bytes on success, negative error
     *                  code on failure.
     */
    virtual nsapi_size_or_error_t socket_sendto(nsapi_socket_t handle, const SocketAddress &address,
                                                const void *data, nsapi_size_t size);

    /** Receive data over a TCP socket.
     *
     *  The socket must be connected to a remote host. Returns the number of
     *  bytes received into the buffer.
     *
     *  @param handle   Socket handle.
     *  @param data     Destination buffer for data received from the host.
     *  @param size     Size of the buffer in bytes.
     *  @return         Number of received bytes on success, negative error
     *                  code on failure.
     */
    virtual nsapi_size_or_error_t socket_recv(nsapi_socket_t handle,
                                              void *data, nsapi_size_t size);

    /** Receive a packet over a UDP socket.
     *
     *  Receives data and stores the source address in address if address
     *  is not NULL. Returns the number of bytes received into the buffer.
     *
     *  @param handle   Socket handle.
     *  @param address  Destination for the source address or NULL.
     *  @param data     Destination buffer for data received from the host.
     *  @param size     Size of the buffer in bytes.
     *  @return         Number of received bytes on success, negative error
     *                  code on failure.
     */
    virtual nsapi_size_or_error_t socket_recvfrom(nsapi_socket_t handle, SocketAddress *address,
                                                  void *data, nsapi_size_t size);

    /** Listen for connections on a TCP socket
     *
     *  Marks the socket as a passive socket that can be used to accept
     *  incoming connections.
     *
     *  @param backlog  Number of pending connections that can be queued
     *                  simultaneously, defaults to 1
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t socket_listen(nsapi_socket_t handle, int backlog);

    /** Accepts a connection on a TCP socket
     *
     *  The server socket must be bound and set to listen for connections.
     *  On a new connection, creates a network socket and stores it in the
     *  specified handle. The handle must be passed to following calls on
     *  the socket.
     *
     *  A stack may have a finite number of sockets, in this case
     *  NSAPI_ERROR_NO_SOCKET is returned if no socket is available.
     *
     *  This call is non-blocking. If accept would block,
     *  NSAPI_ERROR_WOULD_BLOCK is returned immediately.
     *
     *  @param server   Socket handle to server to accept from
     *  @param handle   Destination for a handle to the newly created socket
     *  @param address  Destination for the remote address or NULL
     *  @return         0 on success, negative error code on failure
     */
     virtual nsapi_error_t socket_accept(nsapi_socket_t server,
                                         nsapi_socket_t *handle, SocketAddress *address=0);

    /** Register a callback on state change of the socket
     *
     *  The specified callback will be called on state changes such as when
     *  the socket can recv/send/accept successfully and on when an error
     *  occurs. The callback may also be called spuriously without reason.
     *
     *  The callback may be called in an interrupt context and should not
     *  perform expensive operations such as recv/send calls.
     *
     *  @param handle   Socket handle
     *  @param callback Function to call on state change
     *  @param data     Argument to pass to callback
     */
    virtual void socket_attach(nsapi_socket_t handle, void (*callback)(void *), void *data);

    /** Get the number of bytes pending for reading for this socket.
     *
     * @param handle    The socket handle.
     * @return          The number of bytes pending on success, negative error
     *                  code on failure.
     */
    nsapi_size_or_error_t socket_readable(nsapi_socket_t handle);

    /** Set blocking or non-blocking mode of the socket.
     *
     *  set_blocking(false) is equivalent to set_timeout(-1).
     *  set_blocking(true) is equivalent to set_timeout(0).
     *
     *  @param blocking true for blocking mode, false for non-blocking mode.
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t set_blocking(nsapi_socket_t handle, bool blocking);

    /** Set timeout on blocking socket operations.
     *
     *  set_timeout(-1) is equivalent to set_blocking(true).
     *
     *  @param timeout  Timeout in milliseconds
     *  @return         0 on success, negative error code on failure
     */
    nsapi_error_t set_timeout(nsapi_socket_t handle, int timeout);

    /** Find or create the socket for a modem handle.
     *
     * @param modemHandle  the modem handle, the socket for it is to be found.
     * @return             the socket, or NULL if not found/created.
     */
    SockCtrl * findSocket(int modemHandle = SOCKET_UNUSED);

    /** Callback for Socket Read URC.
     */
    void UUSORD_URC();

    /** Callback for Socket Read From URC.
     */
    void UUSORF_URC();

    /** Callback for Socket Close URC.
     */
    void UUSOCL_URC();

    /**********************************************************************
     * PROTECTED: HTTP
     **********************************************************************/

    /**
     * The profile to use.
     */
    #define PROFILE "0"

    /**
     * Test if an HTTP profile is OK to use.
     */
    #define IS_PROFILE(p) (((p) >= 0) && (((unsigned int) p) < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0]))) \
                           && (_httpProfiles[p].modemHandle != HTTP_PROF_UNUSED))

    /** Management structure for HTTP profiles.
     *
     * It is possible to have up to 4 different HTTP profiles (LISA-C200, LISA-U200 and SARA-G350) having:
     *
     * @param handle the current HTTP profile is in handling state or not (default value is HTTP_ERROR).
     * @param timeout_ms the timeout for the current HTTP command.
     * @param pending the status for the current HTTP command (in processing state or not).
     * @param cmd the code for the current HTTP command.
     * @param result the result for the current HTTP command once processed.
     */
     typedef struct {
         int modemHandle;
         int timeout_ms;
         bool pending;
         int cmd;
         int result;
     } HttpProfCtrl;

    /** The HTTP profile storage.
     */
    HttpProfCtrl _httpProfiles[4];

    /** Callback to capture the response to an HTTP command.
     */
    void UUHTTPCR_URC();

    /** Find a profile with a given handle.  If no handle is given, find the next
     * free profile.
     *
     * @param modemHandle the handle of the profile to find.
     * @return            the profile handle or negative if not found/created.
     */
    int findProfile(int modemHandle = HTTP_PROF_UNUSED);
};

#endif // _UBLOX_CELLULAR_INTERFACE_DATA_
