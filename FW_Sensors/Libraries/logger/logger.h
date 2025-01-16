/*
 * TraceLog.h
 *
 *  Created on: Jun 18, 2024
 *      Author: anh
 */

#ifndef TRACE_LOG_H_
#define TRACE_LOG_H_


#include "stdio.h"
#include "stdarg.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C"{
#endif


typedef enum{
	SIMP_BLACK = 0,
	SIMP_RED,
	SIMP_GREEN,
	SIMP_YELLOW,
	SIMP_BLUE,
	SIMP_PURPLE,
	SIMP_CYAN,
	SIMP_WHITE,

	BOLD_BLACK = 8,
	BOLD_RED,
	BOLD_GREEN,
	BOLD_YELLOW,
	BOLD_BLUE,
	BOLD_PURPLE,
	BOLD_CYAN,
	BOLD_WHITE,

	ITALIC_BLACK = 16,
	ITALIC_RED,
	ITALIC_GREEN,
	ITALIC_YELLOW,
	ITALIC_BLUE,
	ITALIC_PURPLE,
	ITALIC_CYAN,
	ITALIC_WHITE,

	BCKGRN_BLACK = 24,
	BCKGRN_RED,
	BCKGRN_GREEN,
	BCKGRN_YELLOW,
	BCKGRN_BLUE,
	BCKGRN_PURPLE,
	BCKGRN_CYAN,
	BCKGRN_WHITE,
} log_type_t;

#define LOG_MESS(__plog__, __tag__, __mess__) __plog__(__tag__, "%s[%d]>>> %s", CODE_FUNC, CODE_LINE, __mess__);


void log_monitor_init(void (*PrintString_Function)(char*));
void log_monitor_set_timestamp_cb(uint32_t (*TimeStamp_Function)(void));

void log_monitor_set_log(char *func, log_type_t log_type);

void LOG(log_type_t log_type, const char *tag, const char *format, ...);

void LOGI (const char *tag,  const char *format, ...);
void LOGW (const char *tag,  const char *format, ...);
void LOGE(const char *tag,  const char *format, ...);
void LOGD(const char *tag,  const char *format, ...);
void LOGV(const char *tag,  const char *format, ...);
void LOGM  (const char *tag,  const char *format, ...);
void LOGR  (const char *tag,  const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* TRACE_LOG_H_ */
