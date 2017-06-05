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

#include "ublox_modem_driver/UbloxCellularBase.h"

/** UbloxCellularDriverGen class
 * This interface provide SMS, USSD and
 * module File System functionality.
 */
class UbloxCellularDriverGen: virtual public UbloxCellularBase {

public:
    /** Constructor.
     *
     * @param tx       the UART TX data pin to which the modem is attached.
     * @param rx       the UART RX data pin to which the modem is attached.
     * @param baud     the UART baud rate.
     * @param debugOn  true to switch AT interface debug on, otherwise false.
     */
    UbloxCellularDriverGen(PinName tx = MDMTXD, PinName rx = MDMRXD,
                           int baud = MBED_CONF_UBLOX_CELL_BAUD_RATE,
                           bool debugOn = false);

    /* Destructor.
     */
    ~UbloxCellularDriverGen();

    /**********************************************************************
     * PUBLIC: Short Message Service
     **********************************************************************/
    
    /** The size of an SMS storage buffer, including null terminator.
     */
    #define SMS_BUFFER_SIZE 145

    /** Count the number of messages in the device and optionally return a
     * list with indexes from the storage locations in the device.
     *
     * Note: init() should be called before this method can be used.
     *
     * @param stat  what type of messages you can use:
     *              "REC UNREAD", "REC READ", "STO UNSENT", "STO SENT", "ALL".
     * @param index list where to save the storage positions (pointer
     *              to an array of ints).
     * @param num   number of elements that can be stored in the list.
     * @return      the number of messages, this can be bigger than num,
     *              -1 on failure.
     */
    int smsList(const char* stat = "ALL", int* index = NULL, int num = 0);
    
    /** Read a message from a storage position.
     *
     * Note: init() should be called before this method can be used.
     *
     * @param index  the storage position to read.
     * @param num    the originator address (16 chars including terminator).
     * @param buf    a buffer where to save the short message.
     * @param len    the length of buf.
     * @return       true if successful, false otherwise.
     */
    bool smsRead(int index, char* num, char* buf, int len);
    
    /** Delete a message.
     *
     * Note: init() should be called before this method can be used.
     *
     * @param index the storage position to delete.
     * @return true if successful, false otherwise.
     */
    bool smsDelete(int index);
    
    /** Send a message to a recipient.
     *
     * Note: init() and nwk_registration() should be called before
     * this method can be used.
     *
     * @param num the phone number of the recipient as a null terminated
     *            string.  Note: no spaces are allowed in this string.
     * @param buf the content of the message to sent, null terminated.
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
     * Note: init() should be called before using this method can be
     * used and, in many cases, nwk_registration() is also required as
     * the USSD may need network access.
     *
     * Note: some USSD commands relate to call waiting, call forwarding,
     * etc., which can result in multiple responses.  This function returns
     * only the first response.  Instantiate this class with debugOn set to
     * true to get a better view of what is really going on.  If such
     * responses are important to you, you should subclass this class and
     * parse for them specifically and, probably, use specific AT commands
     * rather than USSD.
     *
     * @param cmd the USSD string to send e.g "*#100#".
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
     * Note: init() should be called before this method can be used.
     *
     * @param filename the name of the file.
     * @return         true if successful, false otherwise.
     */
    bool delFile(const char* filename);
    
    /** Write some data to a file in the module's local file system.
     *
     * Note: init() should be called before this method can be used.
     *
     * @param  filename the name of the file.
     * @param  buf the data to write.
     * @param  len the size of the data to write.
     * @return the number of bytes written.
     */
    int writeFile(const char* filename, const char* buf, int len);
    
    /** Read a file from the module's local file system.
     *
     * Note: init() should be called before this method can be used.
     *
     * @param  filename the name of the file
     * @param  buf a buffer to hold the data
     * @param  len the size to read
     * @return the number of bytes read
    */
    int readFile(const char* filename, char* buf, int len);
    
    /** Retrieve the file size from the module's local file system.
     *
     * Note: init() should be called before this method can be used.
     *
     * @param  filename the name of the file.
     * @return the file size in bytes.
     */
    int fileSize(const char* filename);
    
protected:

    /**********************************************************************
     * PROTECTED: Short Message Service
     **********************************************************************/

    /** The number type for telephone numbers without a + on the front
     */
    #define TYPE_OF_ADDRESS_NATIONAL 129

    /** The number type for telephone numbers with a + on the front
     */
    #define TYPE_OF_ADDRESS_INTERNATIONAL 145

    /** Convert a #define to a string
     */
    #define stringify(a) str(a)
    #define str(a) #a

    /** Place to store the index of an SMS message.
     */
    int *_userSmsIndex;

    /** The number of SMS message being returned to the user.
     */
    int _userSmsNum;

    /** The number of SMS messages stored in the module.
     */
    int _smsCount;

    /** Temporary storage for an SMS message.
     */
    char _smsBuf[SMS_BUFFER_SIZE];

    /** URC for Short Message listing.
     */
    void CMGL_URC();

    /** URC for new SMS messages.
     */
    void CMTI_URC();

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
     * Note: this should be no larger than BUFFERED_SERIAL_RXBUF_SIZE
     * (which defaults to 256) minus some margin for the AT command
     * stuff that precedes it (which includes the filename).  A good
     * number is 192.  If you want to use something bigger then add an
     * entry to the target_overrides section of your mbed_app.json of the
     * following form:
     *
     * "platform.buffered-serial-rxbuf-size": 1024
     */

    #define FILE_BUFFER_SIZE 192
};

#endif // _UBLOX_CELLULAR_DRIVER_GEN_
