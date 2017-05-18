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
     * @param stat  what type of messages you can use:
     *              "REC UNREAD", "REC READ", "STO UNSENT", "STO SENT", "ALL".
     * @param index list where to save the storage positions.
     * @param num   number of elements in the list.
     * @return      the number of messages, this can be bigger than num, -1 on failure.
     */
    int smsList(const char* stat = "ALL", int* index = NULL, int num = 0);
    
    /** Read a message from a storage position.
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
     * @param index the storage position to delete.
     * @return true if successful, false otherwise.
     */
    bool smsDelete(int index);
    
    /** Send a message to a recipient.
     *
     * @param num the phone number of the recipient.
     * @param buf the content of the message to sent.
     * @return    true if successful, false otherwise.
     */
    bool smsSend(const char* num, const char* buf);
    
    /**********************************************************************
     * PUBLIC: Unstructured Supplementary Service Data
     **********************************************************************/
    
    /** The size of a USSD storage buffer.
     */
    #define USSD_BUFFER_SIZE 128

    /** Send a message.
     *
     * @param cmd the ussd command to send e.g "*#06#".
     * @param buf a buffer where to save the reply, must be at least
     *            USSD_BUFFER_SIZE in size.
     * @return    true if successful, false otherwise.
     */
    bool ussdCommand(const char* cmd, char* buf);
    
    /**********************************************************************
     * PUBLIC: Module File System
     **********************************************************************/
    
    /** Delete a file from the module's local file system.  An attempt
     * to delete a non-existent file will fail.
     *
     * @param filename the name of the file.
     * @return         true if successful, false otherwise.
     */
    bool delFile(const char* filename);
    
    /** Write some data to a file in the module's local file system.
     *
     * @param  filename the name of the file.
     * @param  buf the data to write.
     * @param  len the size of the data to write.
     * @return the number of bytes written.
     */
    int writeFile(const char* filename, const char* buf, int len);
    
    /** Read a short file, one which can be read in a single line
     * of 128 characters, from the module's local file system.
     *
     * @param  filename the name of the file.
     * @param  buf a buffer to hold the data.
     * @param  len the size to read.
     * @return the number of bytes read.
     */
    int readFile(const char* filename, char* buf, int len);
    
    /** Read a file from the module's local file system that might
     * have contents longer than 128 charaxters.
     *
     * @param  filename the name of the file
     * @param  buf a buffer to hold the data
     * @param  len the size to read
     * @return the number of bytes read
    */
    int readFileNew(const char* filename, char* buf, int len);
    
    /** Retrieve the file size from the module's local file system.
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
     * PROTECTED: File System
     **********************************************************************/

    /** The maximum size of a single read of file data.
     */
    #define FILE_BUFFER_SIZE 128
};

#endif // _UBLOX_CELLULAR_DRIVER_GEN_
