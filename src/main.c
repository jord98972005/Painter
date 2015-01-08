#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx_dcmi.h"
#include "stm32f4xx_dma.h"
#include "tm_stm32f4_ili9341.h"
#include "tm_stm32f4_fonts.h"
#include "i2c_ops.h"
#include "ov7670.h"
#include "stm32f4xx.h"
#include "misc.h"
#include "ov7670_regsmap.h"
#include "main.h"

volatile int32_t dma_handler_counter = 0;
uint16_t j=0;

uint32_t camera_frame[10]; // storage frame data in 320*240 resolution

void RCC_Configuration(void)
{
	/* --------------------------- System Clocks Configuration -----------    ------*/
	/* USART1 clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	/* GPIOA clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
}

void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/*-------------------------- GPIO Configuration ------------------------    ----*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* Connect USART pins to AF */
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);   // USART1_TX
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);  // USART1_RX
}

void USART1_Configuration(void)
{
	USART_InitTypeDef USART_InitStructure;

	/* USARTx configuration ------------------------------------------------------*/
	/* USARTx configured as follow:
	 *  - BaudRate = 9600 baud
	 *  - Word Length = 8 Bits
	 *  - One Stop Bit
	 *  - No parity
	 *  - Hardware flow control disabled (RTS and CTS signals)
	 *  - Receive and transmit enabled
	 */
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
	USART_Cmd(USART3, ENABLE);
}

void USART1_puts(char* s)
{
	while(*s) {
		//USART_SendData(USART3, *s++);
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, *s);
		s++;
	}
}

/*void dma_init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	DMA_DeInit(DMA2_Stream1);

	DMA_InitTypeDef dma_init;

	dma_init.DMA_Channel = DMA_Channel_1;
	dma_init.DMA_PeripheralBaseAddr = (uint32_t) &(DCMI->DR);
	dma_init.DMA_Memory0BaseAddr = (uint32_t) camera_frame;//lcd address
	dma_init.DMA_DIR = DMA_DIR_PeripheralToMemory;
	dma_init.DMA_BufferSize = 1;
	dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable;
	dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma_init.DMA_Mode = DMA_Mode_Circular;
	dma_init.DMA_Priority = DMA_Priority_High;
	dma_init.DMA_FIFOMode = DMA_FIFOMode_Enable;
	dma_init.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	dma_init.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	dma_init.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

	//DCMI中断配置，在使用帧中断或者垂直同步中断的时候打开
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	NVIC_InitTypeDef dma_stream1_irq;
	dma_stream1_irq.NVIC_IRQChannel = DMA2_Stream1_IRQn;
	dma_stream1_irq.NVIC_IRQChannelPreemptionPriority = 1;
	dma_stream1_irq.NVIC_IRQChannelSubPriority = 2;
	dma_stream1_irq.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&dma_stream1_irq);


	DMA_Init(DMA2_Stream1, &dma_init);

	DMA_ITConfig(DMA2_Stream1, DMA_IT_TC | DMA_IT_FE, ENABLE);


	//DMA2 IRQ channel Configuration
	
	
	DMA_Cmd(DMA2_Stream1, ENABLE);
}*/

void camera_init(void)
{
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_DCMI, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

	/* Clocking of  GPIOB is enabled in i2c_init function */

	/* PA4 - HSYNC
	   PA6 - PIXCLK
	   PB7 - DCMI_VSYNC
	   PC6 - DCMI_D0
	   PC7 - DCMI_D1
	   PC8 - DCMI_D2
	   PC9 - DCMI_D3
	   PC11 - DCMI_D4
	   PB6 - DCMI_D5
	   PB8 - DCMI_D6
	   PB9 - DCMI_D7

	   PB10 - I2C_SCL
	   PB11 - I2C_SDA
	 */

	/* set  gpio pins to provide DCMI access */
	GPIO_InitTypeDef dcmi_gpioa_init;
	dcmi_gpioa_init.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_6;
	dcmi_gpioa_init.GPIO_Mode = GPIO_Mode_AF;
	dcmi_gpioa_init.GPIO_OType = GPIO_OType_PP;
	dcmi_gpioa_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
	dcmi_gpioa_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &dcmi_gpioa_init);

	GPIO_InitTypeDef dcmi_gpiob_init;
	dcmi_gpiob_init.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_8 | GPIO_Pin_9;
	dcmi_gpiob_init.GPIO_Mode = GPIO_Mode_AF;
	GPIO_Init(GPIOB, &dcmi_gpiob_init);

	GPIO_InitTypeDef dcmi_gpioc_init;
	dcmi_gpioc_init.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_11;
	dcmi_gpioc_init.GPIO_Mode = GPIO_Mode_AF;
	GPIO_Init(GPIOC, &dcmi_gpioc_init);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_DCMI);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_DCMI);

	GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_DCMI);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_DCMI);
	/* i2c interface init */
	GPIO_InitTypeDef i2c_gpio_init;

	i2c_gpio_init.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	i2c_gpio_init.GPIO_Mode = GPIO_Mode_AF;
	i2c_gpio_init.GPIO_OType = GPIO_OType_OD;
	i2c_gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
	i2c_gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init (GPIOB, &i2c_gpio_init);

	GPIO_PinAFConfig (GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);    // I2C2_SCL
	GPIO_PinAFConfig (GPIOB, GPIO_PinSource11, GPIO_AF_I2C2);    // I2C2_SDA
	/* gpio init to provide mco frequency */
	GPIO_InitTypeDef mco_Init;

	mco_Init.GPIO_Pin = GPIO_Pin_8;
	mco_Init.GPIO_Mode = GPIO_Mode_AF;
	mco_Init.GPIO_Speed = GPIO_Speed_100MHz;
	mco_Init.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOA, &mco_Init);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_MCO);

//*********************DCMI & DMA Configuration****************************************************************
	/* dcmi interface init */
	DCMI_InitTypeDef camera_init;

	camera_init.DCMI_CaptureMode = DCMI_CaptureMode_Continuous;//DCMI_CaptureMode_SnapShot;
	camera_init.DCMI_CaptureRate = DCMI_CaptureRate_All_Frame;
	camera_init.DCMI_ExtendedDataMode = DCMI_ExtendedDataMode_8b;
	camera_init.DCMI_HSPolarity = DCMI_HSPolarity_High;
	camera_init.DCMI_PCKPolarity = DCMI_PCKPolarity_Falling;
	camera_init.DCMI_SynchroMode = DCMI_SynchroMode_Hardware;
	camera_init.DCMI_VSPolarity = DCMI_VSPolarity_High;
 	DCMI_Init(&camera_init);


	DMA_DeInit(DMA2_Stream1);

	DMA_InitTypeDef dma_init;

	dma_init.DMA_Channel = DMA_Channel_1;
	dma_init.DMA_PeripheralBaseAddr = (uint32_t) &(DCMI->DR);
	dma_init.DMA_Memory0BaseAddr = (uint32_t) camera_frame;//lcd address
	dma_init.DMA_DIR = DMA_DIR_PeripheralToMemory;
	dma_init.DMA_BufferSize = 1;
	dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable;
	dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma_init.DMA_Mode = DMA_Mode_Circular;
	dma_init.DMA_Priority = DMA_Priority_High;
	dma_init.DMA_FIFOMode = DMA_FIFOMode_Enable;
	dma_init.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	dma_init.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	dma_init.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

	//DCMI中断配置，在使用帧中断或者垂直同步中断的时候打开
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	NVIC_InitTypeDef dma_stream1_irq;
	dma_stream1_irq.NVIC_IRQChannel = DMA2_Stream1_IRQn;
	dma_stream1_irq.NVIC_IRQChannelPreemptionPriority = 1;
	dma_stream1_irq.NVIC_IRQChannelSubPriority = 2;
	dma_stream1_irq.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&dma_stream1_irq);

	//Enable DCMIinterrupt
	DCMI_ITConfig(DCMI_IT_VSYNC, ENABLE);
	// DCMI configuration 
	DCMI_Init(&camera_init);
	// DMA2 IRQ channel Configuration 
	DMA_Init(DMA2_Stream1, &dma_init);
	//Enable DCMIinterrupt
	DMA_Cmd(DMA2_Stream1, ENABLE); 
 	DCMI_Cmd(ENABLE);




	/*capture mode dcmi & dma 
	DMA_ITConfig(DMA2_Stream1, DMA_IT_TC | DMA_IT_FE, ENABLE);
	// enable DCMI interface
	DCMI_CaptureCmd(ENABLE);
	dma_init();

	DCMI_ITConfig(DCMI_IT_LINE, ENABLE);//列中断)

	NVIC_InitTypeDef camera_irq;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);//中断抢占组1
	camera_irq.NVIC_IRQChannel = DCMI_IRQn;//摄像头中断
	camera_irq.NVIC_IRQChannelPreemptionPriority = 0;//先占优先级
	camera_irq.NVIC_IRQChannelSubPriority = 1;
	camera_irq.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&camera_irq);

	DCMI_ITConfig(DCMI_IT_ERR | DCMI_IT_FRAME | DCMI_IT_LINE |
			DCMI_IT_OVF | DCMI_IT_VSYNC, ENABLE);*/

	I2C_InitTypeDef i2c_init;

	i2c_init.I2C_Ack = I2C_Ack_Enable;
	i2c_init.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	i2c_init.I2C_ClockSpeed = 100000;
	i2c_init.I2C_DutyCycle = I2C_DutyCycle_2;
	i2c_init.I2C_Mode = I2C_Mode_I2C;
	i2c_init.I2C_OwnAddress1 = 21;

	I2C_Init(I2C2, &i2c_init);
	// enable I2C interface
	I2C_Cmd(I2C2, ENABLE);
}

/*char st[10]="";
void DMA2_Stream1_IRQHandler(void)
{
	// Test on DMA2 Channel1 Transfer Complete interrupt
	if(DMA_GetITStatus(DMA2_Stream1, DMA_IT_TCIF1) != RESET)
	{
		int r = DCMI_ReadData();
		itoa(r,st,10);
		USART1_puts(st);
		USART1_puts("\r\n");
		USART1_puts("DMA handle!\r\n");
		dma_handler_counter++;
		DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TCIF1);
		//when frame_flag =1,all the data will be send through serial port in main function
		DMA_ClearITPendingBit(DMA2_Stream1,DMA_IT_TCIF1);
	}
	if(DMA_GetITStatus(DMA2_Stream1, DMA_IT_TEIF1) == SET)
	{
		USART1_puts("DMA ERROR\r\n");
		Delay(100);
		DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TEIF1);
	}
}*/


int flag = 0;

/*void DCMI_IRQHandler(void)
{
	
	// Test on DMA2 Channel1 Transfer Complete interrupt
	if(DCMI_GetFlagStatus(DCMI_FLAG_VSYNCRI) == SET)
	{
		if(flag!=1){
			USART1_puts("dcmi VSYNCR1\r\n");
		}
		DCMI_ClearFlag(DCMI_FLAG_VSYNCRI);
	}
	else if(DCMI_GetFlagStatus(DCMI_FLAG_LINERI) == SET)
	{
		if(flag!=1){
		USART1_puts("dcmi LINERI\r\n");
	}
		DCMI_ClearFlag(DCMI_FLAG_LINERI);
	}
	else if(DCMI_GetFlagStatus(DCMI_FLAG_FRAMERI) == SET)
	{
		flag = 1;
		// USART1_puts("DCMI_FLAG_FRAMERI \n\n");
		DCMI_ClearFlag(DCMI_FLAG_FRAMERI);
	}
	else if(DCMI_GetFlagStatus(DCMI_FLAG_ERRRI) == SET)
	{
		USART1_puts("DCMI_FLAG_ERRRI \n\n");
		DCMI_ClearFlag(DCMI_FLAG_ERRRI);
	}
	else if(DCMI_GetFlagStatus(DCMI_FLAG_OVFRI) == SET)
	{
		flag = 1;
		USART1_puts("DCMI_FLAG_OVFRI \n\n");
		DCMI_ClearFlag(DCMI_FLAG_OVFRI);
	}
}*/



/*void SendPicture(void)
{
	int i;
	for(i=0;i<10;i++)
	{
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, camera_frame[i]);
	}
}*/

void DCMI_IRQHandler(void)
{
	int a;
  if( DCMI_GetITStatus(DCMI_IT_VSYNC)!= RESET)
  {
    DCMI_ClearITPendingBit(DCMI_IT_VSYNC);
    TM_ILI9341_DisplayWindow();
    for(a=0;a<7680;a++){
    TM_ILI9341_SendData(camera_frame[0]);
	TM_ILI9341_SendData(camera_frame[1]);
	TM_ILI9341_SendData(camera_frame[2]);
	TM_ILI9341_SendData(camera_frame[3]);
	TM_ILI9341_SendData(camera_frame[4]);
	TM_ILI9341_SendData(camera_frame[5]);
	TM_ILI9341_SendData(camera_frame[6]);
	TM_ILI9341_SendData(camera_frame[7]);
	TM_ILI9341_SendData(camera_frame[8]);
	TM_ILI9341_SendData(camera_frame[9]);

	}
  }
}



char* itoa(int value, char* result, int base)
{
	if (base < 2 || base > 36) {
		*result = '\0';
		return result;
	}
	char *ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value         - value * base)];
	} while (value);

	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}
int main(void)
{
	uint8_t c = 0;
	SystemInit();
	RCC_Configuration();
	GPIO_Configuration();
	USART1_Configuration();
	TM_ILI9341_Init();
    TM_ILI9341_Rotate(TM_ILI9341_Orientation_Landscape_2);

	RCC_APB2PeriphClockCmd (RCC_APB2Periph_SYSCFG, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

	GPIO_InitTypeDef GPIOG_Init;

	GPIOG_Init.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
	GPIOG_Init.GPIO_Mode = GPIO_Mode_OUT;
	GPIOG_Init.GPIO_OType = GPIO_OType_PP;
	GPIOG_Init.GPIO_Speed = GPIO_Speed_100MHz;

	GPIO_Init(GPIOG, &GPIOG_Init);
	camera_init();
	
	c = I2C_readreg(0x0a);
	
	/* uint8_t creg=0;
creg= I2C_readreg(0x1c);
	for(j=0;j<8;j++){
 		if((creg&0x80)==0x80)	// If bit in Data is high, write high on SCCB/I2C
      {  
        TM_ILI9341_Puts(20, 20*j, "1", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_GREEN2);
      }
      else 
      {
        TM_ILI9341_Puts(20, 20*j, "0", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_GREEN2);
       }
       creg<<=1; 
    }  

    uint8_t dreg=0;

dreg = I2C_readreg(0x1d);
for(j=0;j<8;j++){
 		if((dreg&0x80)==0x80)	// If bit in Data is high, write high on SCCB/I2C
      {  
        TM_ILI9341_Puts(60, 20*j, "1", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_GREEN2);
      }
      else 
      {
        TM_ILI9341_Puts(60, 20*j, "0", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_GREEN2);
       }
       dreg<<=1; 
    }  */


	if(c == 0x76)
	{
		ov7670_init();
		GPIO_SetBits(GPIOG, GPIO_Pin_13);
	}

	while(1)
	{
		if(dma_handler_counter == 10)
			GPIO_SetBits(GPIOG, GPIO_Pin_14);
	}

DCMI_CaptureCmd(ENABLE);

	return 0;
}
