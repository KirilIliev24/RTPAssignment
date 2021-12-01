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
 

 struct SimpleMessage
 {
	 char id;
	 char data;
 };

#define QUEUE_LENGTH 10
#define QUEUE_ITEM_SIZE sizeof(struct SimpleMessage)

 TimerHandle_t timer1;
 TimerHandle_t timer2;
 TimerHandle_t timer3;
 
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
	 DDRK = 0b00110101;
	 
	 //PortC all data OUT
	 DDRC = 0b11111111;
	 
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
 
 
void receiveTask (void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 1);
	#endif
	 
	while(1)
	{
		//receive data on pin 1
		//printf("Task 1 \n");
		int s = PINA & 0b00000010;
		if (s == 0)
		{
			//printf("s is 0");
		}
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}

void sendTask(void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 2);
	#endif
	 
	while(1)
	{
		//send data on pin 0 and 2
		//printf("Sending signal \n");
		PORTA = PORTA & 0b11111010;
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}

char id = 0;

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
	 
	while(1)
	{
		xTaskDelayUntil(&last_wake_time, xFrequency);
		
		while (stdio_inputIsWaiting())
		{
			i = getchar();
			message.data = i;
			message.id = ++id;
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
	 
	 while(1)
	 {
		 // Blocks when nothing in queue
		 // TOOD: change to receiveQueue
		 if( xQueueReceive( sendQueue, &message, portMAX_DELAY )){
		 
			//read from the queue and print
			 if (xSemaphoreTake(printSemaphore, portMAX_DELAY))
			 {
				 printf("%c", message.data);
				 xSemaphoreGive(printSemaphore);
			 }
		 }
	 }
 }
 