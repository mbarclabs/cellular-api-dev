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

#ifndef _UBLOX_CELLULAR_DRIVER_GEN_
#define _UBLOX_CELLULAR_DRIVER_GEN_

#include "TARGET_UBLOX_MODEM_GENERIC/ublox_modem_driver/UbloxCellularDriverGenBase.h"

/** UbloxCellularDriverGen class
 * This interface provide SMS, USSD and
 * module File System functionality.
 */
class UbloxCellularDriverGen: virtual public UbloxCellularDriverGenBase {

public:
    UbloxCellularDriverGen(PinName tx = MDMTXD, PinName rx = MDMRXD,
                           int baud = MBED_CONF_UBLOX_CELL_GEN_DRV_BAUD_RATE,
                           bool debugOn = false);
    ~UbloxCellularDriverGen();


    /**********************************************************************
     * PUBLIC: Short Message Service
     **********************************************************************/
    
    /** Count the number of messages in the device and optionally return a
     * list with indexes from the storage locations in the device.
     *
     * Note: init() should be called before using this command.
     *
     * @param stat  what type of messages you can use:
     *              "REC UNREAD", "REC READ", "STO UNSENT", "STO SENT", "ALL".
     * @param index list where to save the storage positions.
     * @param num   number of elements in the list.
     * @return      the number of messages, this can be bigger than num, -1 on failure.
     */
    int smsList(const char* stat = "ALL", int* index = NULL, int num = 0);
    
    /** Read a message from a storage position.
     *
     * Note: init() should be called before using this command.
     *
     * @param ix  the storage position to read.
     * @param num the originator address (~16 chars).
     * @param buf a buffer where to save the sm.
     * @param len the length of the sm.
     * @return    true if successful, false otherwise.
     */
    bool smsRead(int ix, char* num, char* buf, int len);
    
    /** Delete a message.
     *
     * Note: init() should be called before using this command.
     *
     * @param index the storage position to delete.
     * @return true if successful, false otherwise.
     */
    bool smsDelete(int index);
    
    /** Send a message to a recipient.
     *
     * Note: init() and nwk_registration() should be called before
     * using this command.
     *
     * @param num the phone number of the recipient.
     * @param buf the content of the message to sent.
     * @return    true if successful, false otherwise.
     */
    bool smsSend(const char* num, const char* buf);
    
    /**********************************************************************
     * PUBLIC: Unstructured Supplementary Service Data
     **********************************************************************/
    
    /** The maximum size of a USSD string (not including terminator).
     */
    #define USSD_STRING_LENGTH 128

    /** Make a USSD query.
     *
     * Note: init() should be called before using this command and,
     * in many cases, nwk_registration() is also required as the USSD
     * command may need network access.
     *
     * Note: some USSD commands relate to call waiting, call forwarding,
     * etc., which can result in multiple responses.  This function returns
     * only the first response.  Instantiate this class with debugOn set to
     * true to get a better view of what is really going on.  If such
     * responses are important to you, you should subclass this class and
     * parse for them specifically and, probably, use specific AT commands
     * rather than USSD.
     *
     * @param cmd the ussd command to send e.g "*#100#".
     * @param buf a buffer where to save the reply, which
     *            will always be returned zero terminated.
     * @param len the length of buf, set to USSD_STRING_LENGTH + 1 to
     *            obtain the maximum length response plus terminator.
     * @return    true if successful, false otherwise.
     */
    bool ussdCommand(const char* cmd, char* buf, int len);
    
    /**********************************************************************
     * PUBLIC: Module File System
     **********************************************************************/
    
    /** Delete a file from the module's local file system.  An attempt
     * to delete a non-existent file will fail.
     *
     * Note: init() should be called before using this command.
     *
     * @param filename the name of the file.
     * @return         true if successful, false otherwise.
     */
    bool delFile(const char* filename);
    
    /** Write some data to a file in the module's local file system.
     *
     * Note: init() should be called before using this command.
     *
     * @param  filename the name of the file.
     * @param  buf the data to write.
     * @param  len the size of the data to write.
     * @return the number of bytes written.
     */
    int writeFile(const char* filename, const char* buf, int len);
    
    /** Read a file from the module's local file system.
     *
     * Note: init() should be called before using this command.
     *
     * @param  filename the name of the file
     * @param  buf a buffer to hold the data
     * @param  len the size to read
     * @return the number of bytes read
    */
    int readFile(const char* filename, char* buf, int len);
    
    /** Retrieve the file size from the module's local file system.
     *
     * Note: init() should be called before using this command.
     *
     * @param  filename the name of the file.
     * @return the file size in bytes.
     */
    int fileSize(const char* filename);
    
protected:

    /**********************************************************************
     * PROTECTED: Short Message Service
     **********************************************************************/

    /** An SMS storage buffer
     */
    #define SMS_BUFFER_SIZE 144

    /** Convert a #define to a string
     */
    #define stringify(a) str(a)
    #define str(a) #a

    /** Place to store the index of an SMS message.
     */
    int *_smsIndex;

    /** Place to store the number of SMS message stored in the module.
     */
    int _smsNum;

    /** URC for Short Message listing.
     */
    void CMGL_URC();

    /** URC for new class 0 SMS messages.
     */
    void CMTI_URC();

    /** URC for non-class 0 SMS messages.
     */
    void CMT_URC();


    /**********************************************************************
     * PROTECTED: Unstructured Supplementary Service Data
     **********************************************************************/

    /** A buffer for the string assembled from the URC.
     */
    char * _ssUrcBuf;

    /** URC for call waiting.
     */
    void CCWA_URC();

    /** URC for call forwarding.
     */
    void CCFC_URC();

    /** URC for calling lined ID restriction.
     */
    void CLIR_URC();

    /** URC for calling lined ID presentation.
     */
    void CLIP_URC();

    /** URC for connected lined ID restriction.
     */
    void COLR_URC();

    /** URC for connected lined ID presentation.
     */
    void COLP_URC();

    /**********************************************************************
     * PROTECTED: File System
     **********************************************************************/

    /** The maximum size of a single read of file data.
     */
    #define FILE_BUFFER_SIZE 128
};

#endif // _UBLOX_CELLULAR_DRIVER_GEN_
