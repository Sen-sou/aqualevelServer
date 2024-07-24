#include <SD.h>
#include <SPI.h>
#include <WT_debug.h>
#include <cstring>

#define ChipSelectPin D8
#define MAX_TRANSFER_BYTES 8280
#define MAX_RESULT_SIZE 9216

namespace WT_fileHandler {

    void showSDInfo();
    const char* getFileContent(const char* filePath, uint32_t startPos, uint32_t endPos);
    void setFileContent(String& content, const char* filePath, bool overWrite, uint32_t appendPos, bool create);
    bool checkFile(const char* filePath);
    void clearResultData();
    void getSensorLogSize();
    void setup();

    bool showInfo{true};
    File file;
    char resultData[MAX_RESULT_SIZE];
    long lastReadByte {0};
    long sensorLogSize {0};

    void setup() {
        Serial.print("Initializing SD card...");
        if (SD.begin(ChipSelectPin, SD_SCK_MHZ(1)))
        {
            Serial.println("SD Card Initialization Done!");
            showSDInfo();
            getSensorLogSize();
            return;
        }
        Serial.println("Could not Intialize SD Card or Could not detect SD Card");
        showSDInfo();
        DebugCode::sdCardIntializationError();
        ESP.reset();
    }

    void showSDInfo() {
        if (!showInfo) return;
        Serial.print("Card type:         ");
        Serial.println(SD.type());
        Serial.print("fatType:           ");
        Serial.println(SD.fatType());
        Serial.print("size:              ");
        Serial.println(SD.size());
    }

    const char* getFileContent(const char* filePath, uint32_t startPos = 0, uint32_t endPos = 0) {
        if (!checkFile(filePath)) return "";
        clearResultData();

        file = SD.open(filePath, FILE_READ);
        file.seek(startPos, SeekMode::SeekSet);
        if ((endPos < startPos) || (endPos == 0) || (endPos > file.size()))
            endPos = (file.available() > MAX_TRANSFER_BYTES) ? MAX_TRANSFER_BYTES : file.available();
        else endPos -= startPos;

        // Serial.println("Reading " + String(filePath));
        file.readBytes(resultData, endPos);
        resultData[endPos + 1] = '\0';
        // Serial.println("Number of bytes read: " + String(readbytes));
        if (file.position() == file.size()) lastReadByte = -1;
        else lastReadByte = file.position();
        // Serial.println("Read from File:\n " + resultData);
        file.close();

        return resultData;
    }

    // may accept point for set
    void setFileContent(String& content, const char* filePath, bool overWrite = true, uint32_t appendPos = 0, bool create = false) {
        if (!create && !checkFile(filePath)) return;

        // Serial.println("Writing " + String(filePath));
        if (overWrite) {
            file = SD.open(filePath, FILE_WRITE);
            file.seek(0, SeekMode::SeekSet);
            file.print(content);
        } else if (appendPos == 0){
            file = SD.open(filePath, FILE_WRITE);
            file.seek(0, SeekMode::SeekEnd);
            file.print(content);
        } else {
            file = SD.open(filePath, FILE_READ);
            file.seek(appendPos, SeekMode::SeekSet);
            char temp[file.size() - appendPos];
            file.readBytes(temp, file.size() - appendPos);
            file.close();
            file = SD.open(filePath, FILE_WRITE);
            file.seek(appendPos, SeekMode::SeekSet);
            file.print(content);
            file.print(temp);
        }
        if (std::strcmp(filePath, "SensorDataLog.txt") == 0) sensorLogSize = file.size();
        file.close();
    }

    bool checkFile(const char* filePath) {
        if (!SD.exists(filePath)) {
            Serial.println(String(filePath) + " not Found!!");
            DebugCode::sdCardFileNotFoundError();    
            return false;
        }
        return true;
    }

    void clearResultData() {
        memset(resultData, 0, MAX_RESULT_SIZE);
    }

    void getSensorLogSize() {
        file = SD.open("SensorDataLog.txt", FILE_READ);
        sensorLogSize = file.size();
        file.close();
    }


}