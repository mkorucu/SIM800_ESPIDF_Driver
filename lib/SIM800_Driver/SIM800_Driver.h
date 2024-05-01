#ifndef __SIM800_DRIVER_H
#define __SIM800_DRIVER_H

#include <stdint.h>
#include <time.h>
#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TX_BUFF_SIZE    1024
#define RX_BUFF_SIZE    1024
class SIM800_Driver
{
    public:
        SIM800_Driver(uart_port_t uart_num, int tx_pin, int rx_pin, const char* APN);
        int init_http();
        int http_get(const char *url);
        int http_post(const char *url, char *body);
        char *get_response();
        void deinit();
    
    private:
        void send_command(const char *data);
        int get_status(int ms);
        char tx_buff[TX_BUFF_SIZE];
        char rx_buff[RX_BUFF_SIZE];
        char _APN[32];
        uart_port_t _uart_num;
        const char *log_tag = "SIM800";
};



#endif