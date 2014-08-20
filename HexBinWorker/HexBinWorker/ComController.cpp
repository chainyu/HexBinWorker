#include "StdAfx.h"
#include "ComController.h"


ComController::ComController(void): 
				_hCom(false, 0) {
    _hCom.SetState(115200, 8, EVENPARITY, ONESTOPBIT);
}


ComController::~ComController(void) {
    _hCom.Close();
}

bool ComController::openCom(int comNumber){
    _comNumber = comNumber;
	bool openOK = _hCom.Open(_comNumber);
	if (openOK)	{
        _hCom.SetState(115200, 8, EVENPARITY, ONESTOPBIT);
        return true;
    } else {
		CString errMessage;
        errMessage.Format(_T("�޷��򿪴��ڣ�COM%d"), _comNumber);
		TRACE(errMessage);
		return false;
	}
}

bool ComController::getCommand() {

    BYTE *revData = new BYTE[15];  //TODO: _revData ?
    int saveCount = 0;

    bool getCommandOK = false;
    while (true)
	{
	    _hCom.Write(GET_COMMAND, 2);
	    _hCom.Read(revData, 15);
		
        if (revData[0] == ACK) { // get!
            // TODO: ..and check ?
            getCommandOK = true;
            break;
        }

        if (saveCount == 100) {
            getCommandOK = false;
            break;
        }

        saveCount++;
    }


    if (getCommandOK) {
        CString bufferCStr, bufferBlock;
	    for (int i=0; i<15; i++) {
		    bufferCStr.Format(_T("%02X"), revData[i]);
		    bufferBlock += bufferCStr;
	    }
    }
		
	delete [] revData;
    return getCommandOK;
}
bool ComController::eraseMemory() {
    BYTE *revData = new BYTE[1];  //TODO: _revData ?
    int saveCount = 0;

    bool eraseMemoryOK = false;
    while (true) {
        _hCom.Write(ERASE_MEMORY, 2);
	    _hCom.Read(revData, 1);
    
        if (revData[0] == ACK) { // get!
            _hCom.Write(GLOBAL_ERASE, 2);
            _hCom.Read(revData, 1);

            if (revData[0] == ACK) {
                eraseMemoryOK = true;
            } else {
                eraseMemoryOK = false;
            }

            break;
        }

        if (saveCount == 100) {
           eraseMemoryOK = false;
           break;
        }

        saveCount++;

    }

    delete [] revData;
    return eraseMemoryOK;
}

bool ComController::sendWriteMemoryHead() {
    BYTE revFlag[1] = { 0xFF };   
    
    _hCom.Write(WRITE_MEMORY, 2);
	_hCom.Read(revFlag, 1);

    if (revFlag[0] == NACK) {
        return false; 
    }

    return true;
}
bool ComController::sendWriteMemoryAddr(long MSB, long LSB) {
    /* Send the start address (4 bytes) & checksum
       Byte 3 to byte 6:start address
         byte 3: MSB
         byte 6: LSB
       Byte 7: Checksum: XOR (Byte3, Byte4, Byte5, Byte6) 

      |  3  |  4  |  5  |  6  |  7  |
      | MSB |     |     | LSB | XOR |

       Wait for ACK
    */
    BYTE revFlag[1] = { 0xFF };

    const int addrSize = 5;
    BYTE* addr = new BYTE[addrSize];
    memset(addr, 0x00, addrSize);

    addr[0] = MSB >> 8;
    addr[1] = MSB;
    addr[2] = LSB >> 8;
    addr[3] = LSB;

    // addr checksum
    addr[4] = addr[0]^addr[1]^addr[2]^addr[3];

    _hCom.Write(addr, addrSize);
    _hCom.Read(revFlag, 1);
    
    if (revFlag[0] == NACK) {
        delete [] addr; //TODO: DRY
        return false; 
    }

    delete [] addr;
    return true;
}
bool ComController::sendWriteMemoryData(BYTE* datas, int dataSize, int currentIndex) {

    BYTE revFlag[1] = { 0xFF };

    /* Send the number of bytes to be written
        (1 byte), the data (N + 0x01) bytes) & checksum
    */
    BYTE* data = new BYTE[1 + 16 + 1 + 1]; // 19
    
    // Byte 8: Number of bytes to be received (0 < N ��255)
    int dataLength = (dataSize - currentIndex >= 16) ? 16 : dataSize - currentIndex;
    data[0] = static_cast<BYTE>(dataLength);

    BYTE dataSumCheck = data[0];
    const int currentLine = (int)(currentIndex / 16);
    for (int i=1; i <= dataLength; i++ ) {
        
        int datasIndex = currentLine * 16 + i - 1;
        data[i] = datas[datasIndex];
        dataSumCheck ^= data[i];
    }

    data[dataLength+1] = 0x01;
    dataSumCheck ^= 0x01;
    data[dataLength+2] =  dataSumCheck;
    
    _hCom.Write(data, dataLength+3);
    _hCom.Read(revFlag, 1);

    if (revFlag[0] == NACK) {
        delete [] data; //TODO: DRY
        return false; 
    }

    delete [] data;
    return true;
}

bool ComController::writeMemory(BYTE* datas, int dataSize, long startAddress) {
/* 
if the write operation was successful, the bootloader transmits the ACK byte; 
otherwise it transmits a NACK byte to the user and aborts the command

The maximum length of the block to be written for the STM32F10xxx is 256 bytes.

When writting to the RAM, care must be taken to avoid overlapping with the first 512 bytes 
(0x200) in RAM because they are used by the bootloader firmware.
*/
    BYTE revFlag[1] = { 0x1F };
    BYTE dataLength = 0x00;

    for (long l = 0; l < dataSize; l++) {

        if (l % 16 == 0) {

            if (!sendWriteMemoryHead()) {
                return false;
            }

            if (!sendWriteMemoryAddr(startAddress, l)) {
                return false;
            }

           dataLength = static_cast<BYTE>((dataSize - l >= 16) ? 16 : dataSize - l);

		}

        if (l % 16 == dataLength - 1) {
            if (!sendWriteMemoryData(datas, dataSize, l)) {
                return false;
            }
        }
	}
    return true;
}