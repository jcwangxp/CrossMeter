/* crossmeter.h -- Version 1.0
 *
 * CrossMeter is a tiny/timer free/low memory footprint/large scaling/high
 *   performance/multiple period peak/accurate speed measure library.
 *
 * You can find the latest source code and description at:
 *   https://github.com/jcwangxp/crossmeter
 *
 * ------------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2023, JC Wang (wang_junchuan@163.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ------------------------------------------------------------------------
 */

#ifndef __CORSSMETER_H
#define __CORSSMETER_H

#include <time.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifdef _WIN32
// Cross-platform gettimeofday function 
extern int cm_gettimeofday (struct timeval *tv);
#else
#define cm_gettimeofday(tv)		gettimeofday (tv, NULL)
#endif

typedef enum {
	CM_PEAK_SHORT	= 0,	// default hour
	CM_PEAK_MEDIUM	= 1,	// default day
	CM_PEAK_LONG	= 2,	// default month (30day)
	CM_PEAK_NUM		= 3
} cm_period_e;

#define CM_TIME_SLOT		10
typedef struct {
	struct timeval	last_ts;					// last feed timestamp
	struct timeval	peak_ts[CM_PEAK_NUM];		// peak speed timestamp
	uint32_t		peak_speed[CM_PEAK_NUM];	// peak speed value
	uint32_t		peak_period[CM_PEAK_NUM];	// eak period, default is day
	uint32_t		speed_slice[CM_TIME_SLOT];	// real speed value slice in each time slot
	uint32_t		real_speed;					// real-time speed value
} crossmeter_t;


/* Init crossmeter_t, set peak_period in sec(if=NULL will set to default: hour/day/month)
 */
extern void     cm_speed_init (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM]);

/* Feed speed with incremental val, return real-time speed, not thread-safe
 * pTS is user provided timestamp, can be NULL then use current TS
 */
extern uint32_t cm_speed_feed (crossmeter_t *pCM, uint32_t inc_val, struct timeval *pTS);

/* Get real-time speed
 * bReal=1: Get accurate real-time speed, not thread-safe with sm_speed_feed thread
 * bReal=0: Get real-time speed of last feed, thread-safe with sm_speed_feed thread
 */
extern uint32_t cm_realspeed_get (crossmeter_t *pCM, int bReal);

/* Get peak speed, pPeakTs saves the timestamp(can be NULL), can be called in other thread
 * period_type can be CM_PEAK_SHORT CM_PEAK_MEDIUM and CM_PEAK_LONG
 */
extern uint32_t cm_peakspeed_get (crossmeter_t *pCM, cm_period_e period_type, struct timeval *pPeakTS);

/* Get all peak speed (CM_PEAK_NUM), can be called in other thread
 * pPeakTS is array, can be NULL
 */
extern void     cm_peakspeed_getall (crossmeter_t *pCM, uint32_t pPeakSpeed[CM_PEAK_NUM], struct timeval pPeakTS[CM_PEAK_NUM]);

/* Get single peak speed period in sec
 */
extern uint32_t cm_peakperiod_get (crossmeter_t *pCM, cm_period_e period_type);

/* Get all peak speed period in sec
 */
extern void     cm_peakperiod_getall (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM]);

/* Set peak speed period in sec(if=0 will set to default: hour/day/month)
 */
extern void     cm_peakperiod_set (crossmeter_t *pCM, cm_period_e period_type, uint32_t peak_period);

/* Set peak speed period in sec(if=NULL will set to default: hour/day/month)
 */
extern void     cm_peakperiod_setall (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM]);


#ifdef __cplusplus
}
#endif

#endif // __CORSSMETER_H
