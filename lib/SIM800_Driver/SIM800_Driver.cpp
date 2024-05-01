#include "SIM800_Driver.h"

SIM800_Driver::SIM800_Driver(uart_port_t uart_num, int tx_pin, int rx_pin, const char *APN) : _uart_num(uart_num)
{
    if (uart_is_driver_installed(uart_num) != true)
    {
        uart_config_t config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT
        };
        ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(uart_num, (const uart_config_t *)&config));
        ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, rx_pin, -1, -1));
    }
    strcpy(_APN, APN);
}

int SIM800_Driver::init_http()
{
    //fetch baud rate
    send_command("AT");
    send_command("AT-IPR");

    send_command("AT+CIPSHUT");//close or turn off network connection
    send_command("AT+SAPBR=0,1");   //terminates GPRS carrier.
    vTaskDelay(1000 / portTICK_PERIOD_MS);


    send_command("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");   //enables GPRS, use "CSD" for calling
    
    char APN_BUFF[50];
    sprintf(APN_BUFF, "AT+SAPBR=3,1,\"APN\",\"%s\"",_APN);  
    send_command(APN_BUFF);  //sets APN
    
    send_command("AT+SAPBR=1,1"); // Opens the carrier with the previously defined parameters. 
    send_command("AT+SAPBR=2,1"); // Queries the status of the previously opened GPRS carrier for diagnosing.
    send_command("AT+HTTPINIT");    // initializes http
    return 0;
}

int SIM800_Driver::http_get(const char *url)
{
    int ret;

    send_command("AT+HTTPPARA=\"CID\",1");  
    
    sprintf(tx_buff, "AT+HTTPPARA=URL,\"\"", url);
    send_command(tx_buff);

    send_command("AT+HTTPACTION=0");//send http request to specified URL, GET session start

    ret = get_status(15000);

    return ret;
}

int SIM800_Driver::http_post(const char *url, char *body)
{
    int timeout_ms = 15000, ret;
    send_command("AT+HTTPPARA=\"CID\",1");  
    
    sprintf(tx_buff, "AT+HTTPPARA=URL,\"\"", url);
    send_command(tx_buff);

    send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
    sprintf(tx_buff, "AT+HTTPDATA=%d,%d",strlen(body), timeout_ms);
    send_command(tx_buff);

    if (strstr(rx_buff, "DOWNLOAD"))
    {
        send_command(body);
        send_command("AT+HTTPACTION=1");//send http request to specified URL, GET session start
    }
    ret = get_status(timeout_ms);
    return ret;
}

char *SIM800_Driver::get_response()
{
    return rx_buff;
}

void SIM800_Driver::deinit()
{
    send_command("AT+CIPSHUT");//close or turn off network connection
    send_command("AT+SAPBR=0,1");// close GPRS context bearer
}

void SIM800_Driver::send_command(const char *data)
{
    int data_size;

    
    data_size = uart_write_bytes(_uart_num, data, strlen(data));
    ESP_LOGI(log_tag, "send to gsm: %s | len %d", tx_buff, data_size);
    
    data_size = uart_read_bytes(_uart_num, tx_buff, TX_BUFF_SIZE, (5000 / portTICK_PERIOD_MS));
    
    ESP_LOGI(log_tag, "gsm response: %s | len %d", rx_buff, data_size);
}

int SIM800_Driver::get_status(int ms)
{
    int start_time_ms = esp_timer_get_time() / 1000;
    char mode_str[5], data_cursor;
    int mode, status, finished, remain;
    int ret = 0;
    while(ms + start_time_ms  > esp_timer_get_time() / 1000)
    {
        send_command("AT+HTTPSTATUS?");

        if (strstr(rx_buff, "+HTTPACTION:"))
        {
            sscanf(strstr(rx_buff, "+HTTPACTION:"), "+HTTPACTION: %d, %d, %d", mode, status, remain);
            ESP_LOGI(log_tag, "transfer done: http %s | return %d | datalen %d", mode == 0 ? "GET" : "POST", status, remain);
            ret = status;
            break;
        }
        else if (strstr(rx_buff, "+HTTPSTATUS:"))
        {
            sscanf(strstr(rx_buff, "+HTTPSTATUS:"), "+HTTPSTATUS: %5s, %d, %d, %d", mode, status, finished, remain);
            ESP_LOGI(log_tag, "transfer stats: http: %s | status: %d | finished: %d | remaining: %d", mode, status, finished, remain);
            if (status == 0) // idle
            {
                ret = 0;
                break;
            }
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
    if (ms < start_time_ms)
    {
        ESP_LOGI(log_tag, "FAILED HTTP REQUEST");
        ret = -1;
    }
    send_command("AT+HTTPREAD");// read results of request, normally contains status code 200 if successful
    sscanf(rx_buff, "+HTTPREAD: %d %1024s", finished, rx_buff);
    ESP_LOGI(log_tag, "read byte: %d | data: %s\n", finished, rx_buff);
    
    send_command("AT+HTTPTERM");    //Terminate HTTP
    return ret;
}
