/*
 * shtc3.h
 *
 *  Created on: Jan 3, 2025
 *      Author: anh
 */

#ifndef SHTC3_SHTC3_H_
#define SHTC3_SHTC3_H_

#ifdef __cplusplus
extern "C"
#endif

#include "main.h"


uint16_t shtc3_read_id(I2C_HandleTypeDef *hi2c);
uint32_t shtc3_sleep(I2C_HandleTypeDef *hi2c);
uint32_t shtc3_wakeup(I2C_HandleTypeDef *hi2c);
uint32_t shtc3_perform_measurements(I2C_HandleTypeDef *hi2c, int32_t* temp, int32_t* hum);
uint32_t shtc3_perform_measurements_low_power(I2C_HandleTypeDef *hi2c, int32_t* out_temp, int32_t* out_hum);

#ifdef __cplusplus
}
#endif

#endif /* SHTC3_SHTC3_H_ */
