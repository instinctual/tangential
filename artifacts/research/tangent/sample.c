/*
 *  MockApplication
 *
 *  Test mock IPC application.
 *
 *  Created by Andrew Duguid on 31/03/2011.
 *  Copyright 2011 Tangent Wave Ltd. All rights reserved.
 *  SVN: $Revision: 9 $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// in the mock app we need to differentiate between build platforms for headers and data
#ifdef TMH_PLATFORM_WINDOWS

// windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <direct.h>

#else

// osx and linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>

// specific width integer types without suffix from stdint.h
typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint32_t    bool32;

#endif

// support defs
#define true                        1
#define false                       0

// port for socket transport
#define IPC_SOCKET_PORT_NUMBER		64246

// master definition of the revision number supported by this software version
#define IPC_PROTOCOL_REVISION       6

// message size constants
#define IPC_MESSAGE_HEADER_LEN      4
#define IPC_MESSAGE_DATA_MAX        1024

#define APP_NAME                    "MockAppName"

// definitions for ipc commands from the hub to applications
#define IPC_HUB_COMMAND_INITIATE_COMMS                          0x01
#define IPC_HUB_COMMAND_PARAMETER_CHANGE                        0x02
#define IPC_HUB_COMMAND_PARAMETER_RESET                         0x03
#define IPC_HUB_COMMAND_PARAMETER_VALUE_REQUEST                 0x04
#define IPC_HUB_COMMAND_MENU_CHANGE                             0x05
#define IPC_HUB_COMMAND_MENU_RESET                              0x06
#define IPC_HUB_COMMAND_MENU_STRING_REQUEST                     0x07
#define IPC_HUB_COMMAND_ACTION_ON                               0x08
#define IPC_HUB_COMMAND_MODE_CHANGE                             0x09
#define IPC_HUB_COMMAND_TRANSPORT                               0x0A
#define IPC_HUB_COMMAND_ACTION_OFF                              0x0B
#define IPC_HUB_COMMAND_UNMANAGED_PANEL_CAPABILITIES            0x30
#define IPC_HUB_COMMAND_UNMANAGED_BUTTON_DOWN                   0x31
#define IPC_HUB_COMMAND_UNMANAGED_BUTTON_UP                     0x32
#define IPC_HUB_COMMAND_UNMANAGED_ENCODER_CHANGE                0x33
#define IPC_HUB_COMMAND_UNMANAGED_DISPLAY_REFRESH               0x34
#define IPC_HUB_COMMAND_PANEL_CONNECTION_STATE                  0x35
#define IPC_HUB_COMMAND_CUSTOM_PARAMETER_CHANGE                 0x36
#define IPC_HUB_COMMAND_CUSTOM_PARAMETER_RESET                  0x37
#define IPC_HUB_COMMAND_CUSTOM_PARAMETER_VALUE_REQUEST          0x38
#define IPC_HUB_COMMAND_CUSTOM_MENU_CHANGE                      0x39
#define IPC_HUB_COMMAND_CUSTOM_MENU_RESET                       0x3A
#define IPC_HUB_COMMAND_CUSTOM_MENU_STRING_REQUEST              0x3B
#define IPC_HUB_COMMAND_CUSTOM_ACTION_ON                        0x3C
#define IPC_HUB_COMMAND_CUSTOM_ACTION_OFF                       0x3D
#define IPC_HUB_COMMAND_TRAIN_CONTROL                           0x3E

// definitions for ipc commands from applications to the hub
#define IPC_APP_COMMAND_APPLICATION_DEFINITION                  0x81
#define IPC_APP_COMMAND_PARAMETER_VALUE                         0x82
#define IPC_APP_COMMAND_MENU_STRING                             0x83
#define IPC_APP_COMMAND_ALL_CHANGE                              0x84
#define IPC_APP_COMMAND_MODE_VALUE                              0x85
#define IPC_APP_COMMAND_DISPLAY_TEXT                            0x86
#define IPC_APP_COMMAND_ARC_DISPLAY_TEXT                        0x87
#define IPC_APP_COMMAND_UNMANAGED_PANEL_CAPABILITIES_REQUEST    0xA0
#define IPC_APP_COMMAND_UNMANAGED_DISPLAY_WRITE                 0xA1
#define IPC_APP_COMMAND_RENAME_CONTROL                          0xA2
#define IPC_APP_COMMAND_HIGHLIGHT_CONTROL                       0xA3
#define IPC_APP_COMMAND_INDICATE_CONTROL                        0xA4
#define IPC_APP_COMMAND_REQUEST_PANEL_CONNECTION_STATES         0xA5
#define IPC_APP_COMMAND_CUSTOM_PARAMETER_VALUE                  0xA6
#define IPC_APP_COMMAND_CUSTOM_MENU_STRING                      0xA7
#define IPC_APP_COMMAND_RENAME_CUSTOM_CONTROL                   0xA8
#define IPC_APP_COMMAND_HIGHLIGHT_CUSTOM_CONTROL                0xA9
#define IPC_APP_COMMAND_INDICATE_CUSTOM_CONTROL                 0xAA
#define IPC_APP_COMMAND_REQUEST_FOCUS                           0xAB
#define IPC_APP_COMMAND_RELEASE_FOCUS                           0xAC
#define IPC_APP_COMMAND_TRAIN_RESPONSE                          0xB0

// max length for any of the control string idenitifers used for custom control processing
#define IPC_CUSTOM_CONTROL_STRING_MAX                           128

// special case value for undefined control id
#define CS_CONTROL_ID_UNDEFINED                                 0xFFFFFFFFU

#if defined(TMH_PLATFORM_WINDOWS)

// windows
#define SYS_DIR			"\\sys"
#define USR_DIR			"\\usr"
typedef SOCKET Socket;
#define ERR_NUMBER      WSAGetLastError()
typedef unsigned int    uint32;
typedef int             int32;
typedef unsigned char   uint8;
typedef unsigned int    bool32;
#define snprintf        _snprintf
#define getcwd          _getcwd
#define EXE_NAME        "MockApp.exe"

#elif defined(TMH_PLATFORM_OSX)

// os x
#define SYS_DIR			"/sys"
#define USR_DIR			"/usr"
#define INVALID_SOCKET  (-1)
typedef int Socket;
#define ERR_NUMBER      errno
#define EXE_NAME        "Terminal"

#elif defined(TMH_PLATFORM_LINUX)

// linux
#define SYS_DIR			"/sys"
#define USR_DIR			"/usr"
#define INVALID_SOCKET  (-1)
typedef int Socket;
#define ERR_NUMBER      errno
#define EXE_NAME        "MockApp"

#endif

// file statics
static Socket		_socketHandle;
static uint32       _commsThreadToExit = false;
static uint8  		_readBuffer[IPC_MESSAGE_HEADER_LEN + IPC_MESSAGE_DATA_MAX];
static uint32  		_readBufferLen = 0;
static uint32  		_readBytesRemaining = 0;
static uint8  		_writeBuffer[IPC_MESSAGE_HEADER_LEN + IPC_MESSAGE_DATA_MAX];
static uint32  		_writeBufferLen = 0;
static uint32       _timecodeFrameCount = 0;
static uint32       _protocolRevision = 0;
static uint32       _unmanagedMode = false;
static uint32       _trainingControlID = CS_CONTROL_ID_UNDEFINED;
static uint32       _trainingDelay = 0;

// extracts incoming commands from the read buffer and responds appropriately
static void ParseReceived();

// fetches the current parameter value and at default settings for the given id
static void FetchParameterValue(uint32 parameterID, float *pValueRtn, uint32 *pAtDefaultRtn)
{
    // return a random value to show update
    *pValueRtn = (float) rand() / RAND_MAX;
    *pAtDefaultRtn = false;
}

// changes the parameter value by the supplied amount
static void ModifyParameterValue(uint32 parameterID, float delta)
{
    // change parameter value
}

// resets the parameter
static void ResetParameter(uint32 parameterID)
{
    // reset
}

// fetches the current menu value string and at default setting for the given id
static void FetchMenuValue(uint32 menuID, char **pStringRtn, uint32 *pAtDefaultRtn)
{
    static char buffer[32 + 1];
    
    // return a changing value to show update
    snprintf(buffer, 32, "Menu %c", (char) ((rand() % 26) + 65));
    *pStringRtn = buffer;
    *pAtDefaultRtn = true;
}

// changes the menu value string for the correct step direction
static void ModifyMenuValue(uint32 menuID, int32 stepDir)
{
    // change menu value
}

// resets the menu
static void ResetMenu(uint32 menuID)
{
    // reset
}

// reads a 32 bit signed from the byte buffer returning the address of the next byte after the read value
static uint8 *ReadInt32(uint8 *pByteBuffer, int32 *pValue)
{
	// build up the 32 bit value from the first four bytes in the buffer
	*pValue = *pByteBuffer++ << 24;
	*pValue |= *pByteBuffer++ << 16;
	*pValue |= *pByteBuffer++ << 8;
	*pValue |= *pByteBuffer++;
    
    // return the address of the following byte
    return pByteBuffer;
}

// writes a 32 bit signed to the byte buffer returning the address of the next byte after the written value
static uint8 *WriteInt32(uint8 *pByteBuffer, int32 value)
{
	// store the appropriate bytes of the 32 bit value into the first four bytes of the buffer
	*pByteBuffer++ = (value >> 24) & 0xFF;
	*pByteBuffer++ = (value >> 16) & 0xFF;
	*pByteBuffer++ = (value >> 8) & 0xFF;
	*pByteBuffer++ = value & 0xFF;
    
	// return the address of the following byte
	return pByteBuffer;
}

// reads a 32 bit unsigned from the byte buffer returning the address of the next byte after the read value
static uint8 *ReadUInt32(uint8 *pByteBuffer, uint32 *pValue)
{
	// build up the 32 bit value from the first four bytes in the buffer
	*pValue = *pByteBuffer++ << 24;
	*pValue |= *pByteBuffer++ << 16;
	*pValue |= *pByteBuffer++ << 8;
	*pValue |= *pByteBuffer++;
    
    // return the address of the following byte
    return pByteBuffer;
}

// writes a 32 bit unsigned to the byte buffer returning the address of the next byte after the written value
static uint8 *WriteUInt32(uint8 *pByteBuffer, uint32 value)
{
	// store the appropriate bytes of the 32 bit value into the first four bytes of the buffer
	*pByteBuffer++ = (value >> 24) & 0xFF;
	*pByteBuffer++ = (value >> 16) & 0xFF;
	*pByteBuffer++ = (value >> 8) & 0xFF;
	*pByteBuffer++ = value & 0xFF;
    
	// return the address of the following byte
	return pByteBuffer;
}

// writes an 8 bit unsigned to the byte buffer returning the address of the next byte after the written value
static uint8 *WriteUInt8(uint8 *pByteBuffer, uint8 value)
{
	// store the appropriate byte
	*pByteBuffer++ = value;

	// return the address of the following byte
	return pByteBuffer;
}

// extracts a 32 bit float value that is stored in the byte buffer, returning the address of the byte in the
// buffer following the read value
uint8 *ReadFloat(uint8 *pByteBuffer, float *pValue)
{
    uint8 *pFloatBytes = (uint8 *) pValue;
    
	// build up the 32 bit value from the first four bytes in the buffer, preserving the byte order from earlier code
	*(pFloatBytes + 3) = *pByteBuffer++;
	*(pFloatBytes + 2) = *pByteBuffer++;
	*(pFloatBytes + 1) = *pByteBuffer++;
	*pFloatBytes = *pByteBuffer++;
    
    // return the address of the following byte
    return pByteBuffer;
}

// stores the supplied 32 bit float value in the byte buffer, returning the address of the byte in the buffer
// following the written value
uint8 *WriteFloat(uint8 *pByteBuffer, float value)
{
    uint8 *pFloatBytes = (uint8 *) &value;

	// store the appropriate bytes of the 32 bit value into the first four bytes of the buffer, preserving the byte 
	// order from earlier code
	*pByteBuffer++ = *(pFloatBytes + 3);
	*pByteBuffer++ = *(pFloatBytes + 2);
	*pByteBuffer++ = *(pFloatBytes + 1);
	*pByteBuffer++ = *pFloatBytes;
    
	// return the address of the following byte
	return pByteBuffer;
}

// writes a string of characters to the buffer, prefixing the string with a single byte length of the string
// no terminating character is stored, returns the next byte after the written data
static uint8 *WriteStringWithLengthPrefix(uint8 *pByteBuffer, char *str)
{
	uint32 strLen = strlen(str);
    
    // write the length of the string
    pByteBuffer = WriteUInt32(pByteBuffer, strLen);
    
    // copy the string to the buffer, not incuding a terminator
    memcpy(pByteBuffer, str, strLen);
    
    // return the address of the following byte
    return pByteBuffer + strLen;
}

// sends the contents of the write buffer
static void Send()
{
    uint32 	i;
    int     writeResult;
    
	// send the contents of the buffer    
    writeResult = send(_socketHandle, _writeBuffer, _writeBufferLen, 0);
    
    // check for error
    if (writeResult == -1)
    {
        // write failed, abort
        printf("write failed with error %d\n", ERR_NUMBER);
        exit(0);
    }
    else if (writeResult != _writeBufferLen)
    {
        // failed, abort
        printf("write data streams are out of sync\n");
        exit(0);
    }
    
    // when printing out the raw message we don't include the header in terms of count or data 
    printf("\nTX: [%4d][ ", _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    for (i = IPC_MESSAGE_HEADER_LEN; i < _writeBufferLen; i++)
    {
        printf("0x%02X ", _writeBuffer[i]);
    }
    printf("]\n");    
}

// called when a complete message has been received
static void Received()
{
	uint32	i;
    
    // raw print
    printf("\nRX: [%4d][ ", _readBufferLen);
    for (i = 0; i < _readBufferLen; i++)
    {
        printf("0x%02X ", _readBuffer[i]);
    }
    printf("]\n");
    
    // parse and respond
    ParseReceived();
}

// sends a buffer of random bytes to the hub
static void SendNoise()
{
    uint32  length,
            index;
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;

    // get a random message length up to the maximum
    length = rand() % (IPC_MESSAGE_DATA_MAX + 1);
    
    // fill with noise
    for (index = 0; index < length; index++)
    {
        // random byte 
        pBuffer = WriteUInt8(pBuffer, rand());
    }
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a valid message with zero length
static void SendZeroLengthMessage()
{
    // clear the whole buffer, this includes the header byte count which we want to be zero
    memset(_writeBuffer, 0, IPC_MESSAGE_HEADER_LEN + IPC_MESSAGE_DATA_MAX);
    
    // set the message length to the header size
    _writeBufferLen = IPC_MESSAGE_HEADER_LEN;
    
    // send the cmd
    Send();
}

// sends a command with invalid data lengths wrapped in an otherwise valid application definition commaand
static void SendInvalidDataLength()
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN,
            *pStringLen;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_APPLICATION_DEFINITION);
    
    // app name with length prefix not including terminator
    pStringLen = pBuffer;
    pBuffer = WriteStringWithLengthPrefix(pBuffer, APP_NAME);
    *pStringLen += 5;

    // system dir path with too long length
    pStringLen = pBuffer;
    pBuffer = WriteStringWithLengthPrefix(pBuffer, SYS_DIR);
    *pStringLen += 5;
    
    // user dir path
    pStringLen = pBuffer;
    pBuffer = WriteStringWithLengthPrefix(pBuffer, USR_DIR);
    *pStringLen += 5;
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a complete application definition command to the hub
static void SendApplicationDefinition()
{
    char    pathBuffer[1024 + 1];
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_APPLICATION_DEFINITION);
    
    // app name with length prefix not including terminator
    pBuffer = WriteStringWithLengthPrefix(pBuffer, APP_NAME);

    // unmanaged mode means empty paths
    if (_unmanagedMode)
    {
        // unmanaged mode, both paths are empty
        pBuffer = WriteStringWithLengthPrefix(pBuffer, "");
        pBuffer = WriteStringWithLengthPrefix(pBuffer, "");
    }
    else
    {
        // standard mode, system dir path - base the dir in a sub-folder of the current working folder
        getcwd(pathBuffer, 1024);
        strncat(pathBuffer, SYS_DIR, 1024);    
        pBuffer = WriteStringWithLengthPrefix(pBuffer, pathBuffer);

        // no user dir path required in this version
        pBuffer = WriteStringWithLengthPrefix(pBuffer, "");
    }

    // task executable name, linked to protocol revision
    if (_protocolRevision >= 7)
    {
        // this field was introduced in ipc rev 7
        pBuffer = WriteStringWithLengthPrefix(pBuffer, EXE_NAME);
    }
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a request focus command to the hub
static void SendRequestFocus()
{
    uint8     *pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_REQUEST_FOCUS);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a release focus command to the hub
static void SendReleaseFocus()
{
    uint8     *pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_RELEASE_FOCUS);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends an all change command to the hub
static void SendAllChange()
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_ALL_CHANGE);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a parameter value command to the hub
static void SendParameterValue(uint32 parameterID)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    float   value;
    bool32  atDefault;
    
    /// fetch the current test value
    FetchParameterValue(parameterID, &value, &atDefault);
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_PARAMETER_VALUE);
    
    // parameter id
    pBuffer = WriteUInt32(pBuffer, parameterID);
    
    // parameter value
    pBuffer = WriteFloat(pBuffer, value);
    
    // at default flag
    pBuffer = WriteUInt32(pBuffer, atDefault);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a custom parameter value command to the hub
static void SendCustomParameterValue(uint32 hashID, char *controlString)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    float   value;
    bool32  atDefault;
    
    /// fetch the current test value, for custom params we use a hash of the string as the value id
    FetchParameterValue(hashID, &value, &atDefault);
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_CUSTOM_PARAMETER_VALUE);
    
    // custom parameter control string
    pBuffer = WriteStringWithLengthPrefix(pBuffer, controlString);
    
    // parameter value
    pBuffer = WriteFloat(pBuffer, value);
    
    // at default flag
    pBuffer = WriteUInt32(pBuffer, atDefault);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a menu string command to the hub
static void SendMenuString(uint32 menuID)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    char    *menuString;
    bool32  atDefault;
    
    // fetch the current test value
    FetchMenuValue(menuID, &menuString, &atDefault);
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_MENU_STRING);
    
    // menu id
    pBuffer = WriteUInt32(pBuffer, menuID);
    
    // menu string value 
    pBuffer = WriteStringWithLengthPrefix(pBuffer, menuString);
    
    // at default flag
    pBuffer = WriteUInt32(pBuffer, atDefault);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a custom menu string command to the hub
static void SendCustomMenuString(uint32 hashID, char *controlString)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    char    *menuString;
    bool32  atDefault;
    
    // fetch the current test value, for custom params we use a hash of the string as the value id
    FetchMenuValue(hashID, &menuString, &atDefault);
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_CUSTOM_MENU_STRING);
    
    // custom menu control string
    pBuffer = WriteStringWithLengthPrefix(pBuffer, controlString);
    
    // menu string value
    pBuffer = WriteStringWithLengthPrefix(pBuffer, menuString);
    
    // at default flag
    pBuffer = WriteUInt32(pBuffer, atDefault);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a mode value command to the hub
static void SendModeValue(uint32 modeID)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_MODE_VALUE);
    
    // mode id
    pBuffer = WriteUInt32(pBuffer, modeID);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a panel connection state request command to the hub
static void SendPanelConnectionStateRequest()
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_REQUEST_PANEL_CONNECTION_STATES);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends an unmanaged panel capabilities request to the hub for the given panel ids
static void SendUnmanagedPanelCapabilitiesRequest(uint32 panelID[], uint32 panelCount)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    uint32  index;
    
    // send multiple commands in a single message, one command per panel with id
    for (index = 0; index < panelCount; index++)
    {
        // cmd
        pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_UNMANAGED_PANEL_CAPABILITIES_REQUEST);
        
        // panel id
        pBuffer = WriteUInt32(pBuffer, panelID[index]);
    }
        
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);

    // send the cmd
    Send();
}

// sends an unmanaged display write to the hub
static void SendPanelDisplayWrite(uint32 panelID)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_UNMANAGED_DISPLAY_WRITE);
    
    // panel id
    pBuffer = WriteUInt32(pBuffer, panelID);

    // display number
    pBuffer = WriteUInt32(pBuffer, 0);

    // line number
    pBuffer = WriteUInt32(pBuffer, 0);

    // start position
    pBuffer = WriteUInt32(pBuffer, 0);

    // length of text and text
    pBuffer = WriteStringWithLengthPrefix(pBuffer, "Hello!");
    *(pBuffer - 1) = *(pBuffer - 1) | 0x80;
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a main display text command to the hub
static void SendMainDisplayText(char selection)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_ARC_DISPLAY_TEXT);

    // choose the target arc panel id
    switch (selection)
    {
        case 'F':
            // panel id
            pBuffer = WriteUInt32(pBuffer, 0x00120001); // arc function
            break;
        case 'G':
            // panel id
            pBuffer = WriteUInt32(pBuffer, 0x00130001); // arc grading
            break;
        case 'N':
            // panel id
            pBuffer = WriteUInt32(pBuffer, 0x00140001); // arc navigation
            break;
        default:
            // n/a
            return;
    }

    // text, number of strings
    pBuffer = WriteUInt32(pBuffer, 2);
    
    // first string of 19 chars
    pBuffer = WriteStringWithLengthPrefix(pBuffer, "1234567890123456789");
    pBuffer = WriteUInt32(pBuffer, 0);
    
    // second string of 8 chars
    pBuffer = WriteStringWithLengthPrefix(pBuffer, "12345678");
    pBuffer = WriteUInt32(pBuffer, 0);

    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;

    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);

    // send the cmd
    Send();
}

// sends a display text command to the hub, single, double or mixed height
static void SendDisplayText(char selection)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_DISPLAY_TEXT);

    switch (selection)
    {
        case 's':
            // single height text, number of strings
            pBuffer = WriteUInt32(pBuffer, 3);

            // first string of 32 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "12345678901234567890123456789012");
            pBuffer = WriteUInt32(pBuffer, 0);
            
            // second string of 16 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "1234567890123456");
            pBuffer = WriteUInt32(pBuffer, 0);        

            // third string of 8 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "12345678");
            pBuffer = WriteUInt32(pBuffer, 0);        
            break;
        case 'd':
            // double height text, number of strings
            pBuffer = WriteUInt32(pBuffer, 1);
            
            // first string of 16 chars, double height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "1234567890123456");
            pBuffer = WriteUInt32(pBuffer, 1);
            break;
        case 'm':
            // mixed height data, number of strings
            pBuffer = WriteUInt32(pBuffer, 2);

            // first string of 16 chars, double height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "1234567890123456");
            pBuffer = WriteUInt32(pBuffer, 1);            

            // second string of 32 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "12345678901234567890123456789012");
            pBuffer = WriteUInt32(pBuffer, 0);        
            break;
        case 'k':
            // skipped line data, number of strings
            pBuffer = WriteUInt32(pBuffer, 3);

            // first string, single height, invert the characters of the last word
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "First line of text");
            *(pBuffer - 4) = *(pBuffer - 4) | 0x80;
            *(pBuffer - 3) = *(pBuffer - 3) | 0x80;
            *(pBuffer - 2) = *(pBuffer - 2) | 0x80;
            *(pBuffer - 1) = *(pBuffer - 1) | 0x80;
            pBuffer = WriteUInt32(pBuffer, 0);            

            // second string of 0 chars, no height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "");

            // third string, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, "Third line of text");
            pBuffer = WriteUInt32(pBuffer, 0);            
            break;
        case 'b':
            // blank text, number of strings
            pBuffer = WriteUInt32(pBuffer, 3);

            // first string of 1 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, " ");
            pBuffer = WriteUInt32(pBuffer, 0);
            
            // second string of 1 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, " ");
            pBuffer = WriteUInt32(pBuffer, 0);        

            // third string of 1 chars, single height
            pBuffer = WriteStringWithLengthPrefix(pBuffer, " ");
            pBuffer = WriteUInt32(pBuffer, 0);        
            break;
        case 't':
            {
                char temp[16 + 1];
                
                // changing timecode frame number for each display call
                sprintf(temp, "  01:12:43:%02u   ", _timecodeFrameCount);
                _timecodeFrameCount = (_timecodeFrameCount + 1) % 25;
                
                // timecode text, number of strings
                pBuffer = WriteUInt32(pBuffer, 1);
                
                // string chars, double height
                pBuffer = WriteStringWithLengthPrefix(pBuffer, temp);
                pBuffer = WriteUInt32(pBuffer, 1);
            }
            break;
    }

    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a rename control command to the hub
static void SendRenameControl(uint32 targetID, char *name)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_RENAME_CONTROL);
    
    // target id
    pBuffer = WriteUInt32(pBuffer, targetID);
    
    // name string chars, double height
    pBuffer = WriteStringWithLengthPrefix(pBuffer, name);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a highlight control command to the hub
static void SendHighlightControl(uint32 targetID, uint32 state)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_HIGHLIGHT_CONTROL);
    
    // target id
    pBuffer = WriteUInt32(pBuffer, targetID);
    
    // state
    pBuffer = WriteUInt32(pBuffer, state);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends an indicate control command to the hub
static void SendIndicateControl(uint32 targetID, uint32 state)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_INDICATE_CONTROL);
    
    // target id
    pBuffer = WriteUInt32(pBuffer, targetID);
    
    // state
    pBuffer = WriteUInt32(pBuffer, state);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a rename custom control command to the hub
static void SendRenameCustomControl(char *controlString, char *name)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_RENAME_CUSTOM_CONTROL);
    
    // custom control string
    pBuffer = WriteStringWithLengthPrefix(pBuffer, controlString);
    
    // name string chars
    pBuffer = WriteStringWithLengthPrefix(pBuffer, name);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a highlight custom control command to the hub
static void SendHighlightCustomControl(char *controlString, uint32 state)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_HIGHLIGHT_CUSTOM_CONTROL);
    
    // custom control string
    pBuffer = WriteStringWithLengthPrefix(pBuffer, controlString);
    
    // state
    pBuffer = WriteUInt32(pBuffer, state);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends an indicate custom control command to the hub
static void SendIndicateCustomControl(char *controlString, uint32 state)
{
    uint8 	*pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_INDICATE_CUSTOM_CONTROL);
    
    // custom control string
    pBuffer = WriteStringWithLengthPrefix(pBuffer, controlString);
    
    // state
    pBuffer = WriteUInt32(pBuffer, state);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// sends a response command for a control training session
static void SendTrainResponse()
{
    uint8           *pBuffer = _writeBuffer + IPC_MESSAGE_HEADER_LEN;
    int32           dataType;
    char            *name,
                    *customString;
    float           min = 0.0,
                    max = 0.0,
                    step = 0.0;
    
    // cmd
    pBuffer = WriteUInt32(pBuffer, IPC_APP_COMMAND_TRAIN_RESPONSE);
    
    // choose a mapping target type from the control type, bit 16 is 0 for encoder, 1 for button
    // mapping data type may be action (4), parameter (5) or menu (6)
    if (_trainingControlID & 0x00010000)
    {
        // button control type, could be menu or action, choose action as an example
        printf("  responding to training for a button control\n");
        dataType = 4;
        name = "C Action";
        customString = "Some.Action.Path";
    }
    else
    {
        // encoder control type, mapping target data type as a parameter
        printf("  responding to training for an encoder control\n");
        dataType = 5;
        name = "C Param";
        customString = "Some.Parameter.Path";
        // params use min/max/step
        min = -100.0;
        max = 100.0;
        step = 0.1;
    }
    
    // mapping target data type
    pBuffer = WriteInt32(pBuffer, dataType);
    
    // display name
    pBuffer = WriteStringWithLengthPrefix(pBuffer, name);
    
    // custom control mapping string
    pBuffer = WriteStringWithLengthPrefix(pBuffer, customString);
    
    // min, max and step values - not always used depending on the target type but have to be sent
    pBuffer = WriteFloat(pBuffer, min);
    pBuffer = WriteFloat(pBuffer, max);
    pBuffer = WriteFloat(pBuffer, step);
    
    // set the length of data in the buffer, this length includes the header
    _writeBufferLen = pBuffer - _writeBuffer;
    
    // complete the message by prefixing with the message length, not including the header
    WriteUInt32(_writeBuffer, _writeBufferLen - IPC_MESSAGE_HEADER_LEN);
    
    // send the cmd
    Send();
}

// called to read data when it arrives at the socket and frames the bytes into complete messages
static void ReadData()
{
    int		readResult;
    
    // check if this is the first read of a new message
    if (_readBytesRemaining == 0)
    {
        // new packet, try to read the first 4 bytes which is a count of the actual data
        readResult = recv(_socketHandle, _readBuffer, 4, 0);
        if (readResult == -1)
        {
            // read failed, abort
            printf("length read failed with error %d\n", ERR_NUMBER);
            exit(0);
        }
        else if (readResult == 4)
        {
        	// get the length of the message and store as expected length and bytes remaining
            ReadUInt32(_readBuffer, &_readBufferLen);
            _readBytesRemaining = _readBufferLen;            
        }
        else if (readResult == 0)
        {
            // hub disconnected, abort
            printf("hub closed the connection\n");
            exit(0);            
        }
        else
        {
            // failed, abort
            printf("read data streams are out of sync with result %d\n", readResult);
            exit(0);
        }
    }
    
    // now try to read any bytes remaining
    if (_readBytesRemaining > 0)
    {
        uint8   *buffer = (uint8 *) &_readBuffer[_readBufferLen - _readBytesRemaining];
        
        // try to read as much as we can
        readResult = recv(_socketHandle, buffer, _readBytesRemaining, 0);
        
        // check the result
        if (readResult == -1)
        {
            // read failed, abort
            printf("data read failed with error %d\n", ERR_NUMBER);
            exit(0);
        }
        else
        {
            // read ok, adjust the remaining count by the number just read
            _readBytesRemaining -= readResult;
            
            // test to see if we have completed the message
            if (_readBytesRemaining == 0)
            {
                // we have a complete message to parse
                Received();
            }
        }
    }    
}

// helper hashing function for null terminated strings
static uint32 StringHash(char *str)
{
    unsigned char   *p = (unsigned char *) str;
    uint32          h = 0;

    // shift-add-XOR hash implementation
    while (*p != '\0')
    {
        h ^= (h << 5) + (h >> 2) + *p++;
    }
    
    return h;
}

// extracts incoming commands from the read buffer and responds appropriately
static void ParseReceived()
{
    uint32  cmd;
    bool32	cmdOK = true;
    uint8   *pCmdData = _readBuffer;
    
    while (((pCmdData - _readBuffer) < _readBufferLen) && cmdOK)
    {
        // we know we have at least 4 bytes left, convert that into a command identifier and handle accordingly
        pCmdData = ReadUInt32(pCmdData, &cmd);
        printf("\n");
        switch (cmd)
        {
            case IPC_HUB_COMMAND_INITIATE_COMMS:
                printf("IPC_HUB_COMMAND_INITIATE_COMMS:\n");
                {
                    uint32  index, 
                            panelCount,
                            panelType,
                            panelID[40];

                    // read the protocol revision
                    pCmdData = ReadUInt32(pCmdData, &_protocolRevision);
                    printf("  hub supports protocol revision %u\n", _protocolRevision);
                    
                    // read panel count
                    pCmdData = ReadUInt32(pCmdData, &panelCount);
                    printf("  %u panel(s) are listed\n", panelCount);
                    
                    // read all panel types and ids
                    for (index = 0; index < panelCount; index++)
                    {
                        pCmdData = ReadUInt32(pCmdData, &panelType);
                        pCmdData = ReadUInt32(pCmdData, &panelID[index]);
                        printf("    %02u: type %u with id 0x%08X\n", index + 1, panelType, panelID[index]);
                    }
                    
                    // respond with app definition
                    SendApplicationDefinition();
                    
                    // also request panel capabilities if in unmanaged mode
                    if (_unmanagedMode)
                    {
                        // ask for panel capabilities
                        SendUnmanagedPanelCapabilitiesRequest(panelID, panelCount);
                    }
                }                
                break;  
            case IPC_HUB_COMMAND_PARAMETER_CHANGE:
                printf("IPC_HUB_COMMAND_PARAMETER_CHANGE:\n");
                {
                    uint32  parameterID;
                    float   value;

                    // read the parameter id
                    pCmdData = ReadUInt32(pCmdData, &parameterID);
                    
                    // read the value delta
                    pCmdData = ReadFloat(pCmdData, &value);
                    
                    printf("  parameter ID 0x%08X has changed %+0.6f\n", parameterID, value);
                    
                    // change the parameter
                    ModifyParameterValue(parameterID, value);
                    
                    // respond with a param value
                    SendParameterValue(parameterID);
                }                
                break;  
            case IPC_HUB_COMMAND_PARAMETER_VALUE_REQUEST:
                printf("IPC_HUB_COMMAND_PARAMETER_VALUE_REQUEST:\n");
                {
                    uint32  parameterID;

                    // read the parameter id
                    pCmdData = ReadUInt32(pCmdData, &parameterID);

                    printf("  parameter ID 0x%08X requested\n", parameterID);
                    
                    // respond with a param value
                    SendParameterValue(parameterID);
                }                
                break;  
            case IPC_HUB_COMMAND_PARAMETER_RESET:
                printf("IPC_HUB_COMMAND_PARAMETER_RESET:\n");
                {
                    uint32  parameterID;

                    // read the parameter id
                    pCmdData = ReadUInt32(pCmdData, &parameterID);

                    printf("  parameter ID 0x%08X reset\n", parameterID);
                    
                    // reset the parameter
                    ResetParameter(parameterID);
                    
                    // respond with a param value
                    SendParameterValue(parameterID);
                }                
                break;  
            case IPC_HUB_COMMAND_CUSTOM_PARAMETER_CHANGE:
                printf("IPC_HUB_COMMAND_CUSTOM_PARAMETER_CHANGE:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    float   value;
                    uint32  hashID;

                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    // read the value delta
                    pCmdData = ReadFloat(pCmdData, &value);
                    
                    printf("  custom parameter control '%s' has changed %+0.6f\n", controlString, value);
                    
                    // we usually store test values using the param id but for custom testing we hash the string name and use that
                    hashID = StringHash(controlString);
                        
                    // change the parameter
                    ModifyParameterValue(hashID, value);
                    
                    // respond with a param value defined by the control string
                    SendCustomParameterValue(hashID, controlString);
                }
                break;
            case IPC_HUB_COMMAND_CUSTOM_PARAMETER_VALUE_REQUEST:
                printf("IPC_HUB_COMMAND_CUSTOM_PARAMETER_VALUE_REQUEST:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    uint32  hashID;
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    printf("  custom parameter control '%s' requested\n", controlString);
                    
                    // we usually store test values using the param id but for custom testing we hash the string name and use that
                    hashID = StringHash(controlString);

                    // respond with a param value defined by the control string
                    SendCustomParameterValue(hashID, controlString);
                }
                break;
            case IPC_HUB_COMMAND_CUSTOM_PARAMETER_RESET:
                printf("IPC_HUB_CUSTOM_COMMAND_PARAMETER_RESET:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    uint32  hashID;
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    printf("  custom parameter control '%s' reset\n", controlString);
                    
                    // we usually store test values using the param id but for custom testing we hash the string name and use that
                    hashID = StringHash(controlString);

                    // reset the parameter
                    ResetParameter(hashID);
                    
                    // respond with a param value defined by the control string
                    SendCustomParameterValue(hashID, controlString);
                }
                break;
            case IPC_HUB_COMMAND_MENU_CHANGE:
                printf("IPC_HUB_COMMAND_MENU_CHANGE:\n");
                {
                    uint32  menuID;
                    int32   value;

                    // read the menu id
                    pCmdData = ReadUInt32(pCmdData, &menuID);
                    
                    // read the value step
                    pCmdData = ReadInt32(pCmdData, &value);
                    
                    printf("  menu ID 0x%08X stepped %s\n", menuID, (value > 0)? "up": "down");
                    
                    // change the menu
                    ModifyMenuValue(menuID, value);
                    
                    // respond with a menu string
                    SendMenuString(menuID);
                }                
                break;  
            case IPC_HUB_COMMAND_MENU_RESET:
                printf("IPC_HUB_COMMAND_MENU_RESET:\n");
                {
                    uint32  menuID;

                    // read the menu id
                    pCmdData = ReadUInt32(pCmdData, &menuID);
                    
                    printf("  menu ID 0x%08X reset\n", menuID);
                    
                    // reset the menu
                    ResetMenu(menuID);
                    
                    // respond with a menu string
                    SendMenuString(menuID);
                }                
                break;  
            case IPC_HUB_COMMAND_MENU_STRING_REQUEST:
                printf("IPC_HUB_COMMAND_MENU_STRING_REQUEST:\n");
                {
                    uint32  menuID;

                    // read the menu id
                    pCmdData = ReadUInt32(pCmdData, &menuID);

                    printf("  menu ID 0x%08X requested\n", menuID);

                    // respond with a menu string
                    SendMenuString(menuID);                    
                }                
                break;  
            case IPC_HUB_COMMAND_CUSTOM_MENU_CHANGE:
                printf("IPC_HUB_COMMAND_CUSTOM_MENU_CHANGE:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    int32   value;
                    uint32  hashID;
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    // read the value step
                    pCmdData = ReadInt32(pCmdData, &value);
                    
                    printf("  custom menu control '%s' stepped %s\n", controlString, (value > 0)? "up": "down");
                    
                    // we usually store test values using the menu id but for custom testing we hash the string name and use that
                    hashID = StringHash(controlString);
                    
                    // change the menu
                    ModifyMenuValue(hashID, value);
                    
                    // respond with a menu string defined by the control string
                    SendCustomMenuString(hashID, controlString);
                }
                break;
            case IPC_HUB_COMMAND_CUSTOM_MENU_RESET:
                printf("IPC_HUB_COMMAND_CUSTOM_MENU_RESET:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    uint32  hashID;
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    printf("  custom menu control '%s' reset\n", controlString);
                    
                    // we usually store test values using the menu id but for custom testing we hash the string name and use that
                    hashID = StringHash(controlString);
                    
                    // reset the menu
                    ResetMenu(hashID);
                    
                    // respond with a menu string defined by the control string
                    SendCustomMenuString(hashID, controlString);
                }
                break;
            case IPC_HUB_COMMAND_CUSTOM_MENU_STRING_REQUEST:
                printf("IPC_HUB_COMMAND_CUSTOM_MENU_STRING_REQUEST:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    uint32  hashID;
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    printf("  custom menu control '%s' requested\n", controlString);
                    
                    // we usually store test values using the menu id but for custom testing we hash the string name and use that
                    hashID = StringHash(controlString);
                    
                    // respond with a menu string defined by the control string
                    SendCustomMenuString(hashID, controlString);
                }
                break;
            case IPC_HUB_COMMAND_ACTION_ON:
                printf("IPC_HUB_COMMAND_ACTION_ON:\n");
                {
                    uint32  actionID;
                    
                    // read the action id
                    pCmdData = ReadUInt32(pCmdData, &actionID);

                    printf("  action ID 0x%08X fired\n", actionID);
                    
                    // no response
                }                
                break;  
            case IPC_HUB_COMMAND_ACTION_OFF:
                printf("IPC_HUB_COMMAND_ACTION_OFF:\n");
                {
                    uint32  actionID;
                    
                    // read the action id
                    pCmdData = ReadUInt32(pCmdData, &actionID);

                    printf("  action ID 0x%08X released\n", actionID);
                    
                    // no response
                }                
                break;  
            case IPC_HUB_COMMAND_CUSTOM_ACTION_ON:
                printf("IPC_HUB_COMMAND_CUSTOM_ACTION_ON:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    printf("  custom action control '%s' fired\n", controlString);
                    
                    // no response
                }
                break;
            case IPC_HUB_COMMAND_CUSTOM_ACTION_OFF:
                printf("IPC_HUB_COMMAND_CUSTOM_ACTION_OFF:\n");
                {
                    uint32  strLen;
                    char    controlString[64];
                    
                    // read the str length of the control name
                    pCmdData = ReadUInt32(pCmdData, &strLen);
                    
                    // copy out the string and terminate
                    memcpy(controlString, pCmdData, strLen);
                    controlString[strLen] = '\0';
                    pCmdData += strLen;
                    
                    printf("  custom action control '%s' released\n", controlString);
                    
                    // no response
                }
                break;
            case IPC_HUB_COMMAND_MODE_CHANGE:
                printf("IPC_HUB_COMMAND_MODE_CHANGE:\n");
                {
                    uint32  modeID;
                    
                    // read the mode id
                    pCmdData = ReadUInt32(pCmdData, &modeID);

                    printf("  mode ID 0x%08X requested\n", modeID);
                    
                    // respond with mode value
                    SendModeValue(modeID);
                }                
                break;  
            case IPC_HUB_COMMAND_TRANSPORT:
                printf("IPC_HUB_COMMAND_TRANSPORT:\n");
                {
                    int32   jogValue,
                            shuttleValue;

                    // read the jog/shuttle values, only one should be non-zero
                    pCmdData = ReadInt32(pCmdData, &jogValue);
                    pCmdData = ReadInt32(pCmdData, &shuttleValue);

                    printf("  jog %+d, shuttle %+d\n", jogValue, shuttleValue);
                    
                    // no response
                }                
                break;  
            case IPC_HUB_COMMAND_UNMANAGED_ENCODER_CHANGE:
                printf("IPC_HUB_COMMAND_UNMANAGED_ENCODER_CHANGE:\n");
                {
                    uint32  panelID,
                            controlNumber;
                    float   value;

                    // read the panel id, control number and delta value
                    pCmdData = ReadUInt32(pCmdData, &panelID);
                    pCmdData = ReadUInt32(pCmdData, &controlNumber);
                    pCmdData = ReadFloat(pCmdData, &value);

                    printf("  unmanaged encoder number %u on panel ID 0x%08X has changed %+0.6f\n", controlNumber, panelID, value);
                    
                    // no response
                }                
                break;  
            case IPC_HUB_COMMAND_UNMANAGED_BUTTON_DOWN:
                printf("IPC_HUB_COMMAND_UNMANAGED_BUTTON_DOWN:\n");
                {
                    uint32  panelID,
                            controlNumber;

                    // read the panel id and control number
                    pCmdData = ReadUInt32(pCmdData, &panelID);
                    pCmdData = ReadUInt32(pCmdData, &controlNumber);

                    printf("  unmanaged button number %u on panel ID 0x%08X has been pressed\n", controlNumber, panelID);
                    
                    // no response
                }                
                break;  
            case IPC_HUB_COMMAND_UNMANAGED_BUTTON_UP:
                printf("IPC_HUB_COMMAND_UNMANAGED_BUTTON_UP:\n");
                {
                    uint32  panelID,
                            controlNumber;

                    // read the panel id and control number
                    pCmdData = ReadUInt32(pCmdData, &panelID);
                    pCmdData = ReadUInt32(pCmdData, &controlNumber);

                    printf("  unmanaged button number %u on panel ID 0x%08X has been released\n", controlNumber, panelID);
                    
                    // no response
                }                
                break;  
            case IPC_HUB_COMMAND_UNMANAGED_DISPLAY_REFRESH:
                printf("IPC_HUB_COMMAND_UNMANAGED_DISPLAY_REFRESH:\n");
                {
                    uint32  panelID;
                    
                    // read the panel id
                    pCmdData = ReadUInt32(pCmdData, &panelID);

                    printf("  unmanaged display refresh requested for panel ID 0x%08X\n", panelID);

                    // respond with a unmanaged display write
                    SendPanelDisplayWrite(panelID);
                }                
                break;  
            case IPC_HUB_COMMAND_UNMANAGED_PANEL_CAPABILITIES:
                printf("IPC_HUB_COMMAND_UNMANAGED_PANEL_CAPABILITIES:\n");
                {
                    uint32  panelID,
                            numButtons,
                            numEncoders,
                            numDisplays,
                            numLines,
                            numChars;
                    
                    // read the panel id
                    pCmdData = ReadUInt32(pCmdData, &panelID);

                    // read the capabilities
                    pCmdData = ReadUInt32(pCmdData, &numButtons);
                    pCmdData = ReadUInt32(pCmdData, &numEncoders);
                    pCmdData = ReadUInt32(pCmdData, &numDisplays);
                    pCmdData = ReadUInt32(pCmdData, &numLines);
                    pCmdData = ReadUInt32(pCmdData, &numChars);
                    
                    printf("  unmanaged panel capabilities for panel ID 0x%08X\n", panelID);
                    printf("    %u button(s)\n", numButtons);
                    printf("    %u encoder(s)\n", numEncoders);
                    printf("    %u display(s)\n", numDisplays);
                    printf("    %u line(s) per display\n", numLines);
                    printf("    %u character(s) per line\n", numChars);
                }                
                break;  
            case IPC_HUB_COMMAND_PANEL_CONNECTION_STATE:
                printf("IPC_HUB_COMMAND_PANEL_CONNECTION_STATE:\n");
                {
                    uint32  panelID,
                            connected;
                    
                    // read the panel id and connected state flag
                    pCmdData = ReadUInt32(pCmdData, &panelID);
                    pCmdData = ReadUInt32(pCmdData, &connected);
                    
                    printf("  panel ID 0x%08X is %s\n", panelID, connected? "connected": "disconnected");
                }
                break;
            case IPC_HUB_COMMAND_TRAIN_CONTROL:
                printf("IPC_HUB_COMMAND_TRAIN_CONTROL:\n");
                {
                    uint32  controlID;
                    
                    // read the control id
                    pCmdData = ReadUInt32(pCmdData, &controlID);
                    printf("  training started for control ID 0x%08X\n", controlID);
                    
                    // trigger a small countdown delay before we respond in this test implentation, storing the control id to determine the control type
                    _trainingControlID = controlID;
                    _trainingDelay = 3;
                }
                break;
            default:
                printf("Unhandled command 0x%X\n", cmd);
                cmdOK = false;
                break;  
        }
    }
}

// worker thread that handles monitoring the socket comms passing out messages as required
#ifdef TMH_PLATFORM_WINDOWS

// windows
static DWORD WINAPI CommsThreadRunner(LPVOID arg)

#else

// osx and linux
static void *CommsThreadRunner(void *arg)

#endif

{
    int             selectResult;
    fd_set          readSet;
    struct timeval  timeout;
    
    // the thread will continue until requested to stop by the flag
    while (!_commsThreadToExit) 
    {
        // on each interation of the loop we must re-apply the select sets as they may be modified during the loop
        FD_ZERO(&readSet);
        FD_SET(_socketHandle, &readSet);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // block until socket activity or timeout
        selectResult = select(FD_SETSIZE, &readSet, NULL, NULL, &timeout);
        
        // check the result
        if (selectResult == -1)
        {
            // error, log it but ignore
            printf("select call failed with error %d\n", ERR_NUMBER);
        }
        else if (selectResult == 0)
        {
            // timeout, this allows us to serve the exit flag before starting the next iteration
            // we also use this to fake a delay in control training to show the intermediate state
            if ((_trainingControlID != CS_CONTROL_ID_UNDEFINED) && (--_trainingDelay == 0))
            {
                // training is active and we have ticked over enough time before completing the training cycle
                SendTrainResponse();
                _trainingControlID = CS_CONTROL_ID_UNDEFINED;
            }
        }
        else if (FD_ISSET(_socketHandle, &readSet))
        {
        	// data available to be read
            ReadData();
        }      
    }
    
    // all done
#ifdef TMH_PLATFORM_WINDOWS

// windows
    return 0;

#else

// osx and linux
    return NULL;

#endif

}

// returns false to exit
static bool32 DoMenu()
{
    int	choice;
    
    // prompt
    printf(
    	"\n"
        "menu\n"
        "====\n"
        "  (a)pplication definition\n"
        " a(l)l change\n"
        "  (p)arameter value\n"
        " m(e)nu string\n"
        " m(o)de value\n"
        "  (s)ingle height display text\n"
        "  (d)ouble height display text\n"
        "  (m)ixed height display text\n"
        "  (t)imecode display text\n"
        " s(k)ipped line display text\n"
        "  (b)lank display text\n"
        "  (F)unction Arc display text\n"
        "  (G)rading Arc display text\n"
        "  (N)avigation Arc display text\n"
        "in(v)alid length\n"
        "  (n)oise\n"
        "  (z)ero length\n"
        "  (r)ename control (set)\n"
        "  (R)ename control (remove)\n"
        "  (h)ighlight control (on)\n"
        "  (H)ighlight control (off)\n"
        "  (i)ndicate control (on)\n"
        "  (I)ndicate control (off)\n"
        "  (P)anel connection state request\n"
        "  (f)ocus request\n"
        "  (g)ive back focus\n"
        "  (q)uit\n"
		"\n"
       	"enter an option or option sequence...\n"
    );
    
    // read the choice
	while ((choice = getc(stdin)) == 10)
    {
        // ignore returns
    }
    
	// handle the choice
	switch (choice)
    {
        case 'a':
        	// application definition command
            SendApplicationDefinition();
            break;
        case 'l':
            // all change command
            SendAllChange();
            break;
        case 'p':
            // parameter value command
            SendParameterValue(0x00030001);
            SendParameterValue(0x0003002B);
            SendParameterValue(0x00030017);
            SendParameterValue(0x0003000E);
            SendParameterValue(0x0003000C);
            SendParameterValue(0x0003000D);
            SendParameterValue(0x00030006);
            SendParameterValue(0x00030005);
            SendParameterValue(0x00030004);
            break;
        case 'e':
            // menu string command
            SendMenuString(0x00040001);
            break;
        case 'o':
            // mode value command
            SendModeValue(0x00010005);
            break;
        case 's':
            // single height display text command
            SendDisplayText('s');
            break;
        case 'd':
            // double height display text command
            SendDisplayText('d');
            break;
        case 'm':
            // mixed height display text command
            SendDisplayText('m');
            break;
        case 't':
            // timecode display text command
            SendDisplayText('t');
            break;
        case 'k':
            // skipped line display text command
            SendDisplayText('k');
            break;
        case 'b':
            // blank display text command
            SendDisplayText('b');
            break;
        case 'F':
            // arc function display text command
            SendMainDisplayText('F');
            break;
        case 'G':
            // arc grading display text command
            SendMainDisplayText('G');
            break;
        case 'N':
            // arc navigation display text command
            SendMainDisplayText('N');
            break;
        case 'v':
            // invalid malformed command, base on an app definition command
            SendInvalidDataLength();
            break;
        case 'n':
        	// noise
            SendNoise();
            break;
        case 'z':
            // zero length message
            SendZeroLengthMessage();
            break;
        case 'r':
            // rename control command, set
            SendRenameControl(0x00040004, "TLM");
            SendRenameControl(0x00010002, "Grad Mode");
            SendRenameCustomControl("cmd.app.action.1", "Act-1");
            break;
        case 'R':
            // rename control command, remove
            SendRenameControl(0x00040004, "");
            SendRenameControl(0x00010002, "");
            SendRenameCustomControl("cmd.app.action.1", "");
            break;
        case 'h':
            // highlight control command, on
            SendHighlightControl(0x00040004, 1);
            SendHighlightControl(0x00010002, 1);
            SendHighlightCustomControl("cmd.app.menu.a", 1);
            SendHighlightCustomControl("cmd.app.param.thing", 1);
            break;
        case 'H':
            // highlight control command, off
            SendHighlightControl(0x00040004, 0);
            SendHighlightControl(0x00010002, 0);
            SendHighlightCustomControl("cmd.app.menu.a", 0);
            SendHighlightCustomControl("cmd.app.param.thing", 0);
            break;
        case 'i':
            // indicate control command, on
            SendIndicateControl(0x00010002, 1);
            SendIndicateCustomControl("cmd.app.action.2", 1);
            break;
        case 'I':
            // indicate control command, off
            SendIndicateControl(0x00010002, 0);
            SendIndicateCustomControl("cmd.app.action.2", 0);
            break;
        case 'P':
            // request panel connection states
            SendPanelConnectionStateRequest();
            break;
        case 'f':
            // request focus command
            SendRequestFocus();
            break;
        case 'g':
            // release focus command
            SendReleaseFocus();
            break;
        case 'q':
            // quit
            return false;
        default:
            break;
    }    
    
    // do it again
    return true;
}

// mock application for basic ipc testing
int main (int argc, const char * argv[]) 
{
    struct sockaddr_in  address;

#ifdef TMH_PLATFORM_WINDOWS

// windows
    WORD                versionRequested;
    WSADATA             wsaData;
    int                 wsaResult;
    HANDLE              threadHandle;

#else

// osx and linux
    int                 pthreadResult;
    pthread_t	        commsThreadID;

#endif

    printf("mock application starting up [2022-10-12:r%u]\n", IPC_PROTOCOL_REVISION);
    printf("using inet based socket with port %d\n", IPC_SOCKET_PORT_NUMBER);
    
    // check for unmanaged mode switch
    if ((argc > 1) && !strcmp(argv[1], "-unmanaged"))
    {
        // enable unmanaged app mode
        _unmanagedMode = true;
        printf("unmanaged mode enabled\n");
    }
    
#ifdef TMH_PLATFORM_WINDOWS

// windows
    versionRequested = MAKEWORD(2, 2);

    // start winsock
    wsaResult = WSAStartup(versionRequested, &wsaData);
    if (wsaResult != 0) 
    {
        printf("WSAStartup failed with error: %d\n", wsaResult);
        exit(0);
    }
    printf("winsock version %d.%d ok\n", LOBYTE(wsaData.wVersion), LOBYTE(wsaData.wVersion));

#endif

    // use the same seed for rand() calls  to be able to repeat the sequence
    srand(0x12345678);

    // create the socket
    _socketHandle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_socketHandle == INVALID_SOCKET)
    {
        printf("create socket failed with error %d\n", ERR_NUMBER);
        exit(0);
    }
    printf("socket ok\n");
    
    // prepare the address
    memset(&address, 0, sizeof(address));

#ifdef TMH_PLATFORM_OSX

    // osx only
    address.sin_len = sizeof(address);

#endif

    address.sin_family = AF_INET;
    address.sin_port = htons(IPC_SOCKET_PORT_NUMBER);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	// connect to the hub    
    if (connect(_socketHandle, (struct sockaddr *) &address, sizeof(address)) == -1)
    {
        printf("connect failed with error %d\n", ERR_NUMBER);
        exit(0);
    }
    printf("connected\n");
    
    // kick off the thread that will monitor for incoming socket comms

#ifdef TMH_PLATFORM_WINDOWS

// windows
    threadHandle = CreateThread(NULL, 0, CommsThreadRunner, NULL, 0, NULL);
    if (threadHandle == NULL)
    {
		printf("create thread failed with error %d\n", GetLastError());
        exit(0);
    }    

#else

// osx and linux
    pthreadResult = pthread_create(&commsThreadID, NULL, CommsThreadRunner, NULL);
    if (pthreadResult != 0)
    {
		printf("pthread failed with error %d\n", pthreadResult);
        exit(0);
    }    

#endif

	printf("comms thread running\n");
    
    // repeat the menu until asked to quit
    while (DoMenu());

#ifdef TMH_PLATFORM_WINDOWS

// windows
    WSACleanup();

#endif

    // all done - as an additional test we simply quit leaving things open for the system to handle rather
    // than shutting down neatly
    return 0;
}
