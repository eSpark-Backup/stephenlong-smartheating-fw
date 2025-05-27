#include "MyLD2410_esp.hpp"

#define UART_PORT_NUM      UART_NUM_1
// #define UART_TX_PIN        GPIO_NUM_7
#define UART_TX_PIN        GPIO_NUM_5
// #define UART_RX_PIN        GPIO_NUM_6
#define UART_RX_PIN        GPIO_NUM_4

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

bool bufferEndsWith(const char *buf, int iMax, const char *other) 
{
    // if (iMax < 3) return false; // Not enough data to compare
    
     
    for (int j = 3; j >= 0; j--)
    {
      if (--iMax < 0)
        iMax = 3;
      if (buf[iMax] != other[j])
        return false;
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
                        // printf("head buff value: %x\n\r", headBuf[(headBufI - 1 + 4) % 4]); 
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
        printf("process data readFrame FAILED\n\r"); 
        return false;
    }
    printf("process data readFrame PASSED\n\r"); 
    
    //   if (_debug) {
    //     ESP_LOGI(TAG, "Processed data - Status: %" PRIu8 ", Moving Dist: %" PRIu16 "cm, Stationary Dist: %" PRIu16 "cm",
    //             (uint8_t)sData.status, 
    //             (uint16_t)sData.mTargetDistance, 
    //             (uint16_t)sData.sTargetDistance);
    // }
    
    if (!LD2410::bufferEndsWith((const char*)inBuf, inBufI, (const char*)LD2410::tailData)) {
        printf("process data buffer ends with FAILED\n\r"); 
        return false;
    }
    printf("process data buffer ends with passed\n\r"); 
    printf("process data inBuf[2] : %x\n\r",inBuf[2]);
    printf("process data inBuf[3] : %x\n\r",inBuf[3]);
    // Check frame type (Basic or Enhanced mode)
    if (((inBuf[2] == 1) || (inBuf[2] == 2)) && (inBuf[3] == 0xAA)) {
        // Update timestamp using ESP32's time function
        sData.timestamp = esp_timer_get_time() / 1000; // Convert µs to ms
        
        // Parse common data fields
        sData.status = inBuf[4] & 0x07;
        sData.mTargetDistance = inBuf[5] | (inBuf[6] << 8);
        sData.mTargetSignal = inBuf[7];
        sData.sTargetDistance = inBuf[8] | (inBuf[9] << 8);
        sData.sTargetSignal = inBuf[10];
        sData.distance = inBuf[11] | (inBuf[12] << 8);
        
        if (inBuf[2] == 1) {  // Enhanced mode
            isEnhanced = true;
            
            // Parse target signals
            sData.mTargetSignals.setN(inBuf[13]);
            sData.sTargetSignals.setN(inBuf[14]);
            
            // Use pointer arithmetic safely
            const byte* p = inBuf + 14;
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
         printf("[readFrame] failed\n\r");
        return false;
    }
    
    if (_debug) {
        LD2410::printBuf(inBuf, inBufI);
    }
 
    if (!LD2410::bufferEndsWith((const char*)inBuf, inBufI, (const char*)LD2410::tailConfig)) {
        printf("[processAck] bufferEndsWith failed\n\r");
        return false;
    }

    // unsigned long command = inBuf[0] | (inBuf[1] << 8);
    unsigned long command = inBuf[2] | (inBuf[3] << 8);


    if (inBuf[4] | (inBuf[5] << 8)) {
        printf("[processAck] buffer adjustment failed\n\r");
        // return false;
    }
    // printf("[processAck] COMMAND: 0x%lx\n\r",command);
    
    switch (command) {
        case 0x1FF: // entered config mode
            isConfig = true;
            version = inBuf[6] | (inBuf[7] << 8);
            bufferSize = inBuf[8] | (inBuf[9] << 8);
            break;
            
        case 0x1FE: // exited config mode
            isConfig = false;
            break;
            
        case 0x1A5: { // MAC
              char mac_temp[18];
              for (int i = 0; i < 6; i++) {
                  MAC[i] = inBuf[i + 6];
                  snprintf(mac_temp + i*3, 4, "%02X%s", MAC[i], (i < 5) ? ":" : "");
              }
              MACstr = mac_temp; // std::string assignment handles memory
              break;
          }
          
          case 0x1A0: { // Firmware
              firmwareMajor = inBuf[9];
              firmwareMinor = inBuf[8];
              
              char fw_temp[16];
              snprintf(fw_temp, sizeof(fw_temp), "%d.%d.%02X%02X%02X", 
                      firmwareMajor, firmwareMinor, inBuf[13], inBuf[12], inBuf[11]);
              
              firmware = fw_temp; // std::string assignment handles memory
              break;
          }
            
        case 0x1AB: // Query Resolution
            fineRes = inBuf[6];
            break;
            
        case 0x1AE: // Query auxiliary control parameters
            lightControl = LightControl(inBuf[6]);
            lightThreshold = inBuf[7];
            outputControl = OutputControl(inBuf[8]);
            break;
            
        case 0x11B:
            autoStatus = AutoStatus(inBuf[6]);
            break;
            
        case 0x1A3: // Reboot
            isEnhanced = false;
            isConfig = false;
            break;
            
        case 0x161: { // Query parameters
            maxRange = inBuf[7];
            movingThresholds.setN(inBuf[8]);
            stationaryThresholds.setN(inBuf[9]);
            
            
            for (byte i = 0; i <= movingThresholds.N; i++) {
              movingThresholds.values[i] = inBuf[10 + i];
              printf("[processAck]  movingThresholds value: %d \n\r" , inBuf[10 + i]);
            }
            
            for (byte i = 0; i <= stationaryThresholds.N; i++) {
              stationaryThresholds.values[i] = inBuf[19 + i];
              printf("[processAck]  stationaryThresholds value: %d \n\r" , inBuf[19 + i]);
            }
            
            noOne_window = inBuf[28] | (inBuf[29] << 8);
            break;
        }
            
        case 0x162:
            isEnhanced = true;
            break;
            
        case 0x163:
            isEnhanced = false;
            break;
            
        case 0x164:
            if (LD2410::gateParam[9] == 0xFF) 
                LD2410::gateParam[9] = 0;
        default:
            printf("[processAck] Unrecognized command: 0x%lx\n\r", command);
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
                    printf("[readFrame] timeout1\n\r");
                return false;
            }
        }
        
        frameSize = inBuf[0] | (inBuf[1] << 8);
    }
    
    if (frameSize <= 0) {
        printf("[readFrame] frame size <=0\n\r");
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
                    printf("[readFrame] timeout2\n\r");

            // Timeout or error
            return false;
        }
    }
    
    return true;
}

esp_err_t MyLD2410::uartInit(uart_port_t uart_num ,int UART_TX ,int UART_RX , int BAUD_RATE ){
    esp_err_t err;

    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,  // Add this line
        .source_clk = UART_SCLK_DEFAULT
    };
    err = uart_param_config(uart_num, &uart_config);
    err=  uart_set_pin(uart_num, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    err = uart_driver_install(uart_num, 1024, 0, 0, NULL, 0);
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
  
    

        while ((esp_timer_get_time() / 1000) < giveUp) {
            uint8_t byte_read;
            int len;

            while ((len = uart_read_bytes(UART_NUM_1, &byte_read, 1, 0)) == 1) {
   
                headBuf[headBufI++] = byte_read;
                //  printf("head buff value: %x\n\r", headBuf[(headBufI - 1 + 4) % 4]); 
                headBufI %= 4;

                if (LD2410::bufferEndsWith((const char*)headBuf, headBufI, (const char*)LD2410::headConfig)) {
                    // printf("[sendCommand] reached\n\r");
                    return processAck();
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
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
    // uint8_t ret = sendCommand(LD2410::configDisable);
    // if(!ret){
    //     printf("sendCommand failed with error code:%d\n\r", ret);
    //     }
    
    while ((esp_timer_get_time() / 1000) < giveUp) {
        if (check() != FAIL) {
            online = true;
            break;
        }
         
        vTaskDelay(pdMS_TO_TICKS(110)); // Non-blocking delay
    }

    return online;
}

bool MyLD2410::configMode(bool enable)
{
  if (enable && !isConfig)
    return sendCommand(LD2410::configEnable);
  if (!enable && isConfig)
    return sendCommand(LD2410::configDisable);
  return false;
}


bool MyLD2410::requestBTon()
{
  if (isConfig)
    return sendCommand(LD2410::BTon);
  return configMode() && sendCommand(LD2410::BTon) && configMode(false);
}

bool MyLD2410::requestBToff()
{
  if (isConfig)
    return sendCommand(LD2410::BToff);
  return configMode() && sendCommand(LD2410::BToff) && configMode(false);
}

bool MyLD2410::requestReboot()
{
  if (isConfig)
    return sendCommand(LD2410::reboot);
  return configMode() && sendCommand(LD2410::reboot);
}

bool MyLD2410::requestFirmware()
{
  if (isConfig)
    return sendCommand(LD2410::firmware);
  return configMode() && sendCommand(LD2410::firmware) && configMode(false);
}

std::string MyLD2410::getFirmware()
{
  if (firmware.length() == 0)
    requestFirmware();
  return firmware;
}

bool MyLD2410::requestParameters()
{
  if (isConfig)
    return sendCommand(LD2410::param);
  return configMode() && sendCommand(LD2410::param) && configMode(false);
}

bool MyLD2410::setMaxGate(byte movingGate, byte staticGate, byte noOneWindow)
{
  if (movingGate > 8)
    movingGate = 8;
  if (staticGate > 8)
    staticGate = 8;
  byte *cmd = LD2410::maxGate;
  cmd[6] = movingGate;
  cmd[12] = staticGate;
  cmd[18] = noOneWindow;
  if (isConfig && sendCommand(cmd))
    return sendCommand(LD2410::param);
  return configMode() && sendCommand(cmd) && sendCommand(LD2410::param) && configMode(false);
}

byte MyLD2410::getMaxGate(){

  requestParameters();
  return(movingThresholds.N + stationaryThresholds.N);

} 

bool MyLD2410::setNoOneWindow(byte noOneWindow)
{
  if (!maxRange)
    requestParameters();
  if (noOne_window == noOneWindow)
    return true;
  return setMaxGate(movingThresholds.N, stationaryThresholds.N, noOneWindow);
}

byte MyLD2410::getNoOneWindow()
{
  if (!maxRange)
    requestParameters();
  return noOne_window;
}


bool MyLD2410::setResolution(bool fine)
{
  if (isConfig && sendCommand(((fine) ? LD2410::resFine : LD2410::resCoarse)))
    return sendCommand(LD2410::res);
  return configMode() && sendCommand(((fine) ? LD2410::resFine : LD2410::resCoarse)) && sendCommand(LD2410::res) && configMode(false);
}

byte MyLD2410::getResolution()
{
  if (fineRes >= 0)
    return ((fineRes == 1) ? 20 : 75);
  if (isConfig)
  {
    if (sendCommand(LD2410::res))
      return getResolution();
  }
  else
  {
    if (configMode() && sendCommand(LD2410::res) && configMode(false))
      return getResolution();
  }
  return 0;
}

byte MyLD2410::getRange()
{
  if (!maxRange)
    requestParameters();
  
  return maxRange;
}
  
unsigned long MyLD2410::getRange_cm()
{
    return (getRange() + 1) * getResolution();
}
  
uint16_t MyLD2410::getConfiguredRange(){
    
    requestParameters();
    return((getMaxGate())*(getResolution()));

};


bool MyLD2410::requestResolution()
{
  if (isConfig)
    return sendCommand(LD2410::res);
  return configMode() && sendCommand(LD2410::res) && configMode(false);
}


bool MyLD2410::setGateParameters(byte gate, byte movingThreshold, byte stationaryThreshold)
{
  if (movingThreshold > 100)
    movingThreshold = 100;
  if (stationaryThreshold > 100)
    stationaryThreshold = 100;
  byte *cmd = LD2410::gateParam;
  if (gate > 8)
  {
    cmd[6] = 0xFF;
    cmd[7] = 0xFF;
  }
  else
  {
    cmd[6] = gate;
    cmd[7] = 0;
  }
  cmd[12] = movingThreshold;
  cmd[18] = stationaryThreshold;
  if (isConfig && sendCommand(cmd))
    return sendCommand(LD2410::param);
  return configMode() && sendCommand(cmd) && sendCommand(LD2410::param) && configMode(false);
}


const MyLD2410::ValuesArray &MyLD2410::getMovingThresholds()
{
  if (!maxRange)
    requestParameters();
  return movingThresholds;
}

const MyLD2410::ValuesArray &MyLD2410::getStationaryThresholds()
{
  if (!maxRange)
    requestParameters();
  return stationaryThresholds;
}