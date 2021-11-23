/*
 * main.c
 *
 * Created: 29/08/2021 21:44:06
 * Author: LBS
 
 * System tick = 58 ticks pr. second
 */

 #include <stdio.h>
 #include <avr/io.h>
 #include <avr/sfr_defs.h>
 #include <stdint.h>
 // Drivers
 #include <rc_servo.h>
 #include <display_7seg.h>

 #include <ATMEGA_FreeRTOS.h>
 #include <semphr.h>

 #include <FreeRTOSTraceDriver.h>
 #include <stdio_driver.h>
 #include <serial.h>
 #include <timers.h>


//
 //// define semaphore handle
 //SemaphoreHandle_t xTestSemaphore;
//
//
 ///*-----------------------------------------------------------*/
 //void create_tasks_and_semaphores(void)
 //{
	 //// Semaphores are useful to stop a Task proceeding, where it should be paused to wait,
	 //// because it is sharing a resource, such as the Serial port.
	 //// Semaphores should only be used whilst the scheduler is running, but we can set it up here.
	 //if ( xTestSemaphore == NULL )  // Check to confirm that the Semaphore has not already been created.
	 //{
		 //xTestSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore.
		 //if ( ( xTestSemaphore ) != NULL )
		 //{
			 //xSemaphoreGive( ( xTestSemaphore ) );  // Make the mutex available for use, by initially "Giving" the Semaphore.
		 //}
	 //}
//
	 //xTaskCreate(
	 //task1
	 //,  "Task1"  // A name just for humans
	 //,  configMINIMAL_STACK_SIZE  // This stack size can be checked & adjusted by reading the Stack Highwater
	 //,  NULL
	 //,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
	 //,  NULL );
//
	 //xTaskCreate(
	 //task2
	 //,  "Task2"  // A name just for humans
	 //,  configMINIMAL_STACK_SIZE  // This stack size can be checked & adjusted by reading the Stack Highwater
	 //,  NULL
	 //,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
	 //,  NULL );
 //}
//
 ///*-----------------------------------------------------------*/
 //void task1( void *pvParameters )
 //{
	 //#if (configUSE_APPLICATION_TASK_TAG == 1)
	 //// Set task no to be used for tracing with R2R-Network
	 //vTaskSetApplicationTaskTag( NULL, ( void * ) 1 );
	 //#endif
//
	 //TickType_t xLastWakeTime;
	 //const TickType_t xFrequency = 58; // 1s
	 //
	 //// Initialize the xLastWakeTime variable with the current time.
	 //xLastWakeTime = xTaskGetTickCount();
	 //int8_t percent =-100;
	 //uint8_t up = 1;
	 //char i;
	  //
	 //for(;;)
	 //{
		 //xTaskDelayUntil( &xLastWakeTime, xFrequency );
			//
		  //// test display	 
		  //display_7seg_display(percent,0);
		//
		 //// test I/O   -   stdio functions are not reentrant - Should normally be protected by MUTEX
		 //if(stdio_inputIsWaiting()){
			 //i = getchar();
			 //printf("\n   Input received: %c \n\n", i);
		 //}
		 //
		 //// test rc servo
		 //if(up){
			 //rc_servo_setPosition (0,  percent);
			 //percent=percent+25;
			 //if(percent>100){
				 //up = 0;
				 //percent =100;
			 //}
		 //}
		 //else{
			 //rc_servo_setPosition (0,  percent);
			 //percent=percent-10;
			 //if(percent<-100){
				 //up = 1;
				 //percent =-100;
			 //}
		 //}
	 //}
 //}
//
 ///*-----------------------------------------------------------*/
 //void task2( void *pvParameters )
 //{
	 //#if (configUSE_APPLICATION_TASK_TAG == 1)
	 //// Set task no to be used for tracing with R2R-Network
	 //vTaskSetApplicationTaskTag( NULL, ( void * ) 2 );
	 //#endif
//
	 //TickType_t xLastWakeTime;
	 //const TickType_t xFrequency = 6; // 100 ms
//
	 //// Initialize the xLastWakeTime variable with the current time.
	 //xLastWakeTime = xTaskGetTickCount();
	 //
	 //for(;;)
	 //{
		 //xTaskDelayUntil( &xLastWakeTime, xFrequency );
		 //
		 //// test switches and led
		 //uint8_t sw0 = PINC & _BV(PINC0);
		 //PORTA = ((PIND & (_BV(PIND3) | _BV(PIND2))) << 4) | (PINC & (_BV(PINC0) | _BV(PINC1)| _BV(PINC2) | _BV(PINC3) | _BV(PINC4)  | _BV(PINC5)));
		//
		 //// stdio functions are not reentrant - Should normally be protected by MUTEX
		 //printf("\n\n\nsw0 %d",((PIND & (_BV(PIND3) | _BV(PIND2))) << 4) | (PINC & (_BV(PINC0) | _BV(PINC1)| _BV(PINC2) | _BV(PINC3) | _BV(PINC4) | _BV(PINC5))));
		 //
	 //}
 //}
//
 ///*-----------------------------------------------------------*/

SemaphoreHandle_t printSemaphore;
QueueHandle_t sendQueue;
QueueHandle_t receiveQueue;

void receiveTask( void *pvParameters );
void sendTask( void *pvParameters );
void userInputTask( void *pvParameters );
void printTask( void *pvParameters );

struct Message
{
	char data[25];
	int16_t ID;
} FullMessage;

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
	 
	 if ( printSemaphore == NULL )  // Check to confirm that the Semaphore has not already been created.
	 {
		printSemaphore = xSemaphoreCreateMutex();  // Create a mutex semaphore.
		if ( ( printSemaphore ) != NULL )
		{
			xSemaphoreGive( ( printSemaphore ) );  // Make the mutex available for use, by initially "Giving" the Semaphore.
		}
	 }
	  
	 //Task initialization
	 xTaskCreate(
		 receiveTask,
		 "receive",
		 400,
		 (void *) 1,
		 1,
		 NULL
	 );
	 
	 xTaskCreate(
		 sendTask,
		 "send",
		 400,
		 (void *) 1,
		 1,
		 NULL
	 );
	 
	 xTaskCreate(
		 userInputTask,
		 "userInput",
		 400,
		 (void *) 1,
		 1,
		 NULL
	 );
	 
	  xTaskCreate(
		 printTask,
		 "print",
		 400,
		 (void *) 1,
		 1,
		 NULL
	 );
	 
	 //Queue init
	 sendQueue = xQueueCreate(10, sizeof(FullMessage));
	 receiveQueue = xQueueCreate(10, sizeof(FullMessage));
	 
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

void userInputTask(void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 3);
	#endif
	
	//Message msg = malloc(sizeof (Message));	
	char i;
	const TickType_t xFrequency = 116;
	while(1)
	{
		xSemaphoreTake(printSemaphore, portMAX_DELAY);
		//add to the queue
		
		if (stdio_inputIsWaiting())
		{
			if (xSemaphoreTake(printSemaphore, xFrequency * 2) == pdTRUE)
			{
				while (stdio_inputIsWaiting())
				{
					i = getchar();
					xQueueSend(sendQueue,
					(void*)&i, portMAX_DELAY);
				}
				xSemaphoreGive(printSemaphore);
			}
		}
		
		
		
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void printTask(void * pvParameters)
{
	//picoscope
	#if (configUSE_APPLICATION_TASK_TAG == 1)
	vTaskSetApplicationTaskTag(NULL, (void *) 4);
	#endif
	const TickType_t xFrequency = 116;
	while(1)
	{
		//read from the queue and print
		if (xSemaphoreTake(printSemaphore, xFrequency * 2) == pdTRUE)
		{
			char i;
			//struct FullMessage xRxedStructure;
			for(;;)
			{
				xQueueReceive(sendQueue,&(i),portMAX_DELAY);
				//Now the struct can be used in the code e.g.
				//int16_t id = xRxedStructure.ID;
				//char data[25] = xRxedStructure.data;
				//printf("Data received: %d %s \n", id, data);
				printf("Data received: %c \n", i);
			}
			xSemaphoreGive(printSemaphore);
		}
		//xSemaphoreTake(printSemaphore, portMAX_DELAY)
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}