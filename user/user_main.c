/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>

//os_timer_t test_timer;
MQTT_Client mqttClient;

MQTT_Client* client_instance;

//************************************
//this is the interrupt service routine that will be executed when external interrupt 
//on esp8266 gpio is triggered 
//**********************************
void ISR_GPIO(void)
{
	 static int status=0;
     uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	 	//Clear the array register
	 GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
	 status^=1;
	 GPIO_OUTPUT_SET(2,status);
	 if(status==1)
	 {MQTT_Publish(client_instance,"/mqtt/topic/led","off",3,0,0);
	 }
	 else{
		 MQTT_Publish(client_instance,"/mqtt/topic/led","on",2,0,0);

	 }

}


//************************
//call back function that will be called 
//when esp is connected to acces point
//it will also connect to mqtt server 
//***********************
static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status)
{
  if (status == STATION_GOT_IP) {
    MQTT_Connect(&mqttClient);
  } else {
    MQTT_Disconnect(&mqttClient);
  }
}


//***********************************
//call back function that will be executed when esp 
//connects to mqtt server in this function the global interrupt is enabled so 
//esp could respond to interrupt after this function 
//***********************************
static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{  system_soft_wdt_feed();
  MQTT_Client* client = (MQTT_Client*)args;
   client_instance=client;
   ETS_GPIO_INTR_ENABLE();
  INFO("MQTT: Connected\r\n");
  //MQTT_Subscribe(client, "/mqtt/topic/0", 0);
  //MQTT_Subscribe(client, "/mqtt/topic/1", 1);
  //MQTT_Subscribe(client, "/mqtt/topic/2", 2);


   // MQTT_Publish(client, "/mqtt/topic/garage/floor1", "20-freeslots",12,0,0);
   //MQTT_Publish(client, "/mqtt/topic/1", "hello1", 6, 1, 0);
   //MQTT_Publish(client, "/mqtt/topic/2", "hello2", 6, 2, 0);

}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  client_instance=client;
  INFO("MQTT: Disconnected\r\n");
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  client_instance=client;
  INFO("MQTT: Published\r\n");
}

static void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
  char *topicBuf = (char*)os_zalloc(topic_len + 1),
        *dataBuf = (char*)os_zalloc(data_len + 1);

  MQTT_Client* client = (MQTT_Client*)args;
  client_instance=client;
  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;
  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;
  INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}

void ICACHE_FLASH_ATTR print_info()
{
  INFO("\r\n\r\n[INFO] BOOTUP...\r\n");
  INFO("[INFO] SDK: %s\r\n", system_get_sdk_version());
  INFO("[INFO] Chip ID: %08X\r\n", system_get_chip_id());
  INFO("[INFO] Memory info:\r\n");
  system_print_meminfo();

  INFO("[INFO] -------------------------------------------\n");
  INFO("[INFO] Build time: %s\n",100);
  INFO("[INFO] -------------------------------------------\n");

}
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 8;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void)
{

}

//********************************************************************
//this function sets the gpio register to make gpio pin 5 as input
//and it will also activate pull up and external interrupt for gpio 5
//then it will set the mqtt client and broker settings
//it will also register some callbacks to be called when some events occur
//function at the end will connected to your netwrok b giving your ssid and password
//******************************************************************


static void ICACHE_FLASH_ATTR app_init(void)
{  //os_timer_disarm(&test_timer);
	 system_soft_wdt_feed();
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
    print_info();


      gpio_init();
      PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U,FUNC_GPIO5);
      PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U ,FUNC_GPIO2);
     // GPIO_OUTPUT_SET(2,0);
      GPIO_DIS_OUTPUT(GPIO_ID_PIN(5));
      PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);
      ETS_GPIO_INTR_DISABLE();
      ETS_GPIO_INTR_ATTACH(ISR_GPIO,NULL);
      gpio_pin_intr_state_set(GPIO_ID_PIN(5),GPIO_PIN_INTR_ANYEDGE);
      system_soft_wdt_feed();


    MQTT_InitConnection(&mqttClient,"34.250.x.x",14553,0);
  //MQTT_InitConnection(&mqttClient, "192.168.11.122", 1880, 0);

  if ( !MQTT_InitClient(&mqttClient,"omar15","ngmcxxx","yq2zAFj7xxx",120,1) )
  {
    INFO("Failed to initialize properly. Check MQTT version.\r\n");
    return;
  }
  //MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);
  MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);
  system_soft_wdt_feed();
 // WIFI_Connect("Etisalat 4G iModem-6569","22999836", wifiConnectCb);
  WIFI_Connect("WE_EHAB","ESONEh@b", wifiConnectCb);
}

/*
void timer_init(void)
{os_timer_disarm(&test_timer);
os_timer_setfn(&test_timer, (os_timer_func_t *)app_init, NULL);
os_timer_arm(&test_timer,200, 0);
	}*/



//************************
//this is the start of the code 
// after system init function app init will be called
//************************
void user_init(void)
{
  system_init_done_cb(app_init);

}
