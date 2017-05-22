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

#include "UbloxCellularDriverGen.h"
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
 * PROTECTED METHODS: Short Message Service
 **********************************************************************/

// URC for Short Message listing.
void UbloxCellularDriverGen::CMGL_URC()
{
    char buf[32];
    int index;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +CMGL: <ix>,...
    if (read_at_to_char(buf, sizeof(buf), '\n') > 0) {
        if (sscanf(buf, ": %d,", &index) == 1) {
            _smsCount++;
            if ((_userSmsIndex != NULL) && (_userSmsNum > 0)) {
                *_userSmsIndex = index;
                _userSmsIndex++;
                _userSmsNum--;
            }
        }
    }
}

// URC for new class 0 SMS messages.
void UbloxCellularDriverGen::CMTI_URC()
{
    char buf[32];

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        // No need to parse, any content is good
        tr_debug("New SMS received");
    }
}

// URC for non-class 0 SMS messages.
void UbloxCellularDriverGen::CMT_URC()
{
    char buf[128];
    char text[50];
    char serviceTimestamp[15];

    // Our CMGF = 1, i.e., text mode. So we expect response in this format:
    //
    // +CMT: <oa>,[<alpha>],<scts>[,<tooa>,
    // <fo>,<pid>,<dcs>,<sca>,<tosca>,
    // <length>]<CR><LF><data>
    //
    // By default detailed SMS header CSDH=0 , so we are not expecting  [,<tooa>,
    // <fo>,<pid>,<dcs>,<sca>,<tosca>
    // AT Command Manual UBX-13002752, section 11.8.2

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    if (read_at_to_char(buf, sizeof (buf), '\n') > 0) {
        if (sscanf(buf, ": \"%49[^\"]\",,%14[^\"]\"", text, serviceTimestamp) == 2) {
            tr_debug("SMS: %s, %s", serviceTimestamp, text);
        }
    }
}

/**********************************************************************
 * PROTECTED METHODS: Unstructured Supplementary Service Data
 **********************************************************************/

// URC for call waiting.
void UbloxCellularDriverGen::CCWA_URC()
{
    char buf[32];
    int numChars;
    int a;
    int b = 0;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +CCWA: <status>[, <class>]
    numChars = read_at_to_char(buf, sizeof (buf), '\n');
    if (numChars > 0) {
        if (sscanf(buf, ": %d, %d", &a, &b) > 0) {
            if (_ssUrcBuf == NULL) {
                _ssUrcBuf = (char *) malloc(numChars + 5 + 1);
                if (_ssUrcBuf != NULL) {
                    memcpy (_ssUrcBuf, "+CCWA", 5);
                    memcpy (_ssUrcBuf + 5, buf, numChars);
                    *(_ssUrcBuf + numChars + 5) = 0;
                    if (a > 0) {
                        tr_debug("Calling Waiting is active");
                    } else {
                        tr_debug("Calling Waiting is not active");
                    }
                    if (b > 0) {
                        if (b & 0x01) {
                            tr_debug(" for voice");
                        }
                        if (b & 0x02) {
                            tr_debug(" for data");
                        }
                        if (b & 0x04) {
                            tr_debug(" for fax");
                        }
                        if (b & 0x08) {
                            tr_debug(" for SMS");
                        }
                        if (b & 0x10) {
                            tr_debug(" for data circuit sync");
                        }
                        if (b & 0x20) {
                            tr_debug(" for data circuit async");
                        }
                        if (b & 0x40) {
                            tr_debug(" for dedicated packet access");
                        }
                        if (b & 0x80) {
                            tr_debug(" for dedicated PAD access");
                        }
                    }
                }
            }
        }
    }
}

// URC for call forwarding.
void UbloxCellularDriverGen::CCFC_URC()
{
    char buf[32];
    int numChars;
    char num[32];
    int a, b;
    int numValues;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +CCFC: <status>[, <class>]
    numChars = read_at_to_char(buf, sizeof (buf), '\n');
    if (numChars > 0) {
        memset (num, 0, sizeof (num));
        numValues = sscanf(buf, ": %d,%d,\"%32[^\"][\"]", &a, &b, num);
        if (numValues > 0) {
            if (_ssUrcBuf == NULL) {
                _ssUrcBuf = (char *) malloc(numChars + 5 + 1);
                if (_ssUrcBuf != NULL) {
                    memcpy (_ssUrcBuf, "+CCFC", 5);
                    memcpy (_ssUrcBuf + 5, buf, numChars);
                    *(_ssUrcBuf + numChars + 5) = 0;
                    if (a > 0) {
                        tr_debug("Calling Forwarding is active");
                    } else {
                        tr_debug("Calling Forwarding is not active");
                    }
                    if (numValues > 1) {
                        if (b > 0) {
                            if (b & 0x01) {
                                tr_debug(" for voice");
                            }
                            if (b & 0x02) {
                                tr_debug(" for data");
                            }
                            if (b & 0x04) {
                                tr_debug(" for fax");
                            }
                            if (b & 0x08) {
                                tr_debug(" for SMS");
                            }
                            if (b & 0x10) {
                                tr_debug(" for data circuit sync");
                            }
                            if (b & 0x20) {
                                tr_debug(" for data circuit async");
                            }
                            if (b & 0x40) {
                                tr_debug(" for dedicated packet access");
                            }
                            if (b & 0x80) {
                                tr_debug(" for dedicated PAD access");
                            }
                        }
                    }
                    if (numValues > 2) {
                        tr_debug(" for %s", num);
                    }
                }
            }
        }
    }
}


// URC for calling line ID restriction.
void UbloxCellularDriverGen::CLIR_URC()
{
    char buf[32];
    int numChars;
    int a, b;
    int numValues;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +CLIR: <n>[, <m>]
    numChars = read_at_to_char(buf, sizeof (buf), '\n');
    if (numChars > 0) {
        numValues = sscanf(buf, ": %d,%d", &a, &b);
        if (numValues > 0) {
            if (_ssUrcBuf == NULL) {
                _ssUrcBuf = (char *) malloc(numChars + 5 + 1);
                if (_ssUrcBuf != NULL) {
                    memcpy (_ssUrcBuf, "+CLIR", 5);
                    memcpy (_ssUrcBuf + 5, buf, numChars);
                    *(_ssUrcBuf + numChars + 5) = 0;
                    switch (a) {
                        case 0:
                            tr_debug("Calling Line ID restriction is as subscribed");
                            break;
                        case 1:
                            tr_debug("Calling Line ID invocation");
                            break;
                        case 2:
                            tr_debug("Calling Line ID suppression");
                            break;
                    }
                    if (numValues > 2) {
                        switch (b) {
                            case 0:
                                tr_debug(" is not provisioned");
                                break;
                            case 1:
                                tr_debug(" is provisioned permanently");
                                break;
                            case 2:
                                tr_debug(" is unknown");
                                break;
                            case 3:
                                tr_debug(" is in temporary mode, presentation restricted");
                                break;
                            case 4:
                                tr_debug(" is in temporary mode, presentation allowed");
                                break;
                        }
                    }
                }
            }
        }
    }
}

// URC for calling line ID presentation.
void UbloxCellularDriverGen::CLIP_URC()
{
    char buf[32];
    int numChars;
    int a, b;
    int numValues;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +CLIP: <n>[, <m>]
    numChars = read_at_to_char(buf, sizeof (buf), '\n');
    if (numChars > 0) {
        numValues = sscanf(buf, ": %d,%d", &a, &b);
        if (numValues > 0) {
            if (_ssUrcBuf == NULL) {
                _ssUrcBuf = (char *) malloc(numChars + 5 + 1);
                if (_ssUrcBuf != NULL) {
                    memcpy (_ssUrcBuf, "+CLIP", 5);
                    memcpy (_ssUrcBuf + 5, buf, numChars);
                    *(_ssUrcBuf + numChars + 5) = 0;
                    switch (a) {
                        case 0:
                            tr_debug("Calling Line ID disable");
                            break;
                        case 1:
                            tr_debug("Calling Line ID enable");
                            break;
                    }
                    if (numValues > 1) {
                        switch (b) {
                            case 0:
                                tr_debug(" is not provisioned");
                                break;
                            case 1:
                                tr_debug(" is provisioned");
                                break;
                            case 2:
                                tr_debug(" is unknown");
                                break;
                        }
                    }
                }
            }
        }
    }
}

// URC for connected line ID presentation.
void UbloxCellularDriverGen::COLP_URC()
{
    char buf[32];
    int numChars;
    int a, b;
    int numValues;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +COLP: <n>[, <m>]
    numChars = read_at_to_char(buf, sizeof (buf), '\n');
    if (numChars > 0) {
        numValues = sscanf(buf, ": %d,%d", &a, &b);
        if (numValues > 0) {
            if (_ssUrcBuf == NULL) {
                _ssUrcBuf = (char *) malloc(numChars + 5 + 1);
                if (_ssUrcBuf != NULL) {
                    memcpy (_ssUrcBuf, "+COLP", 5);
                    memcpy (_ssUrcBuf + 5, buf, numChars);
                    *(_ssUrcBuf + numChars + 5) = 0;
                    switch (a) {
                        case 0:
                            tr_debug("Connected Line ID disable");
                            break;
                        case 1:
                            tr_debug("Connected Line ID enable");
                            break;
                    }
                    if (numValues > 1) {
                        switch (b) {
                            case 0:
                                tr_debug(" is not provisioned");
                                break;
                            case 1:
                                tr_debug(" is provisioned");
                                break;
                            case 2:
                                tr_debug(" is unknown");
                                break;
                        }
                    }
                }
            }
        }
    }
}

// URC for connected line ID restriction.
void UbloxCellularDriverGen::COLR_URC()
{
    char buf[32];
    int numChars;
    int a;

    // Note: not calling _at->recv() from here as we're
    // already in an _at->recv()
    // +COLR: <status>
    numChars = read_at_to_char(buf, sizeof (buf), '\n');
    if (numChars > 0) {
        if (sscanf(buf, ": %d", &a) > 0) {
            if (_ssUrcBuf == NULL) {
                _ssUrcBuf = (char *) malloc(numChars + 5 + 1);
                if (_ssUrcBuf != NULL) {
                    memcpy (_ssUrcBuf, "+COLR", 5);
                    memcpy (_ssUrcBuf + 5, buf, numChars);
                    *(_ssUrcBuf + numChars + 5) = 0;
                    switch (a) {
                        case 0:
                            tr_debug("Connected Line ID restriction is not provisioned");
                            break;
                        case 1:
                            tr_debug("Connected Line ID restriction is provisioned");
                            break;
                        case 2:
                            tr_debug("Connected Line ID restriction is unknown");
                            break;
                    }
                }
            }
        }
    }
}

/**********************************************************************
 * PUBLIC METHODS: Generic
 **********************************************************************/

// Constructor.
UbloxCellularDriverGen::UbloxCellularDriverGen(PinName tx, PinName rx,
                                               int baud, bool debug_on)
{
    _userSmsIndex = NULL;
    _userSmsNum = 0;
    _smsCount = 0;
    _ssUrcBuf = NULL;

    // Initialise the base class, which starts the AT parser
    baseClassInit(tx, rx, baud, debug_on);

    // URCs related to SMS
    _at->oob("+CMGL", callback(this, &UbloxCellularDriverGen::CMGL_URC));
    _at->oob("+CMT", callback(this, &UbloxCellularDriverGen::CMT_URC));
    _at->oob("+CMTI", callback(this, &UbloxCellularDriverGen::CMTI_URC));

    // URCs relater to supplementary services
    _at->oob("+CCWA", callback(this, &UbloxCellularDriverGen::CCWA_URC));
    _at->oob("+CCFC", callback(this, &UbloxCellularDriverGen::CCFC_URC));
    _at->oob("+CLIR", callback(this, &UbloxCellularDriverGen::CLIR_URC));
    _at->oob("+CLIP", callback(this, &UbloxCellularDriverGen::CLIP_URC));
    _at->oob("+COLP", callback(this, &UbloxCellularDriverGen::COLP_URC));
    _at->oob("+COLR", callback(this, &UbloxCellularDriverGen::COLR_URC));
}

// Destructor.
UbloxCellularDriverGen::~UbloxCellularDriverGen()
{
    // TODO
}

/**********************************************************************
 * PUBLIC METHODS: Short Message Service
 **********************************************************************/

// Count the number of messages on the module.
int UbloxCellularDriverGen::smsList(const char* stat, int* index, int num)
{
    int numMessages = -1;
    LOCK();

    _userSmsIndex = index;
    _userSmsNum = num;
    _smsCount = 0;
    // There is a callback to capture the result
    // +CMGL: <ix>,...
    if (_at->send("AT+CMGL=\"%s\"", stat) && _at->recv("OK")) {
        numMessages = _smsCount;
    }

    // Set this back to null so that the URC won't trample
    _userSmsIndex = NULL;

    UNLOCK();
    return numMessages;
}

// Send an SMS message.
bool UbloxCellularDriverGen::smsSend(const char* num, const char* buf)
{
    bool success = false;
    char typeOfAddress = TYPE_OF_ADDRESS_NATIONAL;
    LOCK();

    if ((strlen (num) > 0) && (*(num) == '+')) {
        typeOfAddress = TYPE_OF_ADDRESS_INTERNATIONAL;
    }
    if (_at->send("AT+CMGS=\"%s\",%d", num, typeOfAddress) && _at->recv(">")) {
        if ((_at->write(buf, (int) strlen(buf)) >= (int) strlen(buf)) &&
            (_at->putc(0x1A) == 0) &&  // CTRL-Z
            _at->recv("OK")) {
            success = true;
        }
    }

    UNLOCK();
    return success;
}

bool UbloxCellularDriverGen::smsDelete(int index)
{
    bool success;
    LOCK();

    success = _at->send("AT+CMGD=%d", index) && _at->recv("OK");

    UNLOCK();
    return success;
}

bool UbloxCellularDriverGen::smsRead(int index, char* num, char* buf, int len)
{
    bool success = false;
    char * tmpBuf;
    char * endOfString;
    int smsReadLength = 0;
    LOCK();

    if (len > 0) {
        tmpBuf = (char *) malloc(SMS_BUFFER_SIZE);
        //+CMGR: "REC READ", "+393488535999",,"07/04/05,18:02:28+08",145,4,0,0,"+393492000466",145,93
        // The text of the message.
        // OK
        if (tmpBuf != NULL) {
            memset (tmpBuf, 0, sizeof (SMS_BUFFER_SIZE)); // Ensure terminator
            if (_at->send("AT+CMGR=%d", index) &&
                _at->recv("+CMGR: \"%*[^\"]\",\"%15[^\"]\"%*[^\n]\n", num) &&
                _at->recv("%" stringify(SMS_BUFFER_SIZE) "[^\n]\n", tmpBuf) &&
                _at->recv("OK")) {
                endOfString = strchr(tmpBuf, 0);
                if (endOfString != NULL) {
                    smsReadLength = endOfString - tmpBuf;
                    if (smsReadLength + 1 > len) { // +1 for terminator
                        smsReadLength = len - 1;
                    }
                    memcpy(buf, tmpBuf, smsReadLength);
                    *(buf + smsReadLength) = 0; // Add terminator
                    success = true;
                }
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
bool UbloxCellularDriverGen::ussdCommand(const char* cmd, char* buf, int len)
{
    bool success = false;
    char * tmpBuf;
    int atTimeout = _at_timeout;
    int x;
    Timer timer;
    LOCK();

    if (len > 0) {
        *buf = 0;
        if (len > USSD_STRING_LENGTH + 1) {
            len = USSD_STRING_LENGTH + 1;
        }

        tmpBuf = (char *) malloc(USSD_STRING_LENGTH + 1);

        if (tmpBuf != NULL) {
            memset (tmpBuf, 0, USSD_STRING_LENGTH + 1);
            // +CUSD: \"%*d, \"%128[^\"]\",%*d"
            if (_at->send("AT+CUSD=1,\"%s\"", cmd)) {
                // Wait for either +CUSD to come back or
                // one of the other SS related URCs to trigger
                if (_ssUrcBuf != NULL) {
                    free (_ssUrcBuf);
                    _ssUrcBuf = NULL;
                }
                timer.start();
                _at->set_timeout(1000);
                while (!success && (timer.read_ms() < atTimeout)) {
                    if (_at->recv("+CUSD: %*d,\"")) {
                        // Note: don't wait for "OK" here as the +CUSD response may come
                        // before or after the OK
                        // Also, the return string may include newlines so can't just use
                        // recv() to capture it as recv() will stop capturing at a newline.
                        if (read_at_to_char(tmpBuf, USSD_STRING_LENGTH, '\"') > 0) {
                            success = true;
                            memcpy (buf, tmpBuf, len);
                            *(buf + len - 1) = 0;
                        }
                    } else {
                        // Some of the return values do not appear as +CUSD but
                        // instead as the relevant URC for call waiting, call forwarding,
                        // etc.  Test those here.
                        if (_ssUrcBuf != NULL) {
                            success = true;
                            x = strlen (_ssUrcBuf);
                            if (x > len - 1 ) {
                                x = len - 1;
                            }
                            memcpy (buf, _ssUrcBuf, x);
                            *(buf + x) = 0;
                            free (_ssUrcBuf);
                            _ssUrcBuf = NULL;
                        }
                    }
                }
                at_set_timeout(atTimeout);
                timer.stop();
            }
        }
    }

    UNLOCK();
    return success;
}

/**********************************************************************
 * PUBLIC: Module File System
 **********************************************************************/

// Delete a file from the module's file system.
bool UbloxCellularDriverGen::delFile(const char* filename)
{
    bool success;
    LOCK();

    success = _at->send("AT+UDELFILE=\"%s\"", filename) && _at->recv("OK");

    UNLOCK();
    return success;
}

// Write a buffer of data to a file in the module's file system.
int UbloxCellularDriverGen::writeFile(const char* filename, const char* buf, int len)
{
    int bytesWritten = -1;
    LOCK();

    if (_at->send("AT+UDWNFILE=\"%s\",%d", filename, len) && _at->recv(">")) {
        if ((_at->write(buf, len) >= len) && _at->recv("OK")) {
            bytesWritten = len;
        }
    }

    UNLOCK();
    return bytesWritten;
}

// Read a file from the module's file system
int UbloxCellularDriverGen::readFile(const char* filename, char* buf, int len)
{
    int countBytes = -1;  // Counter for file reading (default value)
    int bytesToRead = fileSize(filename);  // Retrieve the size of the file
    int offset = 0;
    int blockSize = 128;
    char respFilename[48];
    char * tmpBuf = NULL;
    int sz;
    bool success = true;

    tr_debug("readFile: filename is %s; size is %d", filename, bytesToRead);

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
                    if (_at->send("AT+URDBLOCK=\"%s\",%d,%d\r\n", filename, offset, blockSize) &&
                        _at->recv("+URDBLOCK: \"%[^\"]\",%d,", respFilename, &sz)) {
                        if ((_at->read(tmpBuf, sz + 2) >= sz + 2) && // +2 for quotes
                            (tmpBuf[0] == '\"') && (tmpBuf[sz + 1] == '\"')) {
                            memcpy (buf, tmpBuf + 1, sz);
                            bytesToRead -= sz;
                            offset += sz;
                            buf += sz;
                            _at->recv("OK");
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

// Return the size of a file.
int UbloxCellularDriverGen::fileSize(const char* filename)
{
    int returnValue = -1;
    int fileSize;
    LOCK();

    if (_at->send("AT+ULSTFILE=2,\"%s\"", filename) &&
        _at->recv("+ULSTFILE: %d\n", &fileSize) &&
        _at->recv("OK")) {
        returnValue = fileSize;
    }

    UNLOCK();
    return returnValue;
}

// End of file
