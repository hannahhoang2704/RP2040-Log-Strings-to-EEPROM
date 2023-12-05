
/*
STORE LOG STRINGS IN EEPROM
Improve Exercise 1 by adding a persistent log that stores messages to EEPROM. When the program starts it writes “Boot” to
the log and every time when state or LEDs is changed the state change message, as described in Exercise 1, is also written to the log.
The log must have the following properties:
Log starts from address 0 in the EEPROM.
First two kilobytes (2048 bytes) of EEPROM are used for the log.
Log is persistent, after power up writing to log continues from location where it left off last time.
The log is erased only when it is full or by user command.
Each log entry is reserved 64 bytes.
First entry is at address 0, second at address 64, third at address 128, etc.
Log can contain up to 32 entries.
A log entry consists of a string that contains maximum 61 characters, a terminating null character (zero) and two-byte CRC that is used to validate the integrity of the data.
A maximum length log entry uses all 64 bytes. A shorter entry will not use all reserved bytes. The string must contain at least one character.
When a log entry is written to the log, the string is written to the log including the terminating zero. Immediately after the terminating zero follows a 16-bit CRC, MSB first followed by LSB.
Entry is written to the first unused (invalid) location that is available.
If the log is full then the log is erased first and then entry is written to address 0.
User can read the content of the log by typing read and pressing enter.
Program starts reading and validating log entries starting from address zero. If a valid string is found it is printed and program reads string from the next location.
A string is valid if the first character is not zero, there is a zero in the string before index 62, and the string passes the CRC validation.
Printing stops when an invalid string is encountered or the end log are is reached.
User can erase the log by typing erase and pressing enter.
Erasing is done by writing a zero at the first byte of every log entry.*/

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

//define I2C pins
#define I2C0_SDA_PIN 16
#define I2C0_SCL_PIN 17

//define LED pins
#define LED1 22
#define LED2 21
#define LED3 20

//define Buttons
#define SW_0 9
#define SW_1 8
#define SW_2 7

//EEPROME address
#define DEVADDR 0x50
#define BAUDRATE 100000
#define I2C_MEMORY_SIZE 32768

#define ENTRY_SIZE 64
#define MAX_ENTRIES 32
#define STRLEN 62
#define FIRST_ADDRESS 0


typedef struct ledstate{
    uint8_t state;
    uint8_t not_state;
} ledstate;

void initializePins(void);
void write_to_eeprom(uint16_t memory_address, uint8_t *data, size_t length);
void read_from_eeprom(uint16_t memory_address, uint8_t *data_read, size_t length);
void set_led_state(ledstate *ls, uint8_t value);
bool led_state_is_valid(ledstate *ls);
bool pressed(uint button);
uint16_t crc16(const uint8_t *data_p, size_t length);
void write_log_entry(char *str, uint8_t *index, uint16_t index_address);

void read_log_entry(uint8_t *index);
void erase_logs(uint8_t *current_index, uint16_t index_address);
//void printLastThreeBits(uint8_t byte);


int main() {

    initializePins();

    ledstate ls;
    uint8_t stored_led_state = 0;
    uint8_t stored_led_not_state = 0;

    const uint16_t led_state_address = I2C_MEMORY_SIZE-1; //highest address
    const uint16_t led_not_state_address = I2C_MEMORY_SIZE-2;
    const uint16_t stored_entry_index_address = I2C_MEMORY_SIZE-3;

    uint8_t current_entry_index=0;

    //write first Boot log to memory
    read_from_eeprom(stored_entry_index_address, &current_entry_index, 1);
    char boot_log[] = "Boot";
    printf("%s\n", boot_log);
    write_log_entry(boot_log, &current_entry_index, stored_entry_index_address);

    //check led state in memory
    read_from_eeprom(led_state_address, &stored_led_state,1);
    read_from_eeprom(led_not_state_address, &stored_led_not_state,1);
    ls.state = stored_led_state;
    ls.not_state = stored_led_not_state;

    if(!led_state_is_valid(&ls)){
        set_led_state(&ls, 0x02); //middle led is 0x02 (binary : 00000010)
        write_to_eeprom(led_state_address, &ls.state,1);
        write_to_eeprom(led_not_state_address, &ls.not_state,1);
        gpio_put(LED2, ls.state & 0x02);
        sleep_ms(100);
    }else{
        gpio_put(LED3, ls.state & 0x01);
        gpio_put(LED2, ls.state & 0x02);
        gpio_put(LED1, ls.state & 0x04);
    }

    char input_command1[STRLEN];
    char input_command2[STRLEN];
    char chr;
    int lp = 0;

    while(true){
        char log_message[STRLEN];
        if(!pressed(SW_0)){
            ls.state ^= 0x01;
            printf("SW_0 is pressed\n");
            gpio_put(LED3, ls.state & 0x01); //update led light
            //Write new state to EEPROM
            set_led_state(&ls, ls.state);
            write_to_eeprom(led_state_address, &ls.state,1);
            write_to_eeprom(led_not_state_address, &ls.not_state,1);
            sleep_ms(100);

            printf("Time since boot: %llu seconds, LED state: 0x%02X\n", time_us_64()/1000000, ls.state);
//            printLastThreeBits(ls.state);

            //write to log_message and log to EEPROM
            snprintf(log_message, STRLEN, "Time since boot: %llu seconds, LED state: 0x%02X", time_us_64()/1000000, ls.state);
            write_log_entry(log_message, &current_entry_index, stored_entry_index_address);

            while(!gpio_get(SW_0));
        }
        if(!pressed(SW_1)){
            ls.state ^= 0x02;
            printf("SW_1 is pressed\n");
            gpio_put(LED2, ls.state & 0x02);
            //Write new state to EEPROM
            set_led_state(&ls, ls.state);
            write_to_eeprom(led_state_address, &ls.state,1);
            write_to_eeprom(led_not_state_address, &ls.not_state,1);
            sleep_ms(100);

            printf("Time since boot: %lld seconds, LED state: 0x%02X\n", time_us_64() / 1000000, ls.state);
//            printLastThreeBits(ls.state);

            //write to log_message and log to EEPROM
            snprintf(log_message, STRLEN, "Time since boot: %llu seconds, LED state: 0x%02X", time_us_64()/1000000, ls.state);
            write_log_entry(log_message, &current_entry_index, stored_entry_index_address);

            while(!gpio_get(SW_1));
        }
        if(!pressed(SW_2)){
            ls.state ^= 0x04;
            printf("SW_2 is pressed\n");
            gpio_put(LED1, ls.state & 0x04);

            //Write new state to EEPROM
            set_led_state(&ls, ls.state);
            write_to_eeprom(led_state_address, &ls.state,1);
            write_to_eeprom(led_not_state_address, &ls.not_state,1);
            sleep_ms(100);
            printf("Time since boot: %lld seconds, LED state: 0x%02X\n", time_us_64() / 1000000, ls.state);
//            printLastThreeBits(ls.state);

            //write to log_message and log to EEPROM
            snprintf(log_message, STRLEN, "Time since boot: %llu seconds, LED state: 0x%02X", time_us_64()/1000000, ls.state);
            write_log_entry(log_message, &current_entry_index, stored_entry_index_address);
            while(!gpio_get(SW_2));
        }

        //command input
        chr = getchar_timeout_us(0);
        while(chr!=255 && lp < 8){
            input_command1[lp] = chr;
            input_command2[lp] = chr;
            if(lp==5){
                input_command2[lp] = '\0';
                if (strcmp(input_command2, "erase") == 0) {
                    erase_logs(&current_entry_index, stored_entry_index_address);
                    lp = 0;
                    break;
                }
            }
            if(lp==4) {
                input_command1[lp] = '\0';
//                printf("Received input command: %s\n", input_command1);
                if (strcmp(input_command1, "read") == 0) {
                    read_log_entry(&current_entry_index);
                    lp = 0;
                    break;
                }
            }
            lp++;
            chr = getchar_timeout_us(0);
        }
    }
    return 0;
}
void write_log_entry(char *str, uint8_t *index, uint16_t index_address){
    if(*index >= MAX_ENTRIES){
        printf("Maximum log entries. Erasing the log to log the messages\n");
        erase_logs(index, index_address);
    }
//    printf("Entry index: %d\n", *index);
    size_t size_length = strlen(str); //not include NULL terminator
    if(size_length >=1){
        if(size_length > STRLEN-1){
            size_length = STRLEN-1;
        }
    }
    uint8_t log_buf[size_length + 3];
    printf("Log message for index %d: %s\n", *index, str);

    //copy string to uint8_t array
    for(int a= 0; a < strlen(str); ++a){
        log_buf[a] = (uint8_t)str[a];
    }
    log_buf[strlen(str)] = '\0';
//    printf("copy log message to buf in uint8_t format\n");

    //add CRC to log buffer
    uint16_t crc = crc16(log_buf, size_length+1);
    log_buf[size_length+1] = (uint8_t)(crc >> 8);
    log_buf[size_length+2] = (uint8_t) crc;         //check again the size length
//    printf("CRC is %02X %02X\nAfter CRC\n", log_buf[size_length+1], log_buf[size_length+2]);
//    for(int i= 0; i<STRLEN+2; ++i){
//        printf("%02X    ", log_buf[i]);
//    }
//    printf("\n");

    //write to EEPROM
    uint16_t write_address = (uint16_t)FIRST_ADDRESS + (*index * (uint16_t)ENTRY_SIZE);
    if(write_address < ENTRY_SIZE*MAX_ENTRIES){
        write_to_eeprom(write_address, log_buf, ENTRY_SIZE);
        printf("Address to write log: %u\n", write_address);
        *index+=1;
        write_to_eeprom(index_address, &(*index), 1);
    }
}


void read_log_entry(uint8_t *index){
    printf("Reading log entry\n");
    for(int i = 0; i < *index; ++i){
        uint8_t read_buff[ENTRY_SIZE];
        uint16_t read_address = (uint16_t)FIRST_ADDRESS + (i * (uint16_t)ENTRY_SIZE);
        read_from_eeprom(read_address, (uint8_t *)&read_buff, ENTRY_SIZE);
//        printf("Raw data from EEPROM at index %d\n", i);
//        for (int k = 0; k < ENTRY_SIZE; ++k){
//            printf("%02X", read_buff[k]);
//        }
//        printf("\n");
        int term_zero_index = 0;
        while (read_buff[term_zero_index] != '\0') {
            term_zero_index++;
        }
//        printf("Terminate index: %d\n", term_zero_index);

        if (read_buff[0] != 0 && crc16(read_buff, (term_zero_index+3))==0 && term_zero_index < (ENTRY_SIZE - 2))
        {
            printf("Log entry index %d: ", i);
            int b_index = 0;
            while (read_buff[b_index]) {
                printf("%c", read_buff[b_index++]);
            }
            printf("\n");
        } else {
            printf("Invalid or empty log entry at index %d\n", i);
            break; // Stop if an invalid entry is encountered
        }
    }
    printf("\nStop\n");
}

void erase_logs(uint8_t *current_index, uint16_t index_address){
    printf("Erase the log messages\nDelete log from address:\n");
    for(int i=0; i < *current_index; ++i){
        uint16_t write_address = FIRST_ADDRESS + (i * ENTRY_SIZE);
        printf("%u  ", write_address);
        uint8_t buf[] = {00};
        write_to_eeprom(write_address, buf, ENTRY_SIZE);
    }
    printf("\n");
    *current_index = 0;
    //write current index
    write_to_eeprom(index_address, &(*current_index), 1);
}

uint16_t crc16(const uint8_t *data_p, size_t length){
    uint8_t x;
    uint16_t crc = 0xFFFF;
    while(length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc =  (crc << 8) ^ ((uint16_t) (x << 5) ^ ((uint16_t) x));
    }
    return crc;
}
void write_to_eeprom(uint16_t memory_address, uint8_t *data, size_t length){
    uint8_t buf[2+ length];
    buf[0] = (uint8_t)(memory_address >> 8); //high byte of memory address
    buf[1] = (uint8_t)(memory_address); //low byte of memory address
    for(size_t i = 0; i<length; ++i){
        buf[i+2] = data[i];
    }
    i2c_write_blocking(i2c0, DEVADDR, buf, length+2, false);
    sleep_ms(20);
}

void read_from_eeprom(uint16_t memory_address, uint8_t *data_read, size_t length){
    uint8_t buf[2+ length];
    buf[0] = (uint8_t)(memory_address >>8); //high byte of memory address
    buf[1] = (uint8_t)(memory_address); //low byte of memory address
    i2c_write_blocking(i2c0, DEVADDR, buf, 2, true);
    i2c_read_blocking(i2c0, DEVADDR, data_read, length, false);
}


void set_led_state(ledstate *ls, uint8_t value){
    ls->state = value;
    ls->not_state = ~value;
}

bool led_state_is_valid(ledstate *ls){
    return ls->state == (uint8_t) ~ls->not_state;
}

void printLastThreeBits(uint8_t byte) {
    uint8_t lastThreeBits = byte & 0x07;
    // Printing each bit
    int led_n = 0;
    for (int i = 2; i >= 0; i--) {
        printf("Led %d: %d  ", ++led_n, (lastThreeBits >> i) & 0x01);
    }
}
bool pressed(uint button) {
    int press = 0;
    int release = 0;
    while (press < 3 && release < 3) {
        if (gpio_get(button)) {
            press++;
            release = 0;
        } else {
            release++;
            press = 0;
        }
        sleep_ms(10);
    }
    if (press > release) return true;
    else return false;
}

void initializePins(void){

    // Initialize chosen serial port
    stdio_init_all();

    //Initialize I2C
    i2c_init(i2c1, BAUDRATE);
    i2c_init(i2c0, BAUDRATE);

    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);

    //Initialize led
    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);
    gpio_init(LED2);
    gpio_set_dir(LED2, GPIO_OUT);
    gpio_init(LED3);
    gpio_set_dir(LED3, GPIO_OUT);

    //Initialize buttons
    gpio_set_function(SW_0, GPIO_FUNC_SIO);
    gpio_set_dir(SW_0, GPIO_IN);
    gpio_pull_up(SW_0);

    gpio_set_function(SW_1, GPIO_FUNC_SIO);
    gpio_set_dir(SW_1, GPIO_IN);
    gpio_pull_up(SW_1);

    gpio_set_function(SW_2, GPIO_FUNC_SIO);
    gpio_set_dir(SW_2, GPIO_IN);
    gpio_pull_up(SW_2);

}