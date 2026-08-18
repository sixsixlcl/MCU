

#include "stm32f1xx_hal.h" 

#include "esp8266.h"
#include "config.h"

#include "delay.h"
#include "bsp_delay.h"
#include "bsp_usart.h"
#include "oled/bsp_i2c_oled.h"

#include <string.h>
#include <stdio.h>

/* The legacy DelayMs implementation reconfigures SysTick. */
#define DelayMs(ms) HAL_Delay(ms)


#define ESP8266_WIFI_INFO		"AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n"
#define ESP8266_ONENET_INFO		"AT+CIPSTART=\"TCP\",\"" ONENET_MQTT_HOST "\"," ONENET_MQTT_PORT "\r\n"
#define OLED_Clear()            OLED_CLS()
volatile uint8_t rev_ok = 0;

unsigned char esp8266_buf[512];
volatile unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;
static uint8_t esp8266_rx_byte;


//==========================================================
//	函数名称：	ESP8266_Clear
//
//	函数功能：	清空缓存
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Clear(void)
{

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;
	rev_ok = 0;

}

//==========================================================
//	函数名称：	ESP8266_WaitRecive
//
//	函数功能：	等待接收完成
//
//	入口参数：	无
//
//	返回参数：	REV_OK-接收完成		REV_WAIT-接收超时未完成
//
//	说明：		循环调用检测是否接收完成
//==========================================================
// _Bool ESP8266_WaitRecive(void)
// {

// 	if(esp8266_cnt == 0) 							//如果接收计数为0 则说明没有处于接收数据中，所以直接跳出，结束函数
// 		return REV_WAIT;
		
// 	if(esp8266_cnt == esp8266_cntPre)				//如果上一次的值和这次相同，则说明接收完毕
// 	{
// 		esp8266_cnt = 0;							//清0接收计数
			
// 		return REV_OK;								//返回接收完成标志
// 	}
		
// 	esp8266_cntPre = esp8266_cnt;					//置为相同
	
// 	return REV_WAIT;								//返回接收未完成标志

// }



_Bool ESP8266_WaitRecive(void)
{

		
	if(rev_ok == 1)				//如果上一次的值和这次相同，则说明接收完毕
	{
		rev_ok = 0;
		return REV_OK;								//返回接收完成标志
	}
			
	return REV_WAIT;								//返回接收未完成标志

}

//==========================================================
//	函数名称：	ESP8266_SendCmd
//
//	函数功能：	发送命令
//
//	入口参数：	cmd：命令
//				res：需要检查的返回指令
//
//	返回参数：	0-成功	1-失败
//
//	说明：		
//==========================================================
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	uint32_t timeout_ms = 2000U;
	uint32_t start;

	/* Associating to an AP may take many seconds. Do not send another CWJAP
	   while the ESP8266 is still working on the current one. */
	if (strstr(cmd, "AT+CWJAP") != NULL)
		timeout_ms = 30000U;
	else if (strstr(cmd, "AT+CIPSTART") != NULL)
		timeout_ms = 15000U;

	ESP8266_Clear();
	if (HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen((const char *)cmd), 500) != HAL_OK)
		return 1;

	start = HAL_GetTick();
	while ((HAL_GetTick() - start) < timeout_ms)
	{
		/* CIPSEND returns a bare '>' without a line ending. */
		if (esp8266_cnt != 0U && strstr((const char *)esp8266_buf, res) != NULL)
		{
			ESP8266_Clear();
			return 0;
		}

		if(ESP8266_WaitRecive() == REV_OK)							//如果收到数据
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)		//如果检索到关键词
			{
				ESP8266_Clear();									//清空缓存
				
				return 0;
			}

			if (strstr((const char *)esp8266_buf, "ERROR") != NULL ||
				strstr((const char *)esp8266_buf, "DNS Fail") != NULL ||
				strstr((const char *)esp8266_buf, "busy") != NULL)
			{
				printf("ESP error, cmd=%s rx=%u data=%s\r\n",
				       cmd, esp8266_cnt, esp8266_buf);
				return 1;
			}
		}
		
		HAL_Delay(10);
	}
	
	printf("ESP timeout (%lums), cmd=%s rx=%u data=%s\r\n",
	       timeout_ms, cmd, esp8266_cnt, esp8266_buf);
	return 1;

}

//==========================================================
//	函数名称：	ESP8266_SendData
//
//	函数功能：	发送数据
//
//	入口参数：	data：数据
//				len：长度
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[32];
	
	ESP8266_Clear();								//清空接收缓存
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);		//发送命令
	if(!ESP8266_SendCmd(cmdBuf, ">"))				//收到‘>’时可以发送数据
	{
		// Usart_SendString(USART2, data, len);		//发送设备连接请求数据
        HAL_StatusTypeDef tx_status =
            HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 500);
        if (tx_status != HAL_OK)
            printf("ESP MQTT payload UART transmit failed, len=%u\r\n", len);
	}
	else
	{
		printf("ESP CIPSEND failed, len=%u\r\n", len);
	}

}

//==========================================================
//	函数名称：	ESP8266_GetIPD
//
//	函数功能：	获取平台返回的数据
//
//	入口参数：	等待的时间(乘以10ms)
//
//	返回参数：	平台返回的原始数据
//
//	说明：		不同网络设备返回的格式不同，需要去调试
//				如ESP8266的返回格式为	"+IPD,x:yyy"	x代表数据长度，yyy是数据内容
//==========================================================
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;
	
	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								//如果接收完成
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				//搜索“IPD”头
			if(ptrIPD == NULL)											//如果没找到，可能是IPD头的延迟，还是需要等待一会，但不会超过设定的时间
			{
				//printf("\"IPD\" not found\r\n");
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');							//找到':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;
				
			}
		}
		
		DelayMs(5);													//延时等待
	} while(timeOut--);
	
	return NULL;														//超时还未找到，返回空指针

}

//==========================================================
//	函数名称：	ESP8266_Init
//
//	函数功能：	初始化ESP8266
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Init(void)
{
	HAL_UART_AbortReceive(&huart2);
	ESP8266_Clear();
	HAL_UART_Receive_IT(&huart2, &esp8266_rx_byte, 1);
	printf("1. AT \r\n");
	HAL_Delay(2000);
	while(ESP8266_SendCmd("AT\r\n", "OK"))
		HAL_Delay(500);
	
	printf("2. CWMODE \r\n");
	while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
		HAL_Delay(500);
	
	printf("3. AT+CWDHCP \r\n");
	while(ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
		HAL_Delay(500);
	
	printf("4. CWJAP \r\n");
	while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		HAL_Delay(500);
	
	printf("5. CIPSTART TCP \r\n");
	HAL_Delay(500);  // CWJAP后等待网络栈就绪
	while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "OK"))
	{
		HAL_Delay(1000);
		printf("   TCP retry...\r\n");
	}
	
	printf("6. ESP8266 Init OK \r\n");
	OLED_Clear();
	OLED_ShowString_F8X16(1, 1, (uint8_t *)"NET CONNECTED");
	HAL_Delay(1000);
	OLED_Clear();
	

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &huart2)
	{
		if (esp8266_cnt < sizeof(esp8266_buf) - 1U)
		{
			esp8266_buf[esp8266_cnt++] = esp8266_rx_byte;
			esp8266_buf[esp8266_cnt] = '\0';
		}
		else
		{
			esp8266_cnt = 0;
		}

		if (esp8266_rx_byte == '\n' || esp8266_rx_byte == '>')
			rev_ok = 1;

		HAL_UART_Receive_IT(&huart2, &esp8266_rx_byte, 1);
	}
}

