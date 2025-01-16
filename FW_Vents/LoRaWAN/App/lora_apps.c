
#include "platform.h"
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "adc_if.h"
#include "CayenneLpp.h"
#include "sys_sensors.h"
#include "flash_if.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

#include "stdio.h"
#include "string.h"

#include "i2c.h"

#include "logger/logger.h"
#include "app_msgstructs.h"
#include "lora_porting.h"


#define TAG "LoRaWAN-Vent"
#define DEVICE 										VENT_DEVICE
#define LORAWAN_NVM_BASE_ADDRESS                    ((void *)0x0803F000UL)
#define DEFAULT_MAX_SEND_COUNT 						(3U)

#define JOIN_PERIOD_SEC 							(5U)
#define RESEND_DELAY_SEC 							(3U)
#define CTRLCMD_REQUEST_PERIOD_SEC					(20U)



#define ACTION_CTRLCMD_REQUEST  					(uint8_t)APPMSG_CTRLCMD_REQ
#define ACTION_PAIR_REQUEST  						(uint8_t)APPMSG_PAIR_REQ



static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);
static void OnTxData(LmHandlerTxParams_t *params);
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);
static void OnBeaconStatusChange(LmHandlerBeaconParams_t *params);
static void OnSysTimeUpdate(void);
static void OnClassChange(DeviceClass_t deviceClass);
static void StoreContext(void);
static void OnNvmDataChange(LmHandlerNvmContextStates_t state);
static void OnStoreContextRequest(void *nvm, uint32_t nvm_size);
static void OnRestoreContextRequest(void *nvm, uint32_t nvm_size);
static void OnMacProcessNotify(void);
static void OnTxPeriodicityChanged(uint32_t periodicity);
static void OnTxFrameCtrlChanged(LmHandlerMsgTypes_t isTxConfirmed);
static void OnPingSlotPeriodicityChanged(uint8_t pingSlotPeriodicity);
static void OnSystemReset(void);

static uint32_t Get_TimeStamp(void);

static LmHandlerCallbacks_t LmHandlerCallbacks = {
	.GetBatteryLevel =              GetBatteryLevel,
	.GetTemperature =               GetTemperatureLevel,
	.GetUniqueId =                  GetUniqueId,
	.GetDevAddr =                   GetDevAddr,
	.OnRestoreContextRequest =      OnRestoreContextRequest,
	.OnStoreContextRequest =        OnStoreContextRequest,
	.OnMacProcess =                 OnMacProcessNotify,
	.OnNvmDataChange =              OnNvmDataChange,
	.OnJoinRequest =                OnJoinRequest,
	.OnTxData =                     OnTxData,
	.OnRxData =                     OnRxData,
	.OnBeaconStatusChange =         OnBeaconStatusChange,
	.OnSysTimeUpdate =              OnSysTimeUpdate,
	.OnClassChange =                OnClassChange,
	.OnTxPeriodicityChanged =       OnTxPeriodicityChanged,
	.OnTxFrameCtrlChanged =         OnTxFrameCtrlChanged,
	.OnPingSlotPeriodicityChanged = OnPingSlotPeriodicityChanged,
	.OnSystemReset =                OnSystemReset,
};

static LmHandlerParams_t LmHandlerParams = {
	.ActiveRegion =             ACTIVE_REGION,
	.DefaultClass =             LORAWAN_DEFAULT_CLASS,
	.AdrEnable =                LORAWAN_ADR_STATE,
	.IsTxConfirmed =            LORAWAN_DEFAULT_CONFIRMED_MSG_STATE,
	.TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
	.TxPower =                  LORAWAN_DEFAULT_TX_POWER,
	.PingSlotPeriodicity =      LORAWAN_DEFAULT_PING_SLOT_PERIODICITY,
	.RxBCTimeout =              LORAWAN_DEFAULT_CLASS_B_C_RESP_TIMEOUT
};

static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];
static LmHandlerAppData_t AppData = {
	.Port = LORAWAN_USER_APP_PORT,
	.BufferSize = 0,
	.Buffer = AppDataBuffer,
};
static const char *ActionString[] = {
	[APPMSG_CTRLCMD_REQ] 	= "Start send control command request",
	[APPMSG_PAIR_REQ] 		= "Start pair request",
};
static const char *ActionResponsedString[] = {
	[APPMSG_CTRLCMD_RES] 	= "Receive environment data response",
};
static uint32_t uplink_action_count = 0;
static bool is_pair_request = false;
static appmsg_response_t srv_response;

static TaskHandle_t hTask_LoraStoreContext;
static TaskHandle_t hTask_LmHandlerProcess;
static TaskHandle_t hTask_AppProcess;

static QueueHandle_t queue_timer_action;
static SemaphoreHandle_t sem_newoperation_isready;
static SemaphoreHandle_t sem_nextsend_isready;

static TimerEvent_t timer_SendRepeat;
static TimerEvent_t timer_CtrlCmdRequest;
static TimerEvent_t timer_OpenVentIfDisconn;

static void Task_LoraStoreContext(void *);
static void Task_LmHandlerProcess(void *);
static void Task_AppProcess(void *);

static void OnTimerSendRepeat(void*);
static void OnTimerCtrlCmdRequest(void*);
static void OnTimerOpenVentIfDisconn(void*);

static void joined_startapp(void);
static void rejoin_to_network(void);
static void startaction_blockapp(appmsg_types_t);
static void stopaction_releaseapp(void);
static void send_uplink_message(appmsg_types_t type);
static void attemp_tosend_uplink(appmsg_types_t action);
static void prepare_to_resend_uplink(void);







void LoRaWAN_Init(void){
	log_monitor_set_timestamp_cb(Get_TimeStamp);

	queue_timer_action = xQueueCreate(5, sizeof(appmsg_types_t *));
	sem_newoperation_isready = xSemaphoreCreateBinary();
	sem_nextsend_isready = xSemaphoreCreateBinary();
	xSemaphoreGive(sem_nextsend_isready);
	if (xTaskCreate(Task_LmHandlerProcess, "Task_LmHandlerProcess", 4096/4, NULL, 10, &hTask_LmHandlerProcess) != pdPASS){
		LOGE(TAG, "Can't create task Task_LmHandlerProcess.");
		Error_Handler();
	}
	if (xTaskCreate(Task_LoraStoreContext, "Task_LoraStoreContext", 1024/4, NULL, 3, &hTask_LoraStoreContext) != pdPASS){
		LOGE(TAG, "Can't create task Task_LoraStoreContext.");
		Error_Handler();
	}

	if (xTaskCreate(Task_AppProcess, "Task_AppProcess", 8192/4, NULL, 10, &hTask_AppProcess) != pdPASS) {
		LOGE(TAG, "Can't create task Task_AppProcess.");
		Error_Handler();
	}

	TimerInit(&timer_SendRepeat, OnTimerSendRepeat);
	TimerInit(&timer_CtrlCmdRequest, OnTimerCtrlCmdRequest);
	TimerInit(&timer_OpenVentIfDisconn, OnTimerOpenVentIfDisconn);
	TimerSetValue(&timer_CtrlCmdRequest, (CTRLCMD_REQUEST_PERIOD_SEC*1000U));
	TimerSetValue(&timer_OpenVentIfDisconn, 30000U);
	EnableSleepMode(true);

	LoraInfo_Init();
	LmHandlerInit(&LmHandlerCallbacks, APP_VERSION);
	LmHandlerConfigure(&LmHandlerParams);

	LOGI(TAG, "Perform join");
	LmHandlerJoin(LORAWAN_DEFAULT_ACTIVATION_TYPE, LORAWAN_FORCE_REJOIN_AT_BOOT);
}


static void Task_AppProcess(void *){
//	appmsg_types_t action_bootup = ACTION_CTRLCMD_REQUEST;
	static appmsg_types_t action;
	vTaskSuspend(NULL);

//	xQueueSend(queue_timer_action, (void *)&action_bootup, 100);
	while (1){
		if (!LmHandlerIsBusy()){
			if (xSemaphoreTake(sem_newoperation_isready, portMAX_DELAY)){
				if (xQueueReceive(queue_timer_action, (void *)&action, portMAX_DELAY))
					startaction_blockapp(action);
			}
		}
	}
}

static void Task_LmHandlerProcess(void *) {
	uint32_t notify_val;
	while (1) {
		xTaskNotifyWait(0, 0, &notify_val, portMAX_DELAY);
		LmHandlerProcess();
		(void)notify_val;
	}
}


static void Task_LoraStoreContext(void *) {
	uint32_t notify_val;
	while (1) {
		xTaskNotifyWait(0, 0, &notify_val, portMAX_DELAY);
		StoreContext();
		(void)notify_val;
	}
}




static void OnJoinRequest(LmHandlerJoinParams_t *joinParams){
	if (joinParams->Status == LORAMAC_HANDLER_SUCCESS) {
		LOGI(TAG, "Joined.");
		joined_startapp();
	}
	else {
		LOGE(TAG, "Join failed, retry after %ds", JOIN_PERIOD_SEC);
		EnterSleepModeOn(JOIN_PERIOD_SEC*1000U);
		LOGI(TAG, "Perform rejoin");
		LmHandlerJoin(LORAWAN_DEFAULT_ACTIVATION_TYPE, LORAWAN_FORCE_REJOIN_AT_BOOT);
	}
}

static void OnTxData(LmHandlerTxParams_t *params){
	if (params->AppData.BufferSize > 0)
		LOGI(TAG, "Sent uplink");
	if (is_pair_request) {
		stopaction_releaseapp();
		is_pair_request = false;
	}
	else
		prepare_to_resend_uplink();
}

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params){
	if (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET){
		if (appData->BufferSize != 0){
			if (appmsg_parse_message(appData->Buffer, appData->BufferSize, &srv_response, DEVICE)){
				LOGV(TAG, "%s", ActionResponsedString[srv_response.msg_type]);
				if (srv_response.ctrlcmd) LOGM(TAG, "Vent OPEN.");
				else LOGM(TAG, "Vent CLOSE.");
			}
			else
				goto action_done;
		}
		else {
//			if(uplink_action_count >= DEFAULT_MAX_SEND_COUNT){
//				LOGE(TAG, "Action failed, rejoin");
//				rejoin_to_network();
//			}
			return;
		}

action_done:
		stopaction_releaseapp();
	}
}



static void OnSysTimeUpdate(void){
	LOGI(TAG, "System time is up to date");
}



static void joined_startapp(void) {
	TimerStop(&timer_OpenVentIfDisconn);
	TimerStop(&timer_CtrlCmdRequest);
	TimerStart(&timer_CtrlCmdRequest);

	appmsg_types_t action_bootup = ACTION_CTRLCMD_REQUEST;
	xQueueSend(queue_timer_action, (void *)&action_bootup, 100);

	if(xSemaphoreGive(sem_newoperation_isready) != pdPASS)
		LOGE(TAG, "Semaphore give failed");

	vTaskResume(hTask_AppProcess);
}

static void rejoin_to_network(void) {
	TimerStart(&timer_OpenVentIfDisconn);
	TimerStop(&timer_CtrlCmdRequest);
	uplink_action_count = 0;
	vTaskSuspend(hTask_AppProcess);
	LOGI(TAG, "Perform rejoin");
	LmHandlerJoin(LORAWAN_DEFAULT_ACTIVATION_TYPE, LORAWAN_FORCE_REJOIN_AT_BOOT);
}



static void startaction_blockapp(appmsg_types_t action) {
	LOGV(TAG, "%s", ActionString[action]);
	send_uplink_message(action);

	timer_SendRepeat.argument = &action;
	TimerSetValue(&timer_SendRepeat, 200);
	TimerStart(&timer_SendRepeat);

	vTaskSuspend(hTask_AppProcess);
}

static void stopaction_releaseapp(void) {
	TimerStop(&timer_SendRepeat);

	xSemaphoreTake(sem_newoperation_isready, 10);
	if(xSemaphoreGive(sem_newoperation_isready) != pdPASS)
		LOGE(TAG, "Semaphore give failed"); /** cc: Lâu lâu bug ở đây */

	uplink_action_count = 0;
	vTaskResume(hTask_AppProcess);

	LOGV(TAG, "End");
	EnterSleepMode();
}

static void send_uplink_message(appmsg_types_t type){
	float batt_level;

	if (type == APPMSG_CTRLCMD_REQ)
		batt_level = GetBatteryLevel();

	appmsg_request_t msg = {
		.dev_type 			= DEVICE,
		.msg_type    		= type,
		.batt_level			= batt_level,
	};

	AppData.BufferSize = appmsg_create_message(&msg, AppDataBuffer);
}

static void attemp_tosend_uplink(appmsg_types_t action) {
	LmHandlerErrorStatus_t status;

	uplink_action_count++;
	UTIL_TIMER_Stop(&timer_SendRepeat);

	if (uplink_action_count <= DEFAULT_MAX_SEND_COUNT) {
		status = LmHandlerSend(&AppData, LmHandlerParams.IsTxConfirmed, false);
		if (status != LORAMAC_HANDLER_SUCCESS)
			LOGE(TAG, "Send uplink message failed");
	}
	if (action == ACTION_PAIR_REQUEST) is_pair_request = true;

	BaseType_t yield;
	xSemaphoreGiveFromISR(sem_nextsend_isready, &yield);
	if(yield) portYIELD_FROM_ISR(yield);
}

static void prepare_to_resend_uplink(void) {
	if (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET && xSemaphoreTake(sem_nextsend_isready, 50)){
		if (uplink_action_count < DEFAULT_MAX_SEND_COUNT){
			uint32_t nexttx = RESEND_DELAY_SEC * 1000U;
			LOGW(TAG, "Resend uplink after %dms if nothing is received.", nexttx);
			UTIL_TIMER_Stop(&timer_SendRepeat);
			UTIL_TIMER_StartWithPeriod(&timer_SendRepeat, nexttx);
		}
		else {
			LOGW(TAG, "Tried again 3 times but got nothing");
			if(uplink_action_count >= DEFAULT_MAX_SEND_COUNT){
				LOGE(TAG, "Action failed, rejoin");
				rejoin_to_network();
			}
		}
	}
}





static void OnTimerSendRepeat(void *action) {
	appmsg_types_t *act = (appmsg_types_t *)action;
	attemp_tosend_uplink(*act);
}

static void OnTimerCtrlCmdRequest(void*){
	TimerStop(&timer_CtrlCmdRequest);
	TimerStart(&timer_CtrlCmdRequest);

	BaseType_t yield;
	appmsg_types_t action = ACTION_CTRLCMD_REQUEST;
	xQueueSendFromISR(queue_timer_action, (void *)&action, &yield);
	if(yield) portYIELD_FROM_ISR(yield);
}

static void OnTimerOpenVentIfDisconn(void*){
	LOGM(TAG, "Vent OPEN.");
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if (GPIO_Pin == GPIO_PIN_7){
		BaseType_t yield;
		appmsg_types_t action = ACTION_PAIR_REQUEST;
		xQueueSendFromISR(queue_timer_action, (void *)&action, &yield);
		if(yield) portYIELD_FROM_ISR(yield);
	}
}



static void OnTxFrameCtrlChanged(LmHandlerMsgTypes_t isTxConfirmed) {
	LmHandlerParams.IsTxConfirmed = isTxConfirmed;
}

static void OnPingSlotPeriodicityChanged(uint8_t pingSlotPeriodicity) {
	LmHandlerParams.PingSlotPeriodicity = pingSlotPeriodicity;
}

static void OnMacProcessNotify(void){
	if(!xPortIsInsideInterrupt()) xTaskNotify(hTask_LmHandlerProcess, 1, eNoAction);
	else {
		BaseType_t yield;
		xTaskNotifyFromISR(hTask_LmHandlerProcess, 1, eNoAction, &yield);
		if(yield) portEND_SWITCHING_ISR (yield);
	}
}

static void OnBeaconStatusChange(LmHandlerBeaconParams_t *params){}
static void OnClassChange(DeviceClass_t deviceClass){}
static void OnTxPeriodicityChanged(uint32_t periodicity) {}

static void OnSystemReset(void) {
	if ((LORAMAC_HANDLER_SUCCESS == LmHandlerHalt()) && (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET))
		NVIC_SystemReset();
}

static void StoreContext(void) {
	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;
	status = LmHandlerNvmDataStore();
	LOGI(TAG, (status == LORAMAC_HANDLER_NVM_DATA_UP_TO_DATE)? "NVM DATA UP TO DATE" : "NVM DATA STORE FAILED");
}

static void OnNvmDataChange(LmHandlerNvmContextStates_t state) {
	LOGI(TAG, (state == LORAMAC_HANDLER_NVM_STORE)? "NVM DATA STORED" : "NVM DATA RESTORED");
}

static void OnStoreContextRequest(void *nvm, uint32_t nvm_size) {
	if (FLASH_IF_Erase(LORAWAN_NVM_BASE_ADDRESS, FLASH_PAGE_SIZE) == FLASH_IF_OK) {
		FLASH_IF_Write(LORAWAN_NVM_BASE_ADDRESS, (const void*) nvm, nvm_size);
	}
}

static void OnRestoreContextRequest(void *nvm, uint32_t nvm_size) {
	FLASH_IF_Read(nvm, LORAWAN_NVM_BASE_ADDRESS, nvm_size);
}

static uint32_t Get_TimeStamp(void){
	return SysTimeToMs(SysTimeGet());
}

