/*
 * lora_porting.c
 *
 *  Created on: Oct 20, 2024
 *      Author: anh
 */

#include "lora_porting.h"
#include "timer.h"
#include "radio.h"
#include "adc_if.h"



static TimerEvent_t timer_sleep;
static bool timer_created = false;
static bool enable_sleep = true;



static void OnWakeUp(void *);




void EnableSleepMode(bool ena){
	enable_sleep = ena;
}

void EnterSleepMode(void){
	if (!enable_sleep) return;

  	HAL_SuspendTick();
	HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

	extern HAL_StatusTypeDef HAL_InitTick(uint32_t);
	HAL_InitTick(15);
	HAL_ResumeTick();
	extern void SystemClock_Config(void);
	SystemClock_Config();
}

void EnterSleepModeOn(uint32_t time){
	if (timer_created == false) {
		TimerInit(&timer_sleep, OnWakeUp);
		timer_created = true;
	}
	if (!enable_sleep) return;

	TimerSetValue(&timer_sleep, time);
	TimerStart(&timer_sleep);

	EnterSleepMode();
}



static void OnWakeUp(void *){
	TimerStop(&timer_sleep);
}



