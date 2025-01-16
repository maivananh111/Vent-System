/*
 * lora_porting.h
 *
 *  Created on: Oct 20, 2024
 *      Author: anh
 */

#ifndef PORTING_LORA_PORTING_H_
#define PORTING_LORA_PORTING_H_

#ifdef __cplusplus
extern "C"{
#endif


#include "stm32wlxx_hal.h"
#include "stdbool.h"




void EnableSleepMode(bool ena);
void EnterSleepMode(void);
void EnterSleepModeOn(uint32_t time);

#ifdef __cplusplus
}
#endif
#endif /* PORTING_LORA_PORTING_H_ */
