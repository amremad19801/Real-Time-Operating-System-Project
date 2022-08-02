#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "string.h"

#define CCM_RAM __attribute__((section(".ccmram")))

static void Start(void);
static void Reset(void);
static void Sender1Task(void *p);
static void Sender2Task(void *p);
static void ReceiverTask(void *p);
static void prvAutoReloadTimer1Callback(TimerHandle_t xTimer1);
static void prvAutoReloadTimer2Callback(TimerHandle_t xTimer2);
static void prvAutoReloadTimer3Callback(TimerHandle_t xTimer3);

static TimerHandle_t xTimer1 = NULL;
static TimerHandle_t xTimer2 = NULL;
static TimerHandle_t xTimer3 = NULL;

BaseType_t xTimer1Started;
BaseType_t xTimer2Started;
BaseType_t xTimer3Started;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

long int TransmittedMessages;
long int ReceivedMessages;
long int BlockedMessages;

long int LowerLimit[6] = { 50, 80, 110, 140, 170, 200 };
long int UpperLimit[6] = { 150, 200, 250, 300, 350, 400 };

long int CurrentLowerLimit;
long int CurrentUpperLimit;

long int Sender1Period;
long int Sender2Period;
long int ReceiverPeriod = 100;

int i = 0;

QueueHandle_t TestQueue;

SemaphoreHandle_t Sender1;
SemaphoreHandle_t Sender2;
SemaphoreHandle_t Receiver;
SemaphoreHandle_t QueueKeeper;

int main(int argc, char *argv[]) {
	srand(time(0));

	Start();

	TestQueue = xQueueCreate(2, 120 * sizeof(char));

	vSemaphoreCreateBinary(Sender1);
	vSemaphoreCreateBinary(Sender2);
	vSemaphoreCreateBinary(Receiver);

	QueueKeeper = xSemaphoreCreateMutex();

	xTimer1 = xTimerCreate("Timer1", (pdMS_TO_TICKS(Sender1Period)), pdTRUE,
			(void*) 1, prvAutoReloadTimer1Callback);
	xTimer2 = xTimerCreate("Timer2", (pdMS_TO_TICKS(Sender2Period)), pdTRUE,
			(void*) 1, prvAutoReloadTimer2Callback);
	xTimer3 = xTimerCreate("Timer3", (pdMS_TO_TICKS(ReceiverPeriod)), pdTRUE,
			(void*) 1, prvAutoReloadTimer3Callback);

	if (TestQueue != NULL) {
		xTaskCreate(Sender1Task, "Sender1", 2048, NULL, 1, NULL);
		xTaskCreate(Sender2Task, "Sender2", 2048, NULL, 1, NULL);
		xTaskCreate(ReceiverTask, "Receiver", 2048, NULL, 2, NULL);
		//vTaskStartScheduler();
	} else {
		trace_puts("Queue could not be created.");
	}

	if ((xTimer1 != NULL) && (xTimer2 != NULL) && (xTimer3 != NULL)) {
		xTimer1Started = xTimerStart(xTimer1, 0);
		xTimer2Started = xTimerStart(xTimer2, 0);
		xTimer3Started = xTimerStart(xTimer3, 0);
	}

	if ((xTimer1Started == pdPASS) && (xTimer2Started == pdPASS)
			&& (xTimer3Started == pdPASS)) {
		vTaskStartScheduler();
	}

	return 0;
}

#pragma GCC diagnostic pop

static void Start(void) {
	if (i == 0) {
		char Period[120];

		CurrentLowerLimit = LowerLimit[i];
		CurrentUpperLimit = UpperLimit[i];

		Sender1Period = ((rand() % (CurrentUpperLimit - CurrentLowerLimit + 1))
				+ CurrentLowerLimit);
		Sender2Period = ((rand() % (CurrentUpperLimit - CurrentLowerLimit + 1))
				+ CurrentLowerLimit);

		sprintf(Period, "Period are %ld and %ld.", Sender1Period,
				Sender2Period);
		trace_puts(Period);
	}
}

static void Reset(void) {
	char Message[120];

	sprintf(Message, "Range from %ld to %ld.", CurrentLowerLimit,
			CurrentUpperLimit);
	trace_puts(Message);
	sprintf(Message, "The total number of transmitted messages is %ld.\n",
			TransmittedMessages);
	trace_puts(Message);
	sprintf(Message, "The total number of blocked messages is %ld.\n",
			BlockedMessages);
	trace_puts(Message);
	sprintf(Message, "The total number of received messages is %ld.\n",
			ReceivedMessages);
	trace_puts(Message);

	TransmittedMessages = 0;
	BlockedMessages = 0;
	ReceivedMessages = 0;

	xQueueReset(TestQueue);

	if (i <= 5) {
		CurrentLowerLimit = LowerLimit[i];
		CurrentUpperLimit = UpperLimit[i];
	} else {
		trace_puts("Game Over");

		vQueueDelete(TestQueue);

		xTimerDelete(xTimer1, 0);
		xTimerDelete(xTimer2, 0);
		xTimerDelete(xTimer3, 0);

		vTaskEndScheduler();
	}
}

static void Sender1Task(void *parameters) {
	BaseType_t Status;
	TickType_t TimeNow;
//	char Message[120];
	char ValueToSend[120];
//	const TickType_t TicksToWait = pdMS_TO_TICKS( 0 );

	while (1) {
		if (xSemaphoreTake(Sender1, portMAX_DELAY)) {
			xSemaphoreTake(QueueKeeper, portMAX_DELAY);

			TimeNow = xTaskGetTickCount();

			sprintf(ValueToSend, "Time is %ld", TimeNow);
//			trace_puts(ValueToSend);

			Status = xQueueSend(TestQueue, ValueToSend, 0);

			if (Status != pdPASS) {
				BlockedMessages++;
//				sprintf( Message, "The number of blocked message is %ld.", BlockedMessages );
//				trace_puts( Message );
			} else {
				TransmittedMessages++;
//				sprintf( Message, "The number of transmitted message is %ld.", TransmittedMessages );
//				trace_puts( Message );
			}

			xSemaphoreGive(QueueKeeper);
		}
	}
}

static void Sender2Task(void *parameters) {
	BaseType_t Status;
	TickType_t TimeNow;
//	char Message[120];
	char ValueToSend[120];
//	const TickType_t TicksToWait = pdMS_TO_TICKS( 0 );

	while (1) {
		if (xSemaphoreTake(Sender2, portMAX_DELAY)) {
			xSemaphoreTake(QueueKeeper, portMAX_DELAY);

			TimeNow = xTaskGetTickCount();

			sprintf(ValueToSend, "Time is %lu", TimeNow);
//			trace_puts( ValueToSend );

			Status = xQueueSend(TestQueue, ValueToSend, 0);

			if (Status != pdPASS) {
				BlockedMessages++;
//				sprintf( Message, "The number of blocked message is %ld.", BlockedMessages );
//				trace_puts( Message );
			} else {
				TransmittedMessages++;
//				sprintf( Message, "The number of transmitted message is %ld.", TransmittedMessages );
//				trace_puts( Message );
			}
			xSemaphoreGive(QueueKeeper);
		}
	}
}

static void ReceiverTask(void *parameters) {
	BaseType_t Status;
//	char Message[120];
	char ReceivedValue[120];
//	const TickType_t TicksToWait = pdMS_TO_TICKS( 0 );

	while (1) {
		if (xSemaphoreTake(Receiver, portMAX_DELAY)) {
			xSemaphoreTake(QueueKeeper, portMAX_DELAY);

			Status = xQueueReceive(TestQueue, ReceivedValue, 0);

			//trace_puts( ReceivedValue );

			if (Status == pdPASS) {
				ReceivedMessages++;
//				sprintf( Message, "The number of received message is %ld.", ReceivedMessages );
//				trace_puts( Message );
			}
			xSemaphoreGive(QueueKeeper);
		}
	}
}

static void prvAutoReloadTimer1Callback(TimerHandle_t xTimer1) {
//	char Period1[120];

//	trace_puts( "Auto-reload timer 1 callback executing" );

	Sender1Period = ((rand() % (CurrentUpperLimit - CurrentLowerLimit + 1))
			+ CurrentLowerLimit);

//	sprintf( Period1, "Period1 is %ld.", Sender1Period );
//	trace_puts( Period1 );

	xTimerChangePeriod(xTimer1, pdMS_TO_TICKS(Sender1Period), 0);
	xSemaphoreGive(Sender1);
}

static void prvAutoReloadTimer2Callback(TimerHandle_t xTimer2) {
//	char Period2[120];

//	trace_puts( "Auto-reload timer 2 callback executing" );

	Sender2Period = ((rand() % (CurrentUpperLimit - CurrentLowerLimit + 1))
			+ CurrentLowerLimit);

//	sprintf( Period2, "Period2 are %ld.", Sender2Period );
//	trace_puts( Period2 );

	xTimerChangePeriod(xTimer2, pdMS_TO_TICKS(Sender2Period), 0);
	xSemaphoreGive(Sender2);
}

static void prvAutoReloadTimer3Callback(TimerHandle_t xTimer3) {
//	trace_puts( "Auto-reload timer 3 callback executing" );

	if (ReceivedMessages == 500) {
		i++;
		Reset();
	}
	xSemaphoreGive(Receiver);
}

void vApplicationMallocFailedHook(void) {
	for (;;)
		;
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
	(void) pcTaskName;
	(void) pxTask;
	for (;;)
		;
}

void vApplicationIdleHook(void) {
	volatile size_t xFreeStackSpace;
	xFreeStackSpace = xPortGetFreeHeapSize();
	if (xFreeStackSpace > 100) {
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
		StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
		StackType_t **ppxTimerTaskStackBuffer,
		uint32_t *pulTimerTaskStackSize) {
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
