/*
 * app_msgstructs.h
 *
 *  Created on: Oct 20, 2024
 *      Author: anh
 */

#ifndef APP_APP_MSGSTRUCTS_H_
#define APP_APP_MSGSTRUCTS_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "stdint.h"
#include "stdbool.h"





typedef enum {
	VENT_DEVICE 			= 0x01,
	SENSOR_DEVICE	 		= 0x02,
} device_types_t;


typedef enum {
	/**
	 * Device primary actions.
	 */
	APPMSG_CTRLCMD_REQ 					= 0x10,
	APPMSG_CTRLCMD_RES 					= 0x11,

	/**
	 * Pair.
	 */
	APPMSG_PAIR_REQ						= 0x20,

} appmsg_types_t;



typedef struct {
	device_types_t dev_type;
	appmsg_types_t msg_type;
	uint8_t batt_level;
} appmsg_request_t;

typedef struct {
	appmsg_types_t msg_type;
	bool ctrlcmd;
} appmsg_response_t;


uint8_t appmsg_create_message(appmsg_request_t *pinput, uint8_t *poutbuf);
bool appmsg_parse_message(uint8_t *buffer, uint8_t buffersize, appmsg_response_t *poutput, device_types_t devtype);


#ifdef __cplusplus
}
#endif


#endif /* APP_APP_MSGSTRUCTS_H_ */
