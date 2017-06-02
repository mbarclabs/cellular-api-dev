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

#ifndef _UBLOX_AT_CELLULAR_INTERFACE_EXT_
#define _UBLOX_AT_CELLULAR_INTERFACE_EXT_

#include "ublox_modem_driver/UbloxAtCellularInterface.h"
#include "UbloxCellularDriverGen.h"

/**UbloxATCellularInterfaceExt class.
 *
 * This interface extends the UbloxATCellularInterface to
 * include other features that use the IP stack on board the
 * cellular modem: HTTP, FTP and Cell Locate.
 *
 * Note: the UbloxCellularGeneric class is required because
 * reading a large HTTP response is performed via a modem
 * file system call and the UbloxCellularGeneric class is
 * where modem file system support is provided.
 */
class UbloxATCellularInterfaceExt : public  UbloxATCellularInterface, public UbloxCellularDriverGen {

public:
    /** Constructor.
     *
     * @param tx       the UART TX data pin to which the modem is attached.
     * @param rx       the UART RX data pin to which the modem is attached.
     * @param baud     the UART baud rate.
     * @param debugOn  true to switch AT interface debug on, otherwise false.
     */
    UbloxATCellularInterfaceExt(PinName tx = MDMTXD,
                                PinName rx = MDMRXD,
                                int baud = MBED_CONF_UBLOX_CELL_BAUD_RATE,
                                bool debugOn = false);

     /* Destructor.
      */
     virtual ~UbloxATCellularInterfaceExt();

    /**********************************************************************
     * PUBLIC: General
     **********************************************************************/

    /** Infinite timeout.
     */
    #define TIMEOUT_BLOCKING -1

    /** A struct containing an HTTP or FTP error class and code
     */
     typedef struct {
        int eClass;
        int eCode;
    } Error;

    /**********************************************************************
     * PUBLIC: HTTP
     **********************************************************************/

    /** HTTP profile unused marker.
     */
    #define HTTP_PROF_UNUSED -1

    /** HTTP configuration parameters (reference to HTTP control +UHTTP).
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

    /** Type of HTTP Command.
     */
    typedef enum {
        HTTP_HEAD = 0,
        HTTP_GET = 1,
        HTTP_DELETE = 2,
        HTTP_PUT = 3,
        HTTP_POST_FILE = 4,
        HTTP_POST_DATA = 5
    } HttpCmd;

    /** HTTP content types.
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
     * @param timeout    -1 blocking, else non-blocking timeout in milliseconds.
     * @return           true if successful, otherwise false.
     */
    bool httpSetTimeout(int profile, int timeout);
    
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
     * to set all the possible parameters (HttpOpCode).
     *
     * See section 28.1 of u-blox-ATCommands_Manual(UBX-13002752).pdf
     * for full details.  By example:
     *
     * httpOpCode          httpInPar
     * HTTP_IP_ADDRESS     "145.33.18.10" (the target HTTP server IP address)
     * HTTP_SERVER_NAME    "www.myhttpserver.com" (the target HTTP server name)
     * HTTP_USER_NAME      "my_username"
     * HTTP_PASSWORD       "my_password"
     * HTTP_AUTH_TYPE      "0" for no authentication, "1" for username/password
     *                     authentication (the default is 0)
     * HTTP_SERVER_PORT    "81" (default is port 80)
     * HTTP_SECURE         "0" for no security, "1" for TLS (the default is 0)
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
     * @return                NULL if successful, otherwise a pointer to
     *                        a Error struct containing the error class and error
     *                        code, see section Appendix A.B of
     *                        u-blox-ATCommands_Manual(UBX-13002752).pdf for details.
     */
    Error * httpCommand(int httpProfile, HttpCmd httpCmd, const char* httpPath,
                        const char* rspFile, const char* sendStr,
                        int httpContentType, const char* httpCustomPar,
                        char* buf, int len);

    /**********************************************************************
     * PUBLIC: FTP
     **********************************************************************/

    /** FTP configuration parameters (reference to FTP control +UFTP).
     */
    typedef enum {
        FTP_IP_ADDRESS = 0,
        FTP_SERVER_NAME = 1,
        FTP_USER_NAME = 2,
        FTP_PASSWORD = 3,
        FTP_ACCOUNT = 4,
        FTP_INACTIVITY_TIMEOUT = 5,
        FTP_MODE = 6,
        FTP_SERVER_PORT = 7,
        FTP_SECURE = 8,
        NUM_FTP_OP_CODES
    } FtpOpCode;

    /** Type of FTP Command.
     */
    typedef enum {
        FTP_LOGOUT = 0,
        FTP_LOGIN = 1,
        FTP_DELETE_FILE = 2,
        FTP_RENAME_FILE = 3,
        FTP_GET_FILE = 4,
        FTP_PUT_FILE = 5,
        FTP_GET_DIRECT = 6,
        FTP_PUT_DIRECT = 7,
        FTP_CD = 8,
        FTP_MKDIR = 10,
        FTP_RMDIR = 11,
        FTP_FILE_INFO = 13,
        FTP_LS = 14,
        FTP_FOTA_FILE = 100
    } FtpCmd;

    /** Set the timeout for FTP operations.
     *
     * @param timeout -1 blocking, else non-blocking timeout in milliseconds.
     * @return         true if successful, otherwise false.
     */
    bool ftpSetTimeout(int timeout);

    /** Reset the FTP configuration back to defaults.
     *
     * @return   true if successful, false otherwise.
     */
    bool ftpResetPar();
    
    /** Set FTP parameters.
     *
     * This should be called as many times as is necessary
     * to set all the possible parameters (FtpOpCode).
     *
     * See section 27.1 of u-blox-ATCommands_Manual(UBX-13002752).pdf
     * for full details.  By example:
     *
     * ftpOpCode              ftpInPar
     * FTP_IP_ADDRESS         "145.33.18.10" (the target FTP server IP address)
     * FTP_SERVER_NAME        "www.ftpserver.com" (the target FTP server name)
     * FTP_USER_NAME          "my_username"
     * FTP_PASSWORD           "my_password"
     * FTP_ACCOUNT            "my_account" (not required by most FTP servers)
     * FTP_INACTIVITY_TIMEOUT "60" (the default is 0, which means no timeout)
     * FTP_MODE               "0" for active, "1" for passive (the default is 0) 
     * FTP_SERVER_PORT        "25" (default is port 21)
     * FTP_SECURE             "0" for no security, "1" for SFTP (the default is 0)
     *
     * @param ftpOpCode  the FTP operation code.
     * @param ftpInPar   the FTP input parameter.
     * @return           true if successful, false otherwise.
     */
    bool ftpSetPar(FtpOpCode ftpOpCode, const char * ftpInPar);
    
    /** Perform an FTP command.
     *
     * Connect() must have been called previously to establish a data
     * connection.
     *
     * See section 27.2 of u-blox-ATCommands_Manual(UBX-13002752).pdf
     * for full details.  By example, it works like this:
     *
     * ftpCmd               file1      file2     offset    buf     len
     * FTP_LOGOUT            N/A        N/A       N/A      N/A     N/A 
     * FTP_LOGIN             N/A        N/A       N/A      N/A     N/A 
     * FTP_DELETE_FILE   "the_file"     N/A       N/A      N/A     N/A 
     * FTP_RENAME_FILE   "old_name"  "new_name"   N/A      N/A     N/A 
     * FTP_GET_FILE      "the_file"    Note 1  0 - 65535   N/A     N/A (Notes 2)
     * FTP_PUT_FILE      "the_file"    Note 1  0 - 65535   N/A     N/A (Note 2)
     * FTP_CD            "dir1\dir2"    N/A       N/A      N/A     N/A 
     * FTP_MKDIR         "newdir"       N/A       N/A      N/A     N/A 
     * FTP_RMDIR         "dir"          N/A       N/A      N/A     N/A 
     * FTP_FILE_INFO     "the_path"     N/A       N/A         Note 3
     * FTP_LS            "the_path"     N/A       N/A         Note 3
     * FTP_FOTA_FILE     "the_file"     N/A       N/A         Note 4
     *
     * Note 1: for this case, file2 is the name that the file should be
     * given when it arrives (in the modem file system for a GET, at the FTP
     * server for a PUT); if set to NULL then file1 is used.
     * Note 2: the file will placed into the modem file system for the
     * GET case (and can be read with readFile()), or must already be in the
     * modem file system, (can be written using writeFile()) for the PUT case.
     * Note 3: buf should point to the location where the file info
     * or directory listing is to be stored and len should be the maximum
     * length that can be stored.
     * Note 4: a hex string representing the MD5 sum of the FOTA file will be
     * stored at buf; len must be at least 32 as an MD5 sum is 16 bytes.
     * FTP_FOTA_FILE is not supported on SARA-U2.
     * Note 5: FTP_GET_DIRECT and FTP_PUT_DIRECT are not supported by
     * this driver.
     *
     * @param ftpCmd     the FTP command.
     * @param file1      the first file name if required (NULL otherwise).
     * @param file2      the second file name if required (NULL otherwise).
     * @param offset     the offset (in bytes) at which to begin the get
     *                   or put operation within the file.
     * @param buf        pointer to a buffer, required for FTP_DIRECT mode
     *                   and FTP_LS only.
     * @param len        the size of buf.
     * @return           NULL if successful, otherwise a pointer to
     *                   a Error struct containing the error class and error
     *                   code, see section Appendix A.B of
     *                   u-blox-ATCommands_Manual(UBX-13002752).pdf for details.
     */
    Error *ftpCommand(FtpCmd ftpCmd, const char* file1 = NULL, const char* file2 = NULL,
                      int offset = 0, char* buf = NULL, int len = 0);

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
     * @param timeout    the timeout for the current HTTP command.
     * @param pending    the status for the current HTTP command (in processing state or not).
     * @param cmd        the code for the current HTTP command.
     * @param result     the result for the current HTTP command once processed.
     */
     typedef struct {
         int modemHandle;
         int timeout;
         volatile bool pending;
         volatile int cmd;
         volatile int result;
         Error httpError;
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
     * PROTECTED: FTP
     **********************************************************************/

    /** Unused FTP op code marker.
     */
    #define FTP_OP_CODE_UNUSED -1

    /** The FTP timeout in milliseconds.
     */
    int _ftpTimeout;

    /** A place to store the FTP op code for the last result.
     */
    volatile int _lastFtpOpCodeResult;

    /** A place to store the last FTP result.
     */
    volatile int _lastFtpResult;

    /** A place to store the last FTP op code for data response.
     */
    volatile int _lastFtpOpCodeData;

    /** A place to store data returns from an FTP operation.
     */
    char * _ftpBuf;

    /** The length of FTP data that can be stored (at _ftpBuf).
     */
    int _ftpBufLen;

    /** storage for the last FTP error
     */
    Error _ftpError;

    /** Callback to capture the result of an FTP command.
     */
    void UUFTPCR_URC();

    /** Callback to capture data returned from an FTP command.
     */
    void UUFTPCD_URC();

    /** Helper function to get an FTP command as a text string, useful
     * for debug purposes.
     *
     * @param ftpCmdCode  the FTP command.
     * @return            FTP command in string format.
     */
    const char * getFtpCmd(FtpCmd ftpCmd);

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

#endif // _UBLOX_AT_CELLULAR_INTERFACE_EXT_
