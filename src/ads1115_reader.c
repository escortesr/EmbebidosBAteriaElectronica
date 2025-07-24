#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <time.h> 

// ADS1115 default I2C address
#define DEFAULT_ADS1115_ADDRESS 0x48

// Configuration settings
#define ADS1115_CONFIG_OS_SINGLE      0x8000
#define ADS1115_CONFIG_MUX_SINGLE     0x4000  // 0100 0000 0000 0000 + ch << 12
#define ADS1115_CONFIG_MODE_SINGLE    0x0100  // Single-shot mode
#define ADS1115_CONFIG_DR_860SPS      0x00E0  // 860 samples per second
#define ADS1115_CONFIG_CQUE_NONE      0x0003  // Disable comparator

int ADS1115_init(){
    int file;
    char *filename = "/dev/i2c-3";

    if ((file = open(filename, O_RDWR)) < 0) {
        perror("Failed to open the i2c bus");
        return -1;
    }
    return file;
}

int ADS1115_exit(int file){
    printf("Ending communication i2c device \n");
    close(file);
    return 0;
}

int ADS1115_start_reading(int channel, int file){
    // Configure the ADC
    uint8_t config[3] = {0};

    int addr = DEFAULT_ADS1115_ADDRESS;

    if ((channel<0)||(channel>7)){
        printf("Invalid channel. Must be between 0 and 7. Reading from channel 0 \n");
        channel = 0;
    }else if (channel>3){
        channel -= 4;
        addr += 1;
    }

    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        printf("Failed to communicate with device at address 0x%02x\n", addr);
        perror("\n");
        return 1;
    }

    uint16_t config_value = ADS1115_CONFIG_OS_SINGLE |
                            ADS1115_CONFIG_MUX_SINGLE  | 
                            ADS1115_CONFIG_MODE_SINGLE | 
                            ADS1115_CONFIG_DR_860SPS | 
                            ADS1115_CONFIG_CQUE_NONE |
                            (channel << 12);
    
    config[0] = 1; // Point to the config register
    config[1] = config_value >> 8;   // Configuration High Byte
    config[2] = config_value & 0xFF; // Configuration Low Byte
    
    //printf("Trying to read from the channel %i. [CONF] = %04x. \n", channel,config_value);

    if (write(file, config, 3) != 3) {
        perror("Failed to write to the i2c device");
        ADS1115_exit(file);
        return 1;
    }
    return 0;
}

int16_t ADS1115_get_result(int channel, int file){

    int addr = DEFAULT_ADS1115_ADDRESS;

    if ((channel<0)||(channel>7)){
        printf("Invalid channel. Must be between 0 and 7. Reading from channel 0 \n");
        channel = 0;
    }else if (channel>3){
        channel -= 4;
        addr += 1;
    }

    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        printf("Failed to communicate with device at address %02x", addr);
        perror("\n");
        return 1;
    }

    clock_t start_time = clock();
    while (1) {

        uint8_t config_reg = 1; // Point to config register
        if (write(file, &config_reg, 1) != 1) {
            perror("Failed to set config register pointer");
            return -1;
        }
        
        // Read config register to check OS bit

        uint8_t config_status[2];
        if (read(file, config_status, 2) != 2) {
            perror("Config read failed");
            return -1;
        }

        // Check OS bit (bit 15)
        if (config_status[0] & 0x80) { // MSB of config_status[0] = OS bit
            break; // Conversion complete
        }

        // Timeout after 100 ms
        if ((clock() - start_time) * 1000 / CLOCKS_PER_SEC > 100) {
            perror("Timeout waiting for conversion");
            return -1;
        }
        usleep(100); // Small delay to avoid busy-waiting
    }
        
    uint8_t reg = 0;
    if (write(file, &reg, 1) != 1) {
        perror("Failed to set register pointer");
        close(file);
        return -1;
    }
    
    uint8_t data[2];
    if (read(file, data, 2) != 2) {
        perror("Failed to read from the i2c device");
        close(file);
        return -1;
    }
    
    // Convert the result to 16 bits
    int16_t value = (data[0] << 8) | data[1];

    //printf("Raw value: %d\n", value);

    return value;
}

int16_t ADS1115_read(int channel, int file){
    // Configure the ADC
    uint8_t config[3] = {0};

    int addr = DEFAULT_ADS1115_ADDRESS;

    if ((channel<0)||(channel>7)){
        printf("Invalid channel. Must be between 0 and 7. Reading from channel 0 \n");
        channel = 0;
    }else if (channel>3){
        channel -= 4;
        addr += 1;
    }

    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        printf("Failed to communicate with device at address %02x", addr);
        perror("\n");
        return -1;
    }

    uint16_t config_value = ADS1115_CONFIG_OS_SINGLE |
                            ADS1115_CONFIG_MUX_SINGLE  | 
                            ADS1115_CONFIG_MODE_SINGLE | 
                            ADS1115_CONFIG_DR_860SPS | 
                            ADS1115_CONFIG_CQUE_NONE |
                            (channel << 12);
    
    config[0] = 1; // Point to the config register
    config[1] = config_value >> 8;   // Configuration High Byte
    config[2] = config_value & 0xFF; // Configuration Low Byte
    
    //printf("Trying to read from the channel %i. [CONF] = %04x. \n", channel,config_value);

    if (write(file, config, 3) != 3) {
        perror("Failed to write to the i2c device");
        ADS1115_exit(file);
        return -1;
    }
    // Read the conversion result
    
    clock_t start_time = clock();
    while (1) {
        // Read config register to check OS bit

        uint8_t config_reg = 1; // Point to config register
        if (write(file, &config_reg, 1) != 1) {
            perror("Failed to set config register pointer");
            return -1;
        }

        uint8_t config_status[2];
        if (read(file, config_status, 2) != 2) {
            perror("Config read failed");
            return -1;
        }

        // Check OS bit (bit 15)
        if (config_status[0] & 0x80) { // MSB of config_status[0] = OS bit
            break; // Conversion complete
        }

        // Timeout after 100 ms
        if ((clock() - start_time) * 1000 / CLOCKS_PER_SEC > 100) {
            perror("Timeout waiting for conversion");
            return -1;
        }
        // usleep(100); // Small delay to avoid busy-waiting
    }
        
    uint8_t reg = 0;
    if (write(file, &reg, 1) != 1) {
        perror("Failed to set register pointer");
        close(file);
        return -1;
    }
    
    uint8_t data[2];
    if (read(file, data, 2) != 2) {
        perror("Failed to read from the i2c device");
        close(file);
        return -1;
    }
    
    // Convert the result to 16 bits
    int16_t value = (data[0] << 8) | data[1];

    //printf("Raw value: %d\n", value);

    return value;
}

/*
int main(int argc, char **argv) {
    //init the i2c interface
    int file = ADS1115_init(0);

    if(file < 0)
        return 1;


    //Read from the module
    int channel = 0;
    if (argc > 1)
        channel = atoi(argv[1]);

    uint16_t value = ADS1115_read(channel,file);

    // Convert to voltage (assuming -6.144V range)
    float voltage = value * 6.144/ 32768.0;
    
    printf("Voltage: %.4f V\n", voltage);
    
    ADS1115_exit(file);
    return 0;
}    
*/

    