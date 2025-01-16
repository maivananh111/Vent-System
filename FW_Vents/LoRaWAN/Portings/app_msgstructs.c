/*
 * app_msgstructs.c
 *
 *  Created on: Oct 20, 2024
 *      Author: anh
 */

#include "app_msgstructs.h"
#include "stdlib.h"







uint8_t appmsg_create_message(appmsg_request_t *pinput, uint8_t *poutbuf){
	poutbuf[0] = (uint8_t)pinput->dev_type;
	poutbuf[1] = (uint8_t)pinput->msg_type;

	switch (pinput->msg_type){
		case APPMSG_CTRLCMD_REQ:
			poutbuf[2] = pinput->batt_level;
			return 3;

		case APPMSG_PAIR_REQ:
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
	if (buffersize == 3 && poutput->msg_type == APPMSG_CTRLCMD_RES){
		poutput->ctrlcmd = (bool)buffer[2];
	}

	return true;
}





