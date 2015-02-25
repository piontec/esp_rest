// This file was written by Lukasz Piatkowski
// [piontec -at- the well known google mail]
// and is distributed under GPL v2.0 license
// repo: https://github.com/piontec/esp_rest

#include "common.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "private_user_config.h"
#include "dht22.h"
#include "uart_hw.h"
#include "c_types.h"
#include "mem.h"
#include "ip_addr.h"
#include "espconn.h"
#include "config_store.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1


os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

static volatile ETSTimer sensor_timer;
static volatile ETSTimer resetBtntimer;
int lastTemp, lastHum;
char lastTempTxt [8];
char lastHumTxt [8];
char payload[512];
static int resetCnt=0;
static int totalCnt=0;
static config_t* s_conf;

static void ICACHE_FLASH_ATTR enable_sensors()
{
    //Set LEDGPIO to HIGH
    gpio_output_set((1<<LEDGPIO), 0, (1<<LEDGPIO), 0);
    debug_print ("sensors enabled\n");
}


static void ICACHE_FLASH_ATTR disable_sensors()
{
    //Set LEDGPIO to LOW
    gpio_output_set(0, (1<<LEDGPIO), (1<<LEDGPIO), 0);
    debug_print ("sensors disabled\n");
}

static void ICACHE_FLASH_ATTR convertToText (int dhtReading, char *buf, uint8 maxLength, uint8 decimalPlaces)
{    
    if (dhtReading == 0)
        strncpy(buf, "0.00", 4);
    char tmp [8];
    os_bzero (tmp, 8);
    os_sprintf (tmp, "%d", dhtReading);

    uint8 len = os_strlen (tmp);
    uint8 beforeSep = len - decimalPlaces;
    os_strncpy(buf, tmp, beforeSep);
    buf [beforeSep] = '.';
    os_strncpy (buf + beforeSep + 1, tmp + beforeSep, decimalPlaces);
    buf [len + 1] = 0;
}

static void ICACHE_FLASH_ATTR read_DHT22()
{
    int retry = 0;
    float *r;
    do {
        if (retry++ > 0)
        {
        	debug_print("DHT22 read fail, retrying, try %d/%d\n", retry, MAX_DHT_READ_RETRY);
            os_delay_us(DHT_READ_RETRY_DELAY_US);
        }
        r = readDHT();
    }
    while ((r[0] == 0 && r[1] == 0) && retry < MAX_DHT_READ_RETRY);
    debug_print("DHT read done\n");
    lastTemp=(int)(r[0] * 100);
    lastHum=(int)(r[1] * 100);    
    convertToText (lastTemp, lastTempTxt, 8, 2);    
    convertToText (lastHum, lastHumTxt, 8, 2);
    debug_print ("Temp = %s *C, Hum = %s %\n", lastTempTxt, lastHumTxt);
}

static void ICACHE_FLASH_ATTR at_tcpclient_sent_cb(void *arg)
{
    debug_print("TCP sent callback\n");
    //struct espconn *pespconn = (struct espconn *)arg;
    //espconn_disconnect(pespconn);
    // disable sensors power
    //disable_sensors();
    // goto sleep ; gpio16 -> RST -- requires soldiering on ESP-03

    //debug_print ("going to deep sleep for %ds\n", INTERVAL_S);
    //system_deep_sleep(INTERVAL_S*1000*1000);
}

static void ICACHE_FLASH_ATTR at_tcpclient_discon_cb(void *arg) {
    debug_print("TCP disconnect callback\n");
    struct espconn *pespconn = (struct espconn *)arg;
    os_free(pespconn->proto.tcp);

    os_free(pespconn);
   
    debug_print ("going to deep sleep for %ds\n", INTERVAL_S);
    system_deep_sleep(INTERVAL_S*1000*1000);
}


static void ICACHE_FLASH_ATTR at_tcpclient_reconnect_cb(void *arg, sint8 errType)
{
    struct espconn *pespconn = (struct espconn *) arg;
    debug_print("Reconnect callback - resending with delay...\n");
    os_delay_us(1*1000*1000);
    debug_print ("now.\n");
    espconn_sent(pespconn, payload, strlen(payload));
    debug_print("resend done.\n");
}

static void ICACHE_FLASH_ATTR at_tcpclient_read_cb(void *arg, char *data, unsigned short len) 
{
    char buf[128];
    debug_print("Rcv callback: %d\n", os_strlen(data));
    debug_print("%s\n", data);
    struct espconn *conn = arg;
    espconn_disconnect(conn);    
}

static void ICACHE_FLASH_ATTR at_tcpclient_connect_cb(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;
    
    debug_print("TCP client connect\n");

    espconn_regist_sentcb(pespconn, at_tcpclient_sent_cb);
    espconn_regist_disconcb(pespconn, at_tcpclient_discon_cb);
    espconn_regist_recvcb(pespconn, at_tcpclient_read_cb);
    os_sprintf(payload, "GET http://api.thingspeak.com/update?api_key=%s&field1=%s&field2=%s HTTP/1.1\r\nHost: api.thingspeak.com\r\nUser-agent: the best\r\nConnection: close\r\n\r\n", THINGSPEAK_KEY, lastTempTxt, lastHumTxt);
    espconn_sent(pespconn, payload, strlen(payload));
}


static void ICACHE_FLASH_ATTR send_data()
{
    struct espconn *pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (pCon == NULL)
    {
        os_printf("Error: TCP connect failed - memory allocation for conn failed\n");
        return;
    }
    pCon->type = ESPCONN_TCP;
    pCon->state = ESPCONN_NONE;
    uint32_t ip = ipaddr_addr(REMOTE_IP);
    pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    if (pCon->proto.tcp == NULL)
    {
        os_printf("Error: TCP connect failed - memory allocation for TCP failed\n");
        return;
    }
    pCon->proto.tcp->local_port = espconn_port();
    pCon->proto.tcp->remote_port = 80;
    //pCon->proto.tcp->remote_port = lwip_htons(80);

    os_memcpy(pCon->proto.tcp->remote_ip, &ip, 4);

    espconn_regist_connectcb(pCon, at_tcpclient_connect_cb);
    espconn_regist_reconcb(pCon, at_tcpclient_reconnect_cb);
    debug_print("TCP connecting...\n");
    espconn_connect(pCon);
}

static void ICACHE_FLASH_ATTR sensor_timer_func(void *arg)
{
    // enable sensors power
    enable_sensors();
    // sleep and wait for sensors
    os_delay_us(SENSORS_READY_WAIT_US);
    // readDHT
    read_DHT22();
    // read pressure
    // disable sensors
    disable_sensors ();
    // send data
    send_data();
}


//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
    os_delay_us(10);
    debug_print("In user_procTask\n");
}

static void ICACHE_FLASH_ATTR initialize_timer()
{
    os_timer_disarm(&sensor_timer);
    os_timer_setfn(&sensor_timer, (os_timer_func_t *)sensor_timer_func, NULL);
    os_timer_arm(&sensor_timer, 5000, 0);
}


static void ICACHE_FLASH_ATTR initialize_gpio()
{
    // Initialize the GPIO subsystem.
    gpio_init();

    //Set GPIO12 to input mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);
    //PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDI_U);
    gpio_output_set(0, 0, 0, (1<<BTNGPIO));

    //Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    //Set GPIO2 low
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);
    gpio_output_set(0, BIT2, BIT2, 0);

    //Set GPIO13 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
    //Set GPIO13 low
    gpio_output_set(0, (1<<LEDGPIO), (1<<LEDGPIO), 0);
}

static void ICACHE_FLASH_ATTR initialize_uart()
{
    //Set baud rate and other serial parameters to 115200,n,8,1
    uart_div_modify(0, UART_CLK_FREQ/BIT_RATE_115200);
    WRITE_PERI_REG(UART_CONF0(0), (STICK_PARITY_DIS)|(ONE_STOP_BIT << UART_STOP_BIT_NUM_S)| \
                   (EIGHT_BITS << UART_BIT_NUM_S));
    //Reset tx & rx fifo
    SET_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST|UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST|UART_TXFIFO_RST);
    //Clear pending interrupts
    WRITE_PERI_REG(UART_INT_CLR(0), 0xffff);
}

static void ICACHE_FLASH_ATTR config_mode()
{
    os_printf ("Starting config mode\n");
    config_mode_start();
}


static void ICACHE_FLASH_ATTR normal_mode()
{
    os_printf ("Starting normal mode\n");
    DHTInit();
    debug_print ("DHT22 ready\n");
    initialize_timer();
    debug_print ("Timer ready\n");
    os_printf ("Initialization complete, starting main task\n");
    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}

static void ICACHE_FLASH_ATTR boot()
{
	if (s_conf->boot_config == 1)
	{
		if (wifi_get_opmode() == STATION_MODE)
		{
			wifi_set_opmode(CFG_WIFI_MODE);
			system_restart();
		}
		else
			config_mode ();
	}
    else
        normal_mode ();
}


static void ICACHE_FLASH_ATTR resetBtnTimerCb(void *arg)
{
    totalCnt++;
    if (!GPIO_INPUT_GET(BTNGPIO))
        resetCnt++;
    debug_print ("reset counter %d/%d\n", resetCnt, totalCnt);

    if (totalCnt < 2)
        return;

    os_timer_disarm(&resetBtntimer);

    //int currentMode = wifi_get_opmode();
    if (resetCnt>=2)
    {
    	s_conf->boot_config = 1;
    	config_save();
    	os_printf("Restarting system into config mode...\n");
        wifi_station_disconnect();
        wifi_set_opmode(CFG_WIFI_MODE);
    }
    else
    {
        boot();
    }
    resetCnt=0;
    totalCnt=0;
}

static void ICACHE_FLASH_ATTR resetInit()
{
    initialize_gpio();
    s_conf = config_init();
    os_printf("\n\nSoftware version: %s, config version: %d\n", SOFT_VERSION, CONFIG_VERSION);
    debug_print("Current wifi connect status: %d\n", wifi_station_get_connect_status());
    debug_print("Current DHCP status: %d\n", wifi_station_dhcpc_status());
    debug_print("Current wifi mode: %d\n", wifi_get_phy_mode());

    os_timer_disarm(&resetBtntimer);
    os_timer_setfn(&resetBtntimer, resetBtnTimerCb, NULL);
    os_timer_arm(&resetBtntimer, 500, 1);
}

void ICACHE_FLASH_ATTR init_done()
{
    debug_print("INIT IS DONE!\n");
    resetInit();
}

//Init function
void ICACHE_FLASH_ATTR user_init()
{
    initialize_uart();
    system_init_done_cb(init_done); 
    //resetInit ();
}
