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

#ifndef _UBLOX_CELLULAR_DRIVER_GEN_AT_DATA_EXT_
#define _UBLOX_CELLULAR_DRIVER_GEN_AT_DATA_EXT_

#include "TARGET_UBLOX_MODEM_GENERIC_AT_DATA/ublox_modem_driver/UbloxCellularDriverGenAtData.h"
#include "UbloxCellularDriverGen.h"

/** UbloxCellularDriverGenAtDataExt class.
 *
 * This interface extends the UbloxCellularInterfaceAtData to
 * include other features that use the IP stack on board the
 * cellular modem: HTTP and Cell Locate.
 *
 * Note: the UbloxCellularGeneric class is required because
 * reading a large HTTP response is performed via a modem
 * file system call and the UbloxCellularGeneric class is
 * where modem file system support is provided.
 */
class UbloxCellularDriverGenAtDataExt : public UbloxCellularDriverGenAtData, public UbloxCellularDriverGen {

public:
    /** Constructor.
     *
     * @param tx       the UART TX data pin to which the modem is attached.
     * @param rx       the UART RX data pin to which the modem is attached.
     * @param baud     the UART baud rate.
     * @param debugOn  true to switch AT interface debug on, otherwise false.
     */
     UbloxCellularDriverGenAtDataExt(PinName tx = MDMTXD,
                                     PinName rx = MDMRXD,
                                     int baud = MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
                                     bool debugOn = false);

     /* Destructor.
      */
     virtual ~UbloxCellularDriverGenAtDataExt();

    /**********************************************************************
     * PUBLIC: HTTP
     **********************************************************************/

    /** Infinite timeout.
     */
    #define TIMEOUT_BLOCKING -1

    /** HTTP Profile error return codes.
     */
    #define HTTP_PROF_UNUSED -1

    /** Type of HTTP Operational Codes (reference to HTTP control +UHTTP).
     */
    typedef enum {
        HTTP_IP_ADDRESS = 0,
        HTTP_SERVER_NAME = 1,
        HTTP_USER_NAME = 2,
        HTTP_PASSWORD = 3,
        HTTP_AUTH_TYPE = 4,
        HTTP_SERVER_PORT = 5,
        HTTP_SECURE = 6
    } HttpOpCode;

    /** Type of HTTP Commands.
     */
    typedef enum {
        HTTP_HEAD = 0,
        HTTP_GET = 1,
        HTTP_DELETE = 2,
        HTTP_PUT = 3,
        HTTP_POST_FILE = 4,
        HTTP_POST_DATA = 5
    } HttpCmd;

    /** HTTP content types
     */
    typedef enum {
        HTTP_CONTENT_URLENCODED = 0,
        HTTP_CONTENT_TEXT = 1,
        HTTP_CONTENT_OCTET_STREAM = 2,
        HTTP_CONTENT_FORM_DATA = 3,
        HTTP_CONTENT_JSON = 4,
        HTTP_CONTENT_XML = 5,
        HTTP_CONTENT_USER_DEFINED = 6
    } HttpContentType;

    /** Find a free HTTP profile.
     *
     * A profile will be blocking when first allocated.
     *
     * @return the profile or negative if none are available.
     */
    int httpAllocProfile();
    
    /** Free the HTTP profile.
     *
     * @param profile the HTTP profile handle.
     * @return           true if successful, otherwise false.
     */
    bool httpFreeProfile(int profile);

    /** Set the timeout for this profile.
     *
     * @param profile    the HTTP profile handle.
     * @param timeout_ms -1 blocking, else non-blocking timeout in milliseconds.
     * @return           true if successful, otherwise false.
     */
    bool httpSetTimeout(int profile, int timeout_ms);
    
    /** Reset a HTTP profile back to defaults.
     *
     * This may be called if the state of a HTTP profile
     * during parameter setting or exchange of HTTP commands
     * has become confusing/unknown.
     *
     * @param httpProfile the HTTP profile to be reset.
     * @return            true if successful, false otherwise.
     */
    bool httpResetProfile(int httpProfile);
    
    /** Set HTTP parameters.
     *
     * This should be called as many times as is necessary
     * to set all the possible parameters (HttpOpCodes).
     *
     * See section 28.1 of u-blox-ATCommands_Manual(UBX-13002752).pdf
     * for full details.  By example:
     *
     * httpOpCode          httpInPar
     * HTTP_IP_ADDRESS     "145.33.18.10" (the target server IP address)
     * HTTP_SERVER_NAME    "www.myserver.com" (the target server name)
     * HTTP_USER_NAME      "my_username"
     * HTTP_PASSWORD       "my_password"
     * HTTP_AUTH_TYPE      "0" for no authentication, "1" for username/password
     *                     authentication
     * HTTP_SERVER_PORT    "81" (default is port 80)
     * HTTP_SECURE         "0" for no security, "1" for TLS
     *
     * @param httpProfile the HTTP profile identifier.
     * @param httpOpCode  the HTTP operation code.
     * @param httpInPar   the HTTP input parameter.
     * @return            true if successful, false otherwise.
     */
    bool httpSetPar(int httpProfile, HttpOpCode httpOpCode, const char * httpInPar);
    
    /** Perform a HTTP command.
     *
     * See section 28.3 of u-blox-ATCommands_Manual(UBX-13002752).pdf
     * for full details.  By example, it works like this:
     *
     * httpCmd        httpPath       rspFile     sendStr  httpContentType httpCustomPar
     * HEAD       "path/file.html"     NULL       NULL         0             NULL
     * GET        "path/file.html"     NULL       NULL         0             NULL
     * DELETE     "path/file.html"     NULL       NULL         0             NULL
     * PUT        "path/file.html"     NULL    "myfile.txt"  0 to 6         Note 1
     * POST_FILE  "path/file.html"     NULL    "myfile.txt"  0 to 6         Note 1
     * POST       "path/file.html"     NULL   "hello there!" 0 to 6         Note 1
     *
     * Note 1: httpCustomPar is only applicable when httpContentType = HTTP_CONTENT_USER_DEFINED.
     *
     * The server to which this command is directed must have previously been
     * set with a call to httpSetPar().  If the server requires TLS (i.e. "HTTPS"),
     * then set that up with httpSetPar() also (HTTP_SECURE).
     *
     * rspFile may be left as NULL as the server response will be returned in buf.
     * Alternatively, a rspFile may be given (e.g. "myresponse.txt") and this can
     * later be read from the modem file system using readFile().
     *
     * @param httpProfile     the HTTP profile identifier.
     * @param httpCmd         the HTTP command.
     * @param httpPath        the path of resource on the HTTP server.
     * @param rspFile         the local modem file where the server
     *                        response will be stored, use NULL for
     *                        don't care.
     * @param sendStr         the filename or string to be sent
     *                        to the HTTP server with the command request.
     * @param httpContentType the HTTP Content-Type identifier.
     * @param httpCustomPar   the parameter for a user defined HTTP Content-Type.
     * @param buf             the buffer to read into.
     * @param len             the size of the buffer to read into.
     * @return                true if successful, false otherwise.
     */
    bool httpCommand(int httpProfile, HttpCmd httpCmd, const char* httpPath,
                     const char* rspFile, const char* sendStr,
                     int httpContentType, const char* httpCustomPar,
                     char* buf, int len);

    /**********************************************************************
     * PUBLIC: Cell Locate
     **********************************************************************/
 
    /** Which form of Cell Locate sensing to use.
     */
    typedef enum {
        CELL_LAST,
        CELL_GNSS,
        CELL_LOCATE,
        CELL_HYBRID
    } CellSensType;

    /** Types of Cell Locate response.
     */
    typedef enum {
        CELL_DETAILED = 1,
        CELL_MULTIHYP = 2
    } CellRespType;

    /** Cell Locate data.
     */
    typedef struct {
        volatile bool validData;      //!< Flag for indicating if data is valid.
        volatile struct tm time;      //!< GPS Timestamp.
        volatile float longitude;     //!< Estimated longitude, in degrees.
        volatile float latitude;      //!< Estimated latitude, in degrees.
        volatile int altitude;        //!< Estimated altitude, in meters^2.
        volatile int uncertainty;     //!< Maximum possible error, in meters.
        volatile int speed;           //!< Speed over ground m/s^2.
        volatile int direction;       //!< Course over ground in degrees.
        volatile int verticalAcc;     //!< Vertical accuracy, in meters^2.
        volatile CellSensType sensor; //!< Sensor used for last calculation.
        volatile int svUsed;          //!< number of satellite used.
    } CellLocData;

    /** Configure the Cell Locate TCP aiding server.
     *
     * Connect() must have been called previously to establish
     * a data connection.
     *
     * @param server_1   host name of the primary MGA server.
     * @param server_2   host name of the secondary MGA server.
     * @param token      authentication token for MGA server access.
     * @param days       the number of days into the future the off-line
     *                   data for the u-blox 7.
     * @param period     the number of weeks into the future the off-line
     *                   data for u-blox M8.
     * @param resolution resolution of off-line data for u-blox M8: 1 every
     *                   day, 0 every other day.
     * @return           true if the request is successful, otherwise false.
     */
    bool cellLocSrvTcp(const char* token, const char* server_1 = "cell-live1.services.u-blox.com",
                       const char* server_2 = "cell-live2.services.u-blox.com",
                       int days = 14, int period = 4, int resolution = 1);
 
    /** Configure Cell Locate UDP aiding server.
     *
     * Connect() must have been called previously to establish
     * a data connection.
     *
     * @param server_1   host name of the primary MGA server.
     * @param port       server port.
     * @param latency    expected network latency in seconds from 0 to 10000 milliseconds.
     * @param mode       Assist Now management, mode of operation:
     *                   0 data downloaded at GNSS power up,
     *                   1 automatically kept alive, manual download.
     * @return           true if the request is successful, otherwise false.
     */
    bool cellLocSrvUdp(const char* server_1 = "cell-live1.services.u-blox.com",
                       int port = 46434, int latency = 1000, int mode = 0);
 
    /** Configure Cell Locate location sensor.
     *
     * @param scanMode network scan mode: 0 normal, 1 deep scan.
     * @return         true if the request is successful, otherwise false.
     */
    bool cellLocConfig(int scanMode);
 
    /** Request a one-shot Cell Locate.
     *
     * This function is non-blocking, the result is retrieved using cellLocGetxxx.
     *
     * Note: during the location process, unsolicited result codes will be returned
     * by the modem indicating progress and potentially flagging interesting errors.
     * In order to see these errors, instantiate this class with debugOn set to true.
     *   
     * @param sensor     sensor selection.
     * @param timeout    timeout period in seconds (1 - 999).
     * @param accuracy   target accuracy in meters (1 - 999999).
     * @param type       detailed or multi-hypothesis.
     * @param hypothesis maximum desired number of responses from CellLocate (up to 16),
     *                   must be 1 if type is CELL_DETAILED.
     * @return           true if the request is successful, otherwise false.
     */
    bool cellLocRequest(CellSensType sensor, int timeout, int accuracy,
                        CellRespType type = CELL_DETAILED, int hypothesis = 1);

    /** Get a position record.
     *
     * @param data  pointer to a CellLocData structure where the location will be put.
     * @param index of the position to retrieve.
     * @return      true if data has been retrieved and copied, false otherwise.
     */
    bool cellLocGetData(CellLocData *data, int index = 0);
    
    /** Get the number of position records received.
     *
     * @return number of position records received.
     */
    int cellLocGetRes();
    
    /** Get the number of position records expected to be received.
     *
     * @return number of position records expected to be received.
     */
    int cellLocGetExpRes();
    
protected:

    /**********************************************************************
     * PROTECTED: HTTP
     **********************************************************************/

    /** Check for timeout.
     */
    #define TIMEOUT(t, ms)  ((ms != TIMEOUT_BLOCKING) && (ms < t.read_ms()))

    /** Check for a valid profile.
     */
    #define IS_PROFILE(p) (((p) >= 0) && (((unsigned int) p) < (sizeof(_httpProfiles)/sizeof(_httpProfiles[0]))) \
                           && (_httpProfiles[p].modemHandle != HTTP_PROF_UNUSED))

    /** Management structure for HTTP profiles.
     *
     * It is possible to have up to 4 different HTTP profiles (LISA-C200, LISA-U200 and SARA-G350) having:
     *
     * @param handle     the current HTTP profile is in handling state or not (default value is HTTP_ERROR).
     * @param timeout_ms the timeout for the current HTTP command.
     * @param pending    the status for the current HTTP command (in processing state or not).
     * @param cmd        the code for the current HTTP command.
     * @param result     the result for the current HTTP command once processed.
     */
     typedef struct {
         int modemHandle;
         int timeout_ms;
         volatile bool pending;
         volatile int cmd;
         volatile int result;
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


    /** Helper function to get a HTTP command as a text string, useful
     * for debug purposes.
     *
     * @param httpCmdCode the HTTP command.
     * @return            HTTP command in string format.
     */
    const char* getHttpCmd(HttpCmd httpCmd);

    /**********************************************************************
     * PROTECTED: Cell Locate
     **********************************************************************/

    /**  The maximum number of hypotheses
     */
    #define CELL_MAX_HYP    (16 + 1)

    /** Received positions.
     */
    volatile int _locRcvPos;

    /** Expected positions.
     */
    volatile int _locExpPos;

    /**  The Cell Locate data.
     */
    CellLocData _loc[CELL_MAX_HYP];

    /** Buffer for the URC to work with
     */
    char urcBuf[128];

    /** Callback to capture +UULOCIND.
     */
    void UULOCIND_URC();

    /** Callback to capture +UULOC.
     */
    void UULOC_URC();
};

#endif // _UBLOX_CELLULAR_DRIVER_GEN_AT_DATA_EXT_
