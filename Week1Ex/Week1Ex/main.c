#include <stdio.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <stdint.h>
// Drivers
#include <display_7seg.h>

#include <ATMEGA_FreeRTOS.h>
#include <semphr.h>

#include <FreeRTOSTraceDriver.h>
#include <stdio_driver.h>
#include <serial.h>
#include <timers.h>
#include <task.h>

SemaphoreHandle_t printSemaphore;
QueueHandle_t sendQueue;
QueueHandle_t receiveQueue;

void receiveTask( void *pvParameters );
void sendTask( void *pvParameters );
void userInputTask( void *pvParameters );
void printTask( void *pvParameters );

struct Message
{
	char ucMessageID;
	char ucData[ 20 ];
};

typedef struct SimpleMessage SimpleMessage;
 struct SimpleMessage
{
	char data;
	char parity;
};

#define QUEUE_LENGTH 10
#define QUEUE_ITEM_SIZE sizeof(struct SimpleMessage)

TimerHandle_t timer1;
TimerHandle_t timer2;
TimerHandle_t timer3;

char setBit(char value, char pos){
	char mask = 0x01 << pos;
	return (value | mask);
}

char unsetBit(char value, char pos){
	char mask = 0x01 << pos;
	return (value & ~mask);
}

char isSet(char value, int pos){
	char mask = 0x01 << pos;
	return value & mask;
}

// returns a new char with the least sig bit set to the value of the bit in position pos of the data parameter
char bitAtPos(char data, int pos){
	char mask = 0x01;
	return (data >> (pos)) & mask;
}

// returns a char with the leas 4 bits as the hamming bits of the data
char getHammingBits(char data){
	// TODO: This could be prettier
	char bit1 = bitAtPos(data, 0) ^ bitAtPos(data, 1) ^ bitAtPos(data, 3) ^ bitAtPos(data, 4) ^ bitAtPos(data, 6);
	char bit2 = (bitAtPos(data, 0) ^ bitAtPos(data, 2) ^ bitAtPos(data, 3) ^ bitAtPos(data, 5) ^ bitAtPos(data, 6)) << 1;
	char bit3 = (bitAtPos(data, 1) ^ bitAtPos(data, 2) ^ bitAtPos(data, 3) ^ bitAtPos(data, 7)) << 2;
	char bit4 = (bitAtPos(data, 4) ^ bitAtPos(data, 5) ^ bitAtPos(data, 6) ^ bitAtPos(data, 7)) << 3;
	return bit4 | bit3 | bit2 | bit1;
}
 
void initialiseSystem()
{
	// Make it possible to use stdio on COM port 0 (USB) on Arduino board - Setting 57600,8,N,1
	 
	stdio_initialise(ser_USART0);
	 
	//Port initialization
	//PortA all data IN
	DDRA = 0b00000000;
	 
	//PortK
	//bit 0 out for acknowledgment
	//bit 1 in for acknowledgment
	//bit 2 message out signal
	//bit 3 message in signal
	//bit 4 and 5 also data OUT
	//bit 6 and 7 also data IN
	// DDRK = 0b10101100;
	DDRK = 0b00110101;
	PORTK = 0b00000000;

	 
	//PortC all data OUT and set to zero
	DDRC = 0b11111111;
	PORTC = 0b00000000;
	 
	if (NULL == printSemaphore)  // Check to confirm that the Semaphore has not already been created.
	{
		printSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore.
		if (NULL != printSemaphore)
		{
			xSemaphoreGive( ( printSemaphore ) );  // Make the mutex available for use, by initially "Giving" the Semaphore.
		}
	}
	 
	//Task initialization
	xTaskCreate(
	receiveTask,
	"receive",
	configMINIMAL_STACK_SIZE,
	NULL,
	1,
	NULL
	);
	
	xTaskCreate(
	sendTask,
	"send",
	configMINIMAL_STACK_SIZE,
	NULL,
	1,
	NULL
	);
	 
	xTaskCreate(
	userInputTask,
	"userInput",
	configMINIMAL_STACK_SIZE,
	NULL,
	2,
	NULL
	);
	 
	xTaskCreate(
	printTask,
	"print",
	configMINIMAL_STACK_SIZE,
	NULL,
	1,
	NULL
	);
	
	//Queue init
	sendQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
	receiveQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
	
	//trace_init();
}
/*-----------------------------------------------------------*/

int main(void)
{
	initialiseSystem();
	vTaskStartScheduler();
}

void tryReceive(SimpleMessage message){

}

void receiveTask (void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 1);
	#endif
	
	char readValue;
	struct SimpleMessage message;
	
	for(;;)
	{
		vTaskDelay(pdMS_TO_TICKS(25));
		
		if ('\0' != PINA){
			message.data = PINA;
			vTaskDelay(pdMS_TO_TICKS(100));
			message.parity = PINA;
		
			if (message.parity = getHammingBits(message.data))
			{
				PORTA = setBit(PORTA, 0); // ACK
				xQueueSend(receiveQueue,(void*)&message, portMAX_DELAY);
			}
			else{
				PORTA = setBit(PORTA, 2); // NACK
			}
		
			vTaskDelay(pdMS_TO_TICKS(100));
			unsetBit(PORTA, 0);
			unsetBit(PORTA, 2);
		}
		
	}
}

void trySend(SimpleMessage message){
	PORTC = message.data;
	vTaskDelay(pdMS_TO_TICKS(100));
	PORTC = getHammingBits(message.data);
	vTaskDelay(pdMS_TO_TICKS(100));
}


BaseType_t waitForAckNackOrTimeout(){
	// Timeout after ~ 100ms
	for (int i = 10; i > 0; i--)
	{
		if (bitAtPos(PINK, 1)) // ACK
		{
			return pdTRUE;
		}
		else if (bitAtPos(PINK, 3))
		{
			return pdFALSE;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	return pdFALSE;
}

void clearMessage(){
	PORTC = 0x00;
}


void sendTask(void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 2);
	#endif
	
	struct SimpleMessage message;
	
	for(;;)
	{
		if( xQueueReceive( sendQueue, &message, portMAX_DELAY )){
	 
			do{
				trySend(message);
			} while (! waitForAckNackOrTimeout());
			
			clearMessage();
			vTaskDelay(pdMS_TO_TICKS(100));
		 }
	}
}


void userInputTask(void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 3);
	#endif
	
	TickType_t last_wake_time = xTaskGetTickCount();
	const TickType_t xFrequency = 58 / 2; // TODO: define proper
	 
	char i;
	struct SimpleMessage message;
	 
	for(;;)
	{
		xTaskDelayUntil(&last_wake_time, xFrequency);
		
		while (stdio_inputIsWaiting())
		{
			i = getchar();
			message.data = i;
			xQueueSend(sendQueue,(void*)&message, portMAX_DELAY);
		}
	}
}

void printTask(void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 4);
	#endif
	 
	struct SimpleMessage message;
	 
	for(;;)
	{
		// Blocks when nothing in queue
		// TOOD: change to receiveQueue
		if( xQueueReceive( receiveQueue, &message, portMAX_DELAY )){
		//if( xQueueReceive( sendQueue, &message, portMAX_DELAY )){
	 
			//read from the queue and print
			if (xSemaphoreTake(printSemaphore, portMAX_DELAY))
			{
				printf("%c", message.data);
				xSemaphoreGive(printSemaphore);
			}
		 }
	 }
 }
 