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

#include "UbloxCellularInterfaceExt.h"
#include "APN_db.h"
#include "string.h"
#if defined(FEATURE_COMMON_PAL)
#include "mbed_trace.h"
#define TRACE_GROUP "UCID"
#else
#define tr_debug(...) (void(0)) // dummies if feature common pal is not added
#define tr_info(...)  (void(0)) // dummies if feature common pal is not added
#define tr_error(...) (void(0)) // dummies if feature common pal is not added
#endif

/**********************************************************************
 * PROTECTED METHODS: Generic
 **********************************************************************/

/**********************************************************************
 * PROTECTED METHODS: Short Message Service
 **********************************************************************/

// URC for Short Message listing
void UbloxCellularInterfaceExt::CMGL_URC()
{
    int index;

    if ((_smsIndex != NULL) && (_smsNum > 0)) {
        // +CMGL: <ix>,...
        if (_at->recv(": %d,", &index))
        {
            *_smsIndex++ = index;
            _smsNum--;
        }
    }
}

/**********************************************************************
 * PROTECTED  METHODS: Unstructured Supplementary Service Data
 **********************************************************************/

/**********************************************************************
 * PROTECTED: Module File System
 **********************************************************************/

/**********************************************************************
 * PUBLIC METHODS: Generic
 **********************************************************************/

// Constructor.
UbloxCellularInterfaceExt::UbloxCellularInterfaceExt(bool debugOn, PinName tx, PinName rx, int baud):
                           UbloxCellularInterface(debugOn, tx, rx, baud)
{
    _smsIndex = NULL;
    _smsNum = 0;
}

// Destructor.
UbloxCellularInterfaceExt::~UbloxCellularInterfaceExt()
{
    // TODO
}

/**********************************************************************
 * PUBLIC METHODS: Short Message Service
 **********************************************************************/

// Count the number of messages on the module.
int UbloxCellularInterfaceExt::smsList(const char* stat /*= "ALL"*/, int* index /*=NULL*/, int num /*= 0*/)
{
    int numMessages = -1;
    LOCK();

    _smsIndex = index;
    _smsNum = num;
    // Use a callback to capture the result
    _at->oob("+CMGL", callback(this, &UbloxCellularInterfaceExt::CMGL_URC));
    // +CMGL: <ix>,...
    if (_at->send("AT+CMGL=\"%s\"\r\n", stat) && _at->recv("OK")) {
        numMessages = num - _smsNum;
    }

    // Set this back to null so that the URC won't trample
    _smsIndex = NULL;

    UNLOCK();
    return numMessages;
}

// Send an SMS message.
bool UbloxCellularInterfaceExt::smsSend(const char* num, const char* buf)
{
    bool success = false;
    LOCK();

    if (_at->send("AT+CMGS=\"%s\"",num) && _at->recv("@")) {
        if ((_at->write(buf, strlen(buf)) >= (int) strlen(buf)) &&
            (_at->putc(0x1A) == 0x1A) &&  // CTRL-Z
            _at->recv("OK")) {
            success = true;
        }
    }

    UNLOCK();
    return success;
}

bool UbloxCellularInterfaceExt::smsDelete(int index)
{
    bool success;
    LOCK();

    success = _at->send("AT+CMGD=%d", index) && _at->recv("OK");

    UNLOCK();
    return success;
}

bool UbloxCellularInterfaceExt::smsRead(int index, char* num, char* buf, int len)
{
    bool success = false;
    char * tmpBuf;
    char * endOfString;
    int smsReadLength = 0;
    LOCK();

    if (len > 0) {
        tmpBuf = (char *) malloc(SMS_BUFFER_SIZE + 1); // +1 to allow for newline
        if ((tmpBuf != NULL) &&
            _at->send("AT+CMGR=%d", index) &&
            _at->recv("+CMGR: \"%*[^\"]\",\"%[^\"]", num) &&
            _at->recv("%" stringify(SMS_BUFFER_SIZE) "[^\n]\n", tmpBuf)) {
            endOfString = strchr(tmpBuf, '\n');
            if (endOfString != NULL) {
                smsReadLength = endOfString - tmpBuf;
                if (smsReadLength + 1 > len) { // +1 for terminator
                    smsReadLength = len - 1;
                }
                memcpy(buf, tmpBuf, smsReadLength);
                *(buf + smsReadLength)  = '\0'; // Add terminator
                success = true;
            }
        }
        free (tmpBuf);
    }

    UNLOCK();
    return success;
}

/**********************************************************************
 * PUBLIC  METHODS: Unstructured Supplementary Service Data
 **********************************************************************/

// Perform a USSD command.
bool UbloxCellularInterfaceExt::ussdCommand(const char* cmd, char* buf)
{
    bool success;
    LOCK();

    *buf = '\0';
    // +USD: \"%*[^\"]\",\"%[^\"]\",,\"%*[^\"]\",%d,%d,%d,%d,\"*[^\"]\",%d,%d"..);
    success = _at->send("AT+CUSD=1,\"%s\"", cmd) &&
              _at->recv("+CUSD: %*d,\"%[^\"]\",%*d", buf);

    UNLOCK();
    return success;
}

/**********************************************************************
 * PUBLIC: Module File System
 **********************************************************************/

// Delete a file from the module's file system.
bool UbloxCellularInterfaceExt::delFile(const char* filename)
{
    bool success;
    LOCK();

    success = _at->send("AT+UDELFILE=\"%s\"", filename) && _at->recv("OK");

    UNLOCK();
    return success;
}

// Write a buffer of data to a file in the module's file system.
int UbloxCellularInterfaceExt::writeFile(const char* filename, const char* buf, int len)
{
    int bytesWritten = -1;
    LOCK();

    if (_at->send("AT+UDWNFILE=\"%s\",%d", filename, len) && _at->recv("@")) {
        if ((_at->write(buf, len) >= len) && _at->recv("OK")) {
            bytesWritten = len;
        }
    }

    UNLOCK();
    return bytesWritten;
}

int UbloxCellularInterfaceExt::readFile(const char* filename, char* buf, int len)
{
    int bytesRead = -1;
    char respFilename[48];
    char * tmpBuf = NULL;
    int sz;
    LOCK();

    tmpBuf = (char *) malloc(len + 2); // +2 to allow for quotes
    if ((tmpBuf != NULL) &&
        _at->send("AT+URDFILE=\"%s\"", filename) &&
        _at->recv("+URDFILE: \"%[^\"]\",%d,", respFilename, &sz) &&
        (strcmp(filename, respFilename) == 0)) {
        if (sz > 0) {
            if (sz > len) {
                sz = len;
            }
            if ((_at->read(tmpBuf, sz + 2) == sz + 2) && // +2 for quotes
                 _at->recv("OK") &&
                (tmpBuf[0] == '\"') && (tmpBuf[sz - 1] == '\"')) {
                memcpy(buf, tmpBuf + 1, sz);
                bytesRead = sz;
            }
        } else {
            bytesRead = 0;
        }
    }

    UNLOCK();
    return bytesRead;
}

//The following function is useful for reading files with a dimension greater than 128 bytes
int UbloxCellularInterfaceExt::readFileNew(const char* filename, char* buf, int len)
{
    int countBytes = -1;  // Counter for file reading (default value)
    int bytesToRead = fileSize(filename);  // Retrieve the size of the file
    int offset = 0;
    int blockSize = 128;
    char respFilename[48];
    char * tmpBuf = NULL;
    int sz;
    bool success = true;

    tr_debug("readFileNew: filename is %s; size is %d", filename, bytesToRead);

    if (bytesToRead > 0)
    {
        if (bytesToRead > len) {
            bytesToRead = len;
        }

        tmpBuf = (char *) malloc(blockSize + 2); // +2 to allow for quotes
        if (tmpBuf != NULL) {
            while (success && (bytesToRead > 0)) {

                if (bytesToRead < blockSize) {
                    blockSize = bytesToRead;
                }
                LOCK();

                if (blockSize > 0) {
                    if (_at->send("+URDBLOCK=\"%s\",%d,%d\r\n", filename, offset, blockSize) &&
                        _at->recv("+URDBLOCK: \"%[^\"]\",%d,", respFilename, &sz)) {
                        if ((_at->read(tmpBuf, sz + 2) >= sz + 2) && // +2 for quotes
                            (tmpBuf[0] == '\"') && (tmpBuf[sz + 1] == '\"')) {
                            memcpy (buf, tmpBuf + 1, sz);
                            bytesToRead -= sz;
                            offset += sz;
                            buf += sz;
                        } else {
                            success = false;
                        }
                   } else {
                       success = false;
                   }
                }

                UNLOCK();
            }
        }

        free(tmpBuf);

        if (success) {
            countBytes = offset;
        }
    }

    return countBytes;
}

int UbloxCellularInterfaceExt::fileSize(const char* filename)
{
    int returnValue = -1;
    int fileSize;
    LOCK();

    if (_at->send("+ULSTFILE=2,\"%s\"", filename) &&
        _at->recv("+ULSTFILE: %d", &fileSize)) {
        returnValue = fileSize;
    }

    UNLOCK();
    return returnValue;
}

// End of file
