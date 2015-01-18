// This file was written by Lukasz Piatkowski
// [piontec -at- the well known google mail]
// and is distributed under GPL v2.0 license
// repo: https://github.com/piontec/esp_rest

#include "config.h"
#include "user_config.h"
#include "osapi.h"
#include "common.h"
#include "user_interface.h"

static struct espconn espConn;
static esp_tcp espTcp;

static void ICACHE_FLASH_ATTR cfgSentCb(void *arg)
{
    debug_print ("Send callback - writing config connection\n");
}

static void ICACHE_FLASH_ATTR printAndSend(struct espconn *conn, char *msg)
{
    debug_print (msg);
    espconn_sent (conn, msg, os_strlen (msg));
}

static void ICACHE_FLASH_ATTR processWifiGet(struct espconn *conn)
{
    static struct station_config stconf;
    char msg [256];
    wifi_station_get_config (&stconf);
    os_sprintf (msg, "Current wifi config: ssid = %s, key = %s\n", stconf.ssid, stconf.password);
    printAndSend (conn, msg);
}

static void ICACHE_FLASH_ATTR setClientMode(struct espconn *conn, char *ssid, char *key)
{
    static struct station_config stconf;
    os_strncpy (&stconf.ssid, ssid, 32);
    os_strncpy (&stconf.password, key, 64);
    stconf.bssid_set = 0;

    os_printf ("Setting new station mode config\n");
    wifi_set_opmode(STATION_MODE);
    wifi_station_set_config(&stconf);
}

static void ICACHE_FLASH_ATTR processWifiSet(struct espconn *conn, char *buf, unsigned short len)
{
    os_printf ("Parsing new wifi configuration\n");
    char newBuf [128];
    os_strncpy (newBuf, buf + 5, 127);
    newBuf [127] = 0;
    os_printf ("DEBUG: newBuf: %s\n", newBuf);
    char *sep = (char *) os_strstr (newBuf, " ");
    if (sep == NULL)
    {
        os_printf ("ERROR: wrong set wifi\n");
        return;
    }
    os_printf ("DEBUG: sep_ptr: %p, newbuf_ptr: %p\n", sep, newBuf);
    char ssid [32];
    char key [64];
    os_printf ("DEBUG: len ssid: %d\n", sep-newBuf);
    os_strncpy (ssid, newBuf, sep - newBuf);
    os_printf ("DEBUG: new ssid: %s\n", ssid);
    int keyLen = os_strlen (sep + 1);
    os_strncpy (key, sep + 1, keyLen - 1);
    key [keyLen - 1] = 0;
    os_printf ("DEBUG: new key: %s\n", key);

    os_printf ("New SSID = %s, new key = %s|\n", ssid, key);
    setClientMode (conn, ssid, key);
}

static void ICACHE_FLASH_ATTR processGetReq(struct espconn *conn, char *buf, unsigned short len)
{
    os_printf ("Got new get request\n");
    if (((char *) os_strstr(buf, "wifi")) == buf)
        processWifiGet (conn);
    else
        os_printf ("ERROR: unknown get command: %s\n", buf);
}

static void ICACHE_FLASH_ATTR processSetReq(struct espconn *conn, char *buf, unsigned short len)
{
    os_printf ("Got new set request\n");
    if (((char *) os_strstr(buf, "wifi")) == buf)
        processWifiSet (conn, buf, len);
    else
        os_printf ("ERROR: unknown set command: %s\n", buf);
}

static void ICACHE_FLASH_ATTR cfgRecvCb(void *arg, char *data, unsigned short len)
{
    char buf[128];
    os_printf ("Receive callback - reading config connection\n");
    struct espconn *conn=arg;
    os_strncpy(buf, data + 4, 128);
    buf[127] = 0;

    if (os_strncmp (data, "restart", 8) == 0)
    {
        printAndSend (conn, "Restarting...\n");
        system_restart ();
    }
    else if (os_strncmp (data, "get ", 4) == 0)
        processGetReq(conn, buf, os_strlen(buf));
    else if (os_strncmp (data, "set ", 4) == 0)
        processSetReq(conn, buf, os_strlen(buf));
    else
        os_printf("ERROR: unknown command: %s\n", data);
}

static void ICACHE_FLASH_ATTR cfgDisconCb(void *arg)
{
    os_printf ("Disconnect callback - closing config connection\n");
}

static void ICACHE_FLASH_ATTR cfgReconCb(void *arg, sint8 err)
{
    os_printf ("Reconnect callback - no idea when it is called\n");
}

static void ICACHE_FLASH_ATTR configConnectCb(void *arg)
{
    os_printf ("Connect callback - received new config connection\n");

    struct espconn *conn=arg;

    printAndSend (conn, "config# ");
    espconn_regist_recvcb(conn, cfgRecvCb);
    espconn_regist_reconcb(conn, cfgReconCb);
    espconn_regist_disconcb(conn, cfgDisconCb);
    espconn_regist_sentcb(conn, cfgSentCb);

}


void ICACHE_FLASH_ATTR configInit()
{
    espConn.type=ESPCONN_TCP;
    espConn.state=ESPCONN_NONE;
    espTcp.local_port=CONF_PORT;
    espConn.proto.tcp=&espTcp;

    os_printf("TCP config server init, conn=%p\n", &espConn);
    espconn_regist_connectcb(&espConn, configConnectCb);
    espconn_accept(&espConn);
}

