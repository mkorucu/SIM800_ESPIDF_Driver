#include <SIM800_Driver.h>
extern "C" { void app_main();}

void app_main()
{
    SIM800_Driver gsm(UART_NUM_0, GPIO_NUM_10, GPIO_NUM_10, "internet");
    gsm.init_http();
    gsm.http_post("http://google.com", "GET \\ HTTP/1.1\r\nHost:google.com\r\n\r\n");
    printf("resp: %s\n", gsm.get_response());
}