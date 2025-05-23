#include "MyLD2410_esp.hpp"

namespace LD2410
{
  const char *tStatus[7]{
      "No target",
      "Moving only",
      "Stationary only",
      "Both moving and stationary",
      "Auto thresholds in progress",
      "Auto thresholds successful",
      "Auto thresholds failed"};
  const byte headData[4]{0xF4, 0xF3, 0xF2, 0xF1};
  const byte tailData[4]{0xF8, 0xF7, 0xF6, 0xF5};
  const byte headConfig[4]{0xFD, 0xFC, 0xFB, 0xFA};
  const byte tailConfig[4]{4, 3, 2, 1};
  const byte configEnable[6]{4, 0, 0xFF, 0, 1, 0};
  const byte configDisable[4]{2, 0, 0xFE, 0};
  const byte MAC[6]{4, 0, 0xA5, 0, 1, 0};
  const byte firmware[4]{2, 0, 0xA0, 0};
  const byte res[4]{2, 0, 0xAB, 0};
  const byte resCoarse[6]{4, 0, 0xAA, 0, 0, 0};
  const byte resFine[6]{4, 0, 0xAA, 0, 1, 0};
  const byte changeBaud[6]{4, 0, 0xA1, 0, 7, 0};
  const byte reset[4]{2, 0, 0xA2, 0};
  const byte reboot[4]{2, 0, 0xA3, 0};
  const byte BTon[6]{4, 0, 0xA4, 0, 1, 0};
  const byte BToff[6]{4, 0, 0xA4, 0, 0, 0};
  const byte BTpasswd[10]{8, 0, 0xA9, 0, 0x48, 0x69, 0x4C, 0x69, 0x6E, 0x6B};
  const byte param[4]{2, 0, 0x61, 0};
  const byte engOn[4]{2, 0, 0x62, 0};
  const byte engOff[4]{2, 0, 0x63, 0};
  const byte auxQuery[4]{2, 0, 0xAE, 0};
  const byte auxConfig[8]{6, 0, 0xAD, 0, 0, 0x80, 0, 0};
  const byte autoBegin[6]{4, 0, 0x0B, 0, 0x0A, 0};
  const byte autoQuery[4]{2, 0, 0x1B, 0};
  byte gateParam[0x16]{0x14, 0, 0x64, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0};
  byte maxGate[0x16]{0x14, 0, 0x60, 0, 0, 0, 8, 0, 0, 0, 1, 0, 8, 0, 0, 0, 2, 0, 5, 0, 0, 0};

const char* byte2hex(byte b, bool addZero=true) {
    static char hexStr[3]; // Static buffer for the string
    if (addZero && b < 0x10) {
        snprintf(hexStr, sizeof(hexStr), "0%X", b);
    } else {
        snprintf(hexStr, sizeof(hexStr), "%X", b);
    }
    return hexStr;
}

void printBuf(const byte *buf, byte size) {
    char hexBuffer[5]; // Enough for "0xXX "
    
    for (byte i = 0; i < size; i++) {
        snprintf(hexBuffer, sizeof(hexBuffer), "0x%02X", buf[i]);
        ESP_LOGI(TAG, "Buffer[%d]: %s", i, hexBuffer);
    }
    uart_wait_tx_done(UART_NUM_1, portMAX_DELAY);
}

bool bufferEndsWith(const char *buf, int iMax, const char *other) {
    if (iMax < 3) return false; // Not enough data to compare
    
    for (int j = 3; j >= 0; j--) {
        if (buf[iMax - (3 - j)] != other[j]) {
            return false;
        }
    }
    return true;
}
} // namespace LD2410


MyLD2410::Response MyLD2410::check() {
    size_t length = 0;
    uart_get_buffered_data_len(UART_NUM_1, &length);
    
    while (length > 0) {
        uint8_t byte_read;
        int len = uart_read_bytes(UART_NUM_1, &byte_read, 1, pdMS_TO_TICKS(10));
        
        if (len == 1) {
            headBuf[headBufI++] = byte_read;
            headBufI %= 4;
            
            // Check for configuration frame header
            if (LD2410::bufferEndsWith((const char*)headBuf, headBufI, (const char*)LD2410::headConfig)) {
                if (processAck()) {
                    return ACK;
                }
            }
            // Check for data frame header
            else if (LD2410::bufferEndsWith((const char*)headBuf, headBufI, (const char*)LD2410::headData)) {
                if (processData()) {
                    return DATA;
                }
            }
        } else {
            // Timeout or error occurred
            break;
        }
        
        // Update remaining data length
        uart_get_buffered_data_len(UART_NUM_1, &length);
    }
    
    return FAIL;
}
bool MyLD2410::processData() {
    if (!readFrame()) {
        return false;
    }
    
    //   if (_debug) {
    //     ESP_LOGI(TAG, "Processed data - Status: %" PRIu8 ", Moving Dist: %" PRIu16 "cm, Stationary Dist: %" PRIu16 "cm",
    //             (uint8_t)sData.status, 
    //             (uint16_t)sData.mTargetDistance, 
    //             (uint16_t)sData.sTargetDistance);
    // }
    
    if (!LD2410::bufferEndsWith((const char*)inBuf, inBufI, (const char*)LD2410::tailData)) {
        return false;
    }
    
    // Check frame type (Basic or Enhanced mode)
    if (((inBuf[0] == 1) || (inBuf[0] == 2)) && (inBuf[1] == 0xAA)) {
        // Update timestamp using ESP32's time function
        sData.timestamp = esp_timer_get_time() / 1000; // Convert µs to ms
        
        // Parse common data fields
        sData.status = inBuf[2] & 0x07;
        sData.mTargetDistance = inBuf[3] | (inBuf[4] << 8);
        sData.mTargetSignal = inBuf[5];
        sData.sTargetDistance = inBuf[6] | (inBuf[7] << 8);
        sData.sTargetSignal = inBuf[8];
        sData.distance = inBuf[9] | (inBuf[10] << 8);
        
        if (inBuf[0] == 1) {  // Enhanced mode
            isEnhanced = true;
            
            // Parse target signals
            sData.mTargetSignals.setN(inBuf[11]);
            sData.sTargetSignals.setN(inBuf[12]);
            
            // Use pointer arithmetic safely
            const byte* p = inBuf + 13;
            for (byte i = 0; i <= sData.mTargetSignals.N && (p - inBuf) < inBufI; i++) {
                sData.mTargetSignals.values[i] = *p++;
            }
            for (byte i = 0; i <= sData.sTargetSignals.N && (p - inBuf) < inBufI; i++) {
                sData.sTargetSignals.values[i] = *p++;
            }
            
            // Parse additional fields with bounds checking
            if ((p - inBuf) < inBufI) lightLevel = *p++;
            if ((p - inBuf) < inBufI) outLevel = *p;
        } 
        else {  // Basic mode (inBuf[0] == 2)
            isEnhanced = false;
            sData.mTargetSignals.setN(0);
            sData.sTargetSignals.setN(0);
            lightLevel = 0;
            outLevel = 0;
        }
        
        // if (_debug) {
        //     ESP_LOGI(TAG, "Processed data - Status: %d, Moving Dist: %dcm, Stationary Dist: %dcm",
        //             sData.status, sData.mTargetDistance, sData.sTargetDistance);
        // }
        
        return true;
    }
    
    return false;
}

bool MyLD2410::processAck() {
    if (!readFrame()) {
        return false;
    }
    
    if (_debug) {
        LD2410::printBuf(inBuf, inBufI);
    }
    
    if (!LD2410::bufferEndsWith((const char*)inBuf, inBufI, (const char*)LD2410::tailConfig)) {
        return false;
    }
    
    unsigned long command = inBuf[0] | (inBuf[1] << 8);
    if (inBuf[2] | (inBuf[3] << 8)) {
        return false;
    }
    
    switch (command) {
        case 0x1FF: // entered config mode
            isConfig = true;
            version = inBuf[4] | (inBuf[5] << 8);
            bufferSize = inBuf[6] | (inBuf[7] << 8);
            break;
            
        case 0x1FE: // exited config mode
            isConfig = false;
            break;
            
        case 0x1A5: { // MAC
              char mac_temp[18];
              for (int i = 0; i < 6; i++) {
                  MAC[i] = inBuf[i + 4];
                  snprintf(mac_temp + i*3, 4, "%02X%s", MAC[i], (i < 5) ? ":" : "");
              }
              MACstr = mac_temp; // std::string assignment handles memory
              break;
          }
          
          case 0x1A0: { // Firmware
              firmwareMajor = inBuf[7];
              firmwareMinor = inBuf[6];
              
              char fw_temp[16];
              snprintf(fw_temp, sizeof(fw_temp), "%d.%d.%02X%02X%02X", 
                      firmwareMajor, firmwareMinor, inBuf[11], inBuf[10], inBuf[9]);
              
              firmware = fw_temp; // std::string assignment handles memory
              break;
          }
            
        case 0x1AB: // Query Resolution
            fineRes = inBuf[4];
            break;
            
        case 0x1AE: // Query auxiliary control parameters
            lightControl = LightControl(inBuf[4]);
            lightThreshold = inBuf[5];
            outputControl = OutputControl(inBuf[6]);
            break;
            
        case 0x11B:
            autoStatus = AutoStatus(inBuf[4]);
            break;
            
        case 0x1A3: // Reboot
            isEnhanced = false;
            isConfig = false;
            break;
            
        case 0x161: { // Query parameters
            maxRange = inBuf[5];
            movingThresholds.setN(inBuf[6]);
            stationaryThresholds.setN(inBuf[7]);
            
            for (byte i = 0; i <= movingThresholds.N; i++) {
                movingThresholds.values[i] = inBuf[8 + i];
            }
            
            for (byte i = 0; i <= stationaryThresholds.N; i++) {
                stationaryThresholds.values[i] = inBuf[17 + i];
            }
            
            noOne_window = inBuf[26] | (inBuf[27] << 8);
            break;
        }
            
        case 0x162:
            isEnhanced = true;
            break;
            
        case 0x163:
            isEnhanced = false;
            break;
            
        case 0x164:
            if (LD2410::gateParam[7] == 0xFF) {
                LD2410::gateParam[7] = 0;
            }
            break;
    }
    
    return true;
}

bool MyLD2410::readFrame() {
    int frameSize = -1;
    int bytes = 2;
    
    if (bytes > 0) {
        inBufI = 0;
        while (bytes > 0) {
            uint8_t byte_read;
            int len = uart_read_bytes(UART_NUM_1, &byte_read, 1, pdMS_TO_TICKS(100));
            if (len == 1) {
                inBuf[inBufI++] = byte_read;
                bytes--;
            } else {
                // Timeout or error
                return false;
            }
        }
        
        frameSize = inBuf[0] | (inBuf[1] << 8);
    }
    
    if (frameSize <= 0) {
        return false;
    }
    
    frameSize += 4; // Add header and checksum bytes
    inBufI = 2; // We already have the first 2 bytes
    
    while (frameSize > 0) {
        uint8_t byte_read;
        int len = uart_read_bytes(UART_NUM_1, &byte_read, 1, pdMS_TO_TICKS(100));
        if (len == 1) {
            inBuf[inBufI++] = byte_read;
            frameSize--;
        } else {
            // Timeout or error
            return false;
        }
    }
    
    return true;
}

esp_err_t MyLD2410::uartInit(){
    esp_err_t err;

    uart_config_t uart_config = {
        .baud_rate = LD2410_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,  // Add this line
        .source_clk = UART_SCLK_DEFAULT
    };
    err = uart_param_config(UART_NUM_1, &uart_config);
     err = uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    return(err);
}

bool MyLD2410::sendCommand(const byte *command) {
    byte size = command[0] + 2;
    
    // Send header
    uart_write_bytes(UART_NUM_1, (const char*)LD2410::headConfig, 4);
    
    // Send command
    uart_write_bytes(UART_NUM_1, (const char*)command, size);
    
    // Send footer
    uart_write_bytes(UART_NUM_1, (const char*)LD2410::tailConfig, 4);
    
    // Wait for transmission to complete
    uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(timeout));
    
    // Set up timeout
    uint64_t giveUp = esp_timer_get_time() / 1000 + timeout; // Convert µs to ms
    uint8_t headBuf[4] = {0};
    uint8_t headBufI = 0;
    
    while ((esp_timer_get_time() / 1000) < giveUp) {
        uint8_t byte_read;
        int len = uart_read_bytes(UART_NUM_1, &byte_read, 1, pdMS_TO_TICKS(10));
        
        if (len == 1) {
            headBuf[headBufI++] = byte_read;
            headBufI %= 4;
            
            if (LD2410::bufferEndsWith((const char*)headBuf, headBufI, (const char*)LD2410::headConfig)) {
                printf("[sendCommand] reached\n\r");
                return processAck();
            }
        }
    }
    return false;
}

bool MyLD2410::begin() {
    // Wait for the sensor to come online, or to timeout
    uint64_t start_time = esp_timer_get_time() / 1000; // Get current time in milliseconds
    uint64_t giveUp = start_time + timeout;
    bool online = false;
    isConfig = false;

    // Send initial configuration disable command
    sendCommand(LD2410::configDisable);

    while ((esp_timer_get_time() / 1000) < giveUp) {
        if (check() != FAIL) {
            online = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(110)); // Non-blocking delay
    }

    return online;
}