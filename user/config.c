// This file was written by Lukasz Piatkowski
// [piontec -at- the well known google mail]
// and is distributed under GPL v2.0 license
// repo: https://github.com/piontec/esp_rest

#include "config.h"
#include "user_config.h"
#include "osapi.h"
#include "common.h"
#include "user_interface.h"
#include "config_store.h"

#define CFG_PROMPT "config# "

static struct espconn espConn;
static esp_tcp espTcp;
static config_t* s_conf;
static struct station_config station_mode_cfg;


static void ICACHE_FLASH_ATTR printAndSend(struct espconn *conn, char *msg)
{
    debug_print (msg);
    espconn_sent (conn, msg, os_strlen (msg));
}

static void ICACHE_FLASH_ATTR cfgSentCb(void *arg)
{
	debug_print("Send callback\n");
}

static void ICACHE_FLASH_ATTR processWifiGet(struct espconn *conn)
{
    char msg [256];
    os_sprintf (msg, "Current wifi config: ssid = %s, key = %s\n",
    		station_mode_cfg.ssid, station_mode_cfg.password);
    printAndSend (conn, msg);
}

static void ICACHE_FLASH_ATTR processIntervalGet(struct espconn *conn)
{
	s_conf = config_get();

    char msg [32];
    os_sprintf (msg, "Current interval %d s\n", s_conf->interval_sec);
    printAndSend (conn, msg);
}

static void ICACHE_FLASH_ATTR setClientMode()
{
    debug_print ("Setting new station mode config\n");
    wifi_set_opmode(STATION_MODE);
    station_mode_cfg.bssid_set = 0;
    debug_print ("SSID: %s, KEY: %s, bssid_set: %d\n", station_mode_cfg.ssid,
    		station_mode_cfg.password, station_mode_cfg.bssid_set);
    wifi_station_set_config(&station_mode_cfg);
    uint8 status =wifi_station_get_connect_status();
    debug_print("Status: %d\n", status);
}

static void ICACHE_FLASH_ATTR processWifiSet(struct espconn *conn, char *buf, unsigned short len)
{
    debug_print ("Parsing new wifi configuration\n");
    //char newBuf [128];
    //os_strncpy (newBuf, buf + 5, 127);
    //newBuf [127] = 0;
    char* newBuf = buf + 5;
    newBuf [len - 5] = 0;
    debug_print ("newBuf: %s\n", newBuf);
    char *sep = (char *) os_strstr (newBuf, " ");
    if (sep == NULL)
    {
        os_printf ("ERROR: wrong set wifi\n");
        return;
    }
    debug_print ("sep_ptr: %p, newbuf_ptr: %p\n", sep, newBuf);
    char ssid [32];
    char key [64];
    os_strncpy (ssid, newBuf, sep - newBuf);
    ssid [sep - newBuf] = 0;
    debug_print ("new ssid: %s\n", ssid);
    int keyLen = os_strlen (sep + 1);
    os_strncpy (key, sep + 1, keyLen - 1);
    key [keyLen - 1] = 0;
    debug_print ("new key: %s\n", key);

    os_strncpy (&station_mode_cfg.ssid, ssid, 32);
    os_strncpy (&station_mode_cfg.password, key, 64);
    station_mode_cfg.bssid_set = 0;
}

static void ICACHE_FLASH_ATTR processIntervalSet(struct espconn *conn, char *buf, unsigned short len)
{
	char cmd_len = 9;
    debug_print ("Parsing new interval\n");
    //char newBuf [128];
    //os_strncpy (newBuf, buf + 5, 127);
    //newBuf [127] = 0;
    char* newBuf = buf + cmd_len;
    newBuf [len - cmd_len] = 0;
    debug_print ("newBuf: %s\n", newBuf);
    int interval = atoi(newBuf);
    debug_print("New interval is %d\n", interval);
    s_conf->interval_sec = interval;
}

static void ICACHE_FLASH_ATTR processGetReq(struct espconn *conn, char *buf, unsigned short len)
{
    debug_print ("Got new get request\n");
    if (((char *) os_strstr(buf, "wifi")) == buf)
        processWifiGet (conn);
    else if (((char *) os_strstr(buf, "interval")) == buf)
            processIntervalGet (conn);
    else
        os_printf ("ERROR: unknown get command: %s\n", buf);
}

static void ICACHE_FLASH_ATTR processSetReq(struct espconn *conn, char *buf, unsigned short len)
{
    debug_print ("Got new set request\n");
    if (((char *) os_strstr(buf, "wifi")) == buf)
        processWifiSet (conn, buf, len);
    else if (((char *) os_strstr(buf, "interval")) == buf)
        processIntervalSet (conn, buf, len);
    else
        os_printf ("ERROR: unknown set command: %s\n", buf);
}

void processRestart(struct espconn* conn)
{
	printAndSend(conn, "Saving configuration...");
	s_conf->boot_config = 0;
	config_save();
	printAndSend(conn, "Restarting in STA mode...\n");
	setClientMode();
	system_restart();
}

static void ICACHE_FLASH_ATTR cfgRecvCb(void *arg, char *data, unsigned short len)
{
    char buf[128];
    debug_print ("Got new config connection, len: %d\n", os_strlen(data));
    debug_print ("%s\n", data);
    struct espconn *conn=arg;
    os_strncpy(buf, data + 4, 128);
    buf[127] = 0;

    if (os_strncmp (data, "restart", 7) == 0)
    	processRestart(conn);
    else if (os_strncmp (data, "get ", 4) == 0)
        processGetReq(conn, buf, os_strlen(buf));
    else if (os_strncmp (data, "set ", 4) == 0)
        processSetReq(conn, buf, os_strlen(buf));
    else
        os_printf("ERROR: unknown command: %s\n", data);
    printAndSend (conn, CFG_PROMPT);
}

static void ICACHE_FLASH_ATTR cfgDisconCb(void *arg)
{
    os_printf ("Disconnect - closing config connection\n");
}

static void ICACHE_FLASH_ATTR cfgReconCb(void *arg, sint8 err)
{
    os_printf ("Reconnect callback - no idea when it is called\n");
}

static void ICACHE_FLASH_ATTR configConnectCb(void *arg)
{
    os_printf ("Received new config connection\n");

    struct espconn *conn=arg;

    printAndSend (conn, CFG_PROMPT);
    espconn_regist_recvcb(conn, cfgRecvCb);
    espconn_regist_reconcb(conn, cfgReconCb);
    espconn_regist_disconcb(conn, cfgDisconCb);
    espconn_regist_sentcb(conn, cfgSentCb);

}


void ICACHE_FLASH_ATTR config_mode_start()
{
    espConn.type=ESPCONN_TCP;
    espConn.state=ESPCONN_NONE;
    espTcp.local_port=CONF_PORT;
    espConn.proto.tcp=&espTcp;
    s_conf = config_get();
    wifi_station_get_config (&station_mode_cfg);

    os_printf("TCP config server started on port %d\n", CONF_PORT);
    espconn_regist_connectcb(&espConn, configConnectCb);
    espconn_accept(&espConn);
}

