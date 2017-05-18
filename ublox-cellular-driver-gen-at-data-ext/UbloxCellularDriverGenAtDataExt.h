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
     UbloxCellularDriverGenAtDataExt(PinName tx = MDMTXD,
                                     PinName rx = MDMRXD,
                                     int baud = MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
                                     bool debugOn = false);
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
     * @param profile    the HTTP profile handle.
     * @param timeout_ms -1 blocking, else non-blocking timeout in milliseconds.
     * @return           0 if successful or SOCKET_ERROR on failure.
     */
    bool httpSetBlocking(int profile, int timeout_ms);
    
    /** Set the HTTP profile for commands management.
     *
     * @param profile the HTTP profile handle.
     * @return        true if successfully, false otherwise.
     */
    bool httpSetProfileForCmdMng(int profile);
    
    /** Free the HTTP profile.
     *
     * @param profile the HTTP profile handle.
     * @return        true if successfully, false otherwise.
     */
    bool httpFreeProfile(int profile);
    
    /** Reset HTTP profile.
     *
     * @param httpProfile the HTTP profile to be reset.
     * @return            true if successfully, false otherwise.
     */
    bool httpResetProfile(int httpProfile);
    
    /** Set HTTP parameters.
     *
     * @param httpProfile the HTTP profile identifier.
     * @param httpOpCode  the HTTP operation code.
     * @param httpInPar   the HTTP input parameter.
     * @return            true if successfully, false otherwise.
     */
    bool httpSetPar(int httpProfile, HttpOpCode httpOpCode, const char * httpInPar);
    
    /** HTTP commands management.
     *
     * @param httpProfile     the HTTP profile identifier.
     * @param httpCmdCode     the HTTP command code.
     * @param httpPath        the path of HTTP server resource.
     * @param httpOut         the filename where the HTTP server response will be stored.
     * @param httpIn          the input data (filename or string) to be sent
                              to the HTTP server with the command request.
     * @param httpContentType the HTTP Content-Type identifier.
     * @param httpCustomPar   the parameter for an user defined HTTP Content-Type.
     * @param buf             the buffer to read into.
     * @param len             the size of the buffer to read into.
     * @return                true if successfully, false otherwise.
     */
    bool httpCommand(int httpProfile, HttpCmd httpCmdCode, const char* httpPath, \
                     const char* httpOut, const char* httpIn, int httpContentType, \
                     const char* httpCustomPar, char* buf, int len);
    
    /** Get HTTP command as a string.
     *
     * @param httpCmdCode the HTTP command code (reference also the enum format).
     * @return            HTTP command in string format.
     */
    const char* getHttpCmd(int httpCmdCode);

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

    /** Types of Cell Locate response.
     */
    typedef enum {
        CELL_DETAILED = 1,
        CELL_MULTIHYP = 2
    } CellRespType;

    /** Cell Locate data.
     */
    typedef struct {
        bool validData;      //!< Flag for indicating if data is valid.
        struct tm time;      //!< GPS Timestamp.
        float longitude;     //!< Estimated longitude, in degrees.
        float latitude;      //!< Estimated latitude, in degrees.
        int altitude;        //!< Estimated altitude, in meters^2.
        int uncertainty;     //!< Maximum possible error, in meters.
        int speed;           //!< Speed over ground m/s^2.
        int direction;       //!< Course over ground in degrees.
        int verticalAcc;     //!< Vertical accuracy, in meters^2.
        CellSensType sensor; //!< Sensor used for last calculation.
        int svUsed;          //!< number of satellite used.
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
 
    /** Configure Cell Locate URCs in the case of +ULOC operations.
     *
     * @param mode URC configuration: 0 disabled, 1 enabled.
     * @return     true if the request is successful, otherwise false.
     */
    bool cellLocUnsol(int mode);
 
    /** Configure Cell Locate location sensor.
     *
     * @param scanMode network scan mode: 0 normal, 1 deep scan.
     * @return         true if the request is successful, otherwise false.
     */
    bool cellLocConfig(int scanMode);
 
    /** Request Cell Locate.
     *
     * This function is non-blocking, the result is retrieved using cellLocGetxxx.
     *   
     * @param sensor     sensor selection.
     * @param timeout    timeout period in seconds (1 - 999).
     * @param accuracy   target accuracy in meters (1 - 999999).
     * @param type       detailed or multi-hypothesis.
     * @param hypothesis maximum desired number of responses from CellLocate (up to 16).
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

    /**********************************************************************
     * PROTECTED: Cell Locate
     **********************************************************************/

    /** Received positions.
     */
    int _locRcvPos;

    /** Expected positions.
     */
    int _locExpPos;

    /**  The Cell Locate data.
     */
    CellLocData _loc[CELL_MAX_HYP];

    /** Buffer for the URC to work with
     */
    char urcBuf[128];

    /** Callback to capture +UULOC.
     */
    void UULOC_URC();
};

#endif // _UBLOX_CELLULAR_DRIVER_GEN_AT_DATA_EXT_
