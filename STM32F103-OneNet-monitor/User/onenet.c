

//单片机头文件
#include "stm32f1xx_hal.h"

//外设驱动
#include "esp8266.h"

//协议文件
#include "onenet.h"
#include "mqttkit.h"
#include "config.h"

//硬件板级驱动
#include "usart/bsp_usart.h"
#include "bsp_delay.h"
#include "led/bsp_led.h"
#include "alarm/bsp_alarm.h"

//C库
#include <string.h>
#include <stdio.h>

#include "cJSON.h"

extern uint8_t LED_Status;
extern uint8_t Alarm_Status;

extern unsigned char esp8266_buf[512];
extern uint8_t Alarm_flag;
#define USART_DEBUG &huart1

//==========================================================
//	函数名称:	OneNet_DevLink
//
//	函数功能:	与OneNET创建连接
//
//	入口参数:	无
//
//	返回值:		1-成功	0-失败
//
//	说明:		与OneNET平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					//协议包

	unsigned char *dataPtr;
	_Bool status = 1;
	
	UsartPrintf(USART_DEBUG, "OneNet_DevLink: product=%s, device=%s\r\n",
						ONENET_PRODUCT_ID, ONENET_DEVICE_ID);
	
	if(MQTT_PacketConnect(ONENET_PRODUCT_ID, ONENET_TOKEN, ONENET_DEVICE_ID, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//上传平台
		dataPtr = ESP8266_GetIPD(250);									//等待平台响应

		if(dataPtr != NULL)
		{	
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0: UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n"); status = 0; break;
					
					case 1: UsartPrintf(USART_DEBUG, "WARN:	连接失败:协议错误\r\n"); break;
					case 2: UsartPrintf(USART_DEBUG, "WARN:	连接失败:非法的ClientID\r\n"); break;
					case 3: UsartPrintf(USART_DEBUG, "WARN:	连接失败:服务器崩溃\r\n"); break;
					case 4: UsartPrintf(USART_DEBUG, "WARN:	连接失败:用户名或密码错误\r\n"); break;
					case 5: UsartPrintf(USART_DEBUG, "WARN:	连接失败:非法链接(如token非法)\r\n"); break;
					
					default: UsartPrintf(USART_DEBUG, "ERR:	连接失败:未知错误\r\n"); break;
				}
			}
		}
		
		MQTT_DeleteBuffer(&mqttPacket);								//删除包
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");
	
	return status;
}

//==========================================================
//	函数名称:	OneNet_Subscribe
//
//	函数功能:	订阅
//
//	入口参数:	topics:    订阅的Topic
//				topic_cnt: Topic个数
//
//	返回值:		SEND_TYPE_OK-成功	SEND_TYPE_SUBSCRIBE-需要重发
//
//	说明:		
//==========================================================
void OneNet_Subscribe(const char *topics[], unsigned char topic_cnt)
{
	unsigned char i = 0;
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};							//协议包
	
	for(; i < topic_cnt; i++)
		UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topics[i]);
	
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, topics, topic_cnt, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);					//向平台发送订阅请求
		
		MQTT_DeleteBuffer(&mqttPacket);											//删除包
	}
}

//==========================================================
//	函数名称:	OneNet_Publish
//
//	函数功能:	发布消息
//
//	入口参数:	topic: 发布的Topic
//				msg:   消息内容
//
//	返回值:		SEND_TYPE_OK-成功	SEND_TYPE_PUBLISH-需要重发
//
//	说明:		
//==========================================================
void OneNet_Publish(const char *topic, const char *msg)
{
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};							//协议包
	
	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);
	
	/* QoS1 makes OneNET return PUBACK, so failed publishes are observable. */
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL1, 0, 1, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);					//向平台发送发布请求
		
		MQTT_DeleteBuffer(&mqttPacket);											//删除包
	}
}

//==========================================================
//	函数名称:	OneNet_RevPro
//
//	函数功能:	平台返回数据检测
//
//	入口参数:	cmd: 平台返回的数据
//
//	返回值:		无
//
//	说明:		
//==========================================================
void OneNet_RevPro(unsigned char *cmd)
{
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};								//协议包
	
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	
	unsigned short topic_len = 0;
	unsigned short req_len = 0;
	
	unsigned char type = 0;
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;
	
	short result = 0;
	cJSON *json, *params_json, *led_json, *Alarm_json;
	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_CMD:															//命令下发
			
			result = MQTT_UnPacketCmd(cmd, &cmdid_topic, &req_payload, &req_len);	//解析出Topic和消息体
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "cmdid: %s, req: %s, req_len: %d\r\n", cmdid_topic, req_payload, req_len);
				
				if(MQTT_PacketCmdResp(cmdid_topic, req_payload, &mqttPacket) == 0)	//命令回复组包
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send CmdResp\r\n");
					
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//回复命令
					MQTT_DeleteBuffer(&mqttPacket);									//删除包
				}
			}
			break;
			
		case MQTT_PKT_PUBLISH:														//接收到Publish消息
		
			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																	cmdid_topic, topic_len, req_payload, req_len);
				
				// 对数据包req_payload进行JSON格式解析
				json = cJSON_Parse(req_payload);
				params_json = cJSON_GetObjectItem(json, "params");
				led_json = cJSON_GetObjectItem(params_json, "LED");
				Alarm_json = cJSON_GetObjectItem(params_json, "Alarm");
				
				if(led_json != NULL)
				{
					if(led_json->type == cJSON_True) 
					{
						LED_ON();
						LED_ManualStatus = 1;
						LED_Status = 1;   // 更新状态变量
					}
					else 
					{
						LED_OFF();
						LED_ManualStatus = 0;
						LED_Status = 0;
					}
				}	
				
				if(Alarm_json != NULL)
				{
					if(Alarm_json->type == cJSON_True) 
					{
						Alarm_flag = 1;
						Alarm_ManualStatus = 1;
						Alarm_ON();
						UsartPrintf(USART_DEBUG, "Alarm_flag = 1\r\n");									
					}
					else 
					{
						Alarm_flag = 0;
						Alarm_ManualStatus = 0;
						Alarm_OFF();
						UsartPrintf(USART_DEBUG, "Alarm_flag = 0\r\n");					
					}
				}	
				cJSON_Delete(json);
			}
			break;
			
		case MQTT_PKT_PUBACK:														//发送Publish消息，平台回复的Ack
		
			if(MQTT_UnPacketPublishAck(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Publish Send OK\r\n");
			break;
			
		case MQTT_PKT_PUBREC:														//发送Publish消息，平台回复的Rec，设备需回复Rel消息
		
			if(MQTT_UnPacketPublishRec(cmd) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishRec\r\n");
				if(MQTT_PacketPublishRel(MQTT_PUBLISH_ID, &mqttPacket) == 0)
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send PublishRel\r\n");
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);
					MQTT_DeleteBuffer(&mqttPacket);
				}
			}
			break;
			
		case MQTT_PKT_PUBREL:														//收到Publish消息，设备回复Rec后，平台回复的Rel，设备需再次回复Comp
			
			if(MQTT_UnPacketPublishRel(cmd, pkt_id) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishRel\r\n");
				if(MQTT_PacketPublishComp(MQTT_PUBLISH_ID, &mqttPacket) == 0)
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send PublishComp\r\n");
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);
					MQTT_DeleteBuffer(&mqttPacket);
				}
			}
			break;
		
		case MQTT_PKT_PUBCOMP:														//发送Publish消息，平台返回Rec，设备回复Rel，平台再返回的Comp
		
			if(MQTT_UnPacketPublishComp(cmd) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishComp\r\n");
			}
			break;
			
		case MQTT_PKT_SUBACK:														//发送Subscribe消息的Ack
		
			if(MQTT_UnPacketSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe Err\r\n");
			break;
			
		case MQTT_PKT_UNSUBACK:														//发送UnSubscribe消息的Ack
		
			if(MQTT_UnPacketUnSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT UnSubscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT UnSubscribe Err\r\n");
			break;
		
		default:
			result = -1;
			break;
	}
	
	ESP8266_Clear();									//清空缓存区
	
	if(result == -1)
		return;
	
	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}
}
