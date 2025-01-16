/*
 * app_msgstructs.c
 *
 *  Created on: Oct 20, 2024
 *      Author: anh
 */

#include "app_msgstructs.h"
#include "stdlib.h"







uint8_t appmsg_create_message(appmsg_request_t *pinput, uint8_t *poutbuf){
	uint16_t temp, humi;
	temp = (uint16_t)(pinput->temperature_value*100);
	humi = (uint16_t)(pinput->humidity_value*100);

	poutbuf[0] = (uint8_t)pinput->dev_type;
	poutbuf[1] = (uint8_t)pinput->msg_type;

	switch (pinput->msg_type){
		case APPMSG_ENVDATA_REQ:
			poutbuf[2] = (uint8_t)((temp >> 8) & 0xFF);
			poutbuf[3] = (uint8_t)((temp >> 0) & 0xFF);
			poutbuf[4] = (uint8_t)((humi >> 8) & 0xFF);
			poutbuf[5] = (uint8_t)((humi >> 0) & 0xFF);
			poutbuf[6] = pinput->batt_level;
			return 7;

		case APPMSG_PAIR_REQ:
		case APPMSG_PAIR_STAT_REQ:
			return 2;

		default:
		break;
	}

	return 0;
}

bool appmsg_parse_message(uint8_t *buffer, uint8_t buffersize, appmsg_response_t *poutput, device_types_t devtype){
	if (buffersize == 0 || buffer == NULL)
		return false;
	if (buffer[0] != devtype)
		return false;

	poutput->msg_type = (appmsg_types_t)buffer[1];
	if (buffersize == 4 && poutput->msg_type == APPMSG_ENVDATA_RES){
		uint16_t tempset = (uint16_t)((buffer[2] << 8) | buffer[3]);
		poutput->temperature_set = (float)((float)tempset / 100.0);
	}
	else if (buffersize == 3 && poutput->msg_type == APPMSG_PAIR_STAT_RES){
		poutput->is_paired = (bool)buffer[2];
	}

	return true;
}





