/* crossmeter.c -- Version 1.0
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

#include <string.h>
#include "crossmeter.h"


#ifdef _WIN32
int cm_gettimeofday(struct timeval *tv)
{
    struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	tv->tv_sec 	= (long)ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}
#endif

/* Init crossmeter_t, set peak_period in sec(if=NULL will set to default: hour/day/month)
 */
void     cm_speed_init (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM])
{
	memset (pCM, 0, sizeof (*pCM));
	cm_peakperiod_setall (pCM, peak_period);
}

static inline void cm_peakspeed_update (crossmeter_t *pCM, uint32_t speed, struct timeval *pTS)
{
    struct timeval	ts;
	uint32_t period = pCM->peak_period[CM_PEAK_SHORT];

	if (NULL == pTS) {
		pTS = &ts;
    	cm_gettimeofday(pTS);
	}

	if (pTS->tv_sec/period > pCM->peak_ts[CM_PEAK_SHORT].tv_sec/period) {
		pCM->peak_speed[CM_PEAK_SHORT] = 0;
		period = pCM->peak_period[CM_PEAK_MEDIUM];
		if (pTS->tv_sec/period > pCM->peak_ts[CM_PEAK_MEDIUM].tv_sec/period) {
			pCM->peak_speed[CM_PEAK_MEDIUM] = speed;
			pCM->peak_ts[CM_PEAK_MEDIUM] = *pTS;
			period = pCM->peak_period[CM_PEAK_LONG];
			if (pTS->tv_sec/period > pCM->peak_ts[CM_PEAK_LONG].tv_sec/period) {
				pCM->peak_speed[CM_PEAK_LONG] = speed;
				pCM->peak_ts[CM_PEAK_LONG] = *pTS;
			}
		}
	}

	if (speed > pCM->peak_speed[CM_PEAK_SHORT]) {
		pCM->peak_speed[CM_PEAK_SHORT] = speed;
		pCM->peak_ts[CM_PEAK_SHORT] = *pTS;		
		if (speed > pCM->peak_speed[CM_PEAK_MEDIUM]) {
			pCM->peak_speed[CM_PEAK_MEDIUM] = speed;
			pCM->peak_ts[CM_PEAK_MEDIUM] = *pTS;		
			if (speed > pCM->peak_speed[CM_PEAK_LONG]) {
				pCM->peak_speed[CM_PEAK_LONG] = speed;
				pCM->peak_ts[CM_PEAK_LONG] = *pTS;		
			}
		}
	}
}

/* Feed speed with incremental val, return real-time speed, not thread-safe
 * pTS is user provided timestamp, can be NULL then use current TS
 */
uint32_t cm_speed_feed (crossmeter_t *pCM, uint32_t inc_val, struct timeval *pTS)
{
    struct timeval	ts;
	uint32_t		sec_diff, us_diff, i, sum_rm = 0, cur_slot = 0, last_slot = 0;

	if (NULL == pTS) {
		pTS = &ts;
    	cm_gettimeofday(pTS);
	}
	if ((pTS->tv_sec < pCM->last_ts.tv_sec) || 
		((pTS->tv_sec == pCM->last_ts.tv_sec) && (pTS->tv_usec < pCM->last_ts.tv_usec)))  {
		return pCM->real_speed;
	}
	sec_diff = pTS->tv_sec - pCM->last_ts.tv_sec;
	us_diff  = sec_diff * 1000000 + pTS->tv_usec - pCM->last_ts.tv_usec;
	cur_slot = pTS->tv_usec / (1000000/CM_TIME_SLOT);

	if ((sec_diff > 1) || (us_diff >= 1000000)) {
		memset (pCM->speed_slice, 0, sizeof(pCM->speed_slice));
		pCM->real_speed = 0;
	} else {
		last_slot = pCM->last_ts.tv_usec / (1000000/CM_TIME_SLOT);
		if (cur_slot > last_slot) {
			for (i = last_slot+1; i <= cur_slot; ++i) {
				sum_rm += pCM->speed_slice[i];
				pCM->speed_slice[i] = 0;
			}
		} else if (cur_slot < last_slot) {
			for (i = last_slot+1; i < CM_TIME_SLOT; ++i) {
				sum_rm += pCM->speed_slice[i];
				pCM->speed_slice[i] = 0;
			}
			for (i = 0; i <= cur_slot; ++i) {
				sum_rm += pCM->speed_slice[i];
				pCM->speed_slice[i] = 0;
			}
		}
	}

 	pCM->speed_slice[cur_slot] += inc_val;
	pCM->real_speed += inc_val;
	pCM->real_speed -= sum_rm;

	cm_peakspeed_update (pCM, pCM->real_speed, pTS);

	pCM->last_ts = *pTS;

	return pCM->real_speed;
}

/* Get real-time speed
 * bReal=1: Get accurate real-time speed, not thread-safe with cm_speed_feed thread
 * bReal=0: Get real-time speed of last feed, thread-safe with cm_speed_feed thread
 */
uint32_t cm_realspeed_get (crossmeter_t *pCM, int bReal)
{
    struct 	timeval	ts;
	int		sec_diff, us_diff;

	if (bReal) {
		return cm_speed_feed (pCM, 0, NULL);
	}

	cm_gettimeofday(&ts);
	sec_diff = ts.tv_sec - pCM->last_ts.tv_sec;
	us_diff  = sec_diff * 1000000 + ts.tv_usec - pCM->last_ts.tv_usec;

	if ((sec_diff > 1) || (us_diff >= 1000000)) {
		return 0;
	}
	return pCM->real_speed;
}

/* Get peak speed, pPeakTs saves the timestamp(can be NULL), can be called in other thread
 * period_type can be CM_PEAK_SHORT CM_PEAK_MEDIUM and CM_PEAK_LONG
 */
uint32_t cm_peakspeed_get (crossmeter_t *pCM, cm_period_e period_type, struct timeval *pPeakTS)
{
	if (period_type >= CM_PEAK_NUM) {
		return 0;
	}

	cm_peakspeed_update (pCM, 0 , NULL);
	if (NULL != pPeakTS) {
		*pPeakTS = pCM->peak_ts[period_type];
	}
	return pCM->peak_speed[period_type];
}

/* Get all peak speed (CM_PEAK_NUM), can be called in other thread
 * pPeakTS is array, can be NULL
 */
void     cm_peakspeed_getall (crossmeter_t *pCM, uint32_t pPeakSpeed[CM_PEAK_NUM], struct timeval pPeakTS[CM_PEAK_NUM])
{
	cm_peakspeed_update (pCM, 0 , NULL);

	if (NULL != pPeakSpeed) {
		memcpy (pPeakSpeed, pCM->peak_speed, sizeof (pCM->peak_speed));
	}
	if (NULL != pPeakTS) {
		memcpy (pPeakTS, pCM->peak_ts, sizeof (pCM->peak_ts));
	}
}

/* Get single peak speed period in sec
 */
uint32_t cm_peakperiod_get (crossmeter_t *pCM, cm_period_e period_type)
{
	return (period_type >= CM_PEAK_NUM) ? 0 : pCM->peak_period[period_type];
}

/* Get all peak speed period in sec
 */
void     cm_peakperiod_getall (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM])
{
	memcpy (peak_period, pCM->peak_period, sizeof (pCM->peak_period));
}

/* Set single peak speed period in sec(if=0 will set to default: hour/day/month)
 */
void     cm_peakperiod_set (crossmeter_t *pCM, cm_period_e period_type, uint32_t peak_period)
{
	uint32_t	period_default[CM_PEAK_NUM] = {60*60, 24*60*60, 30*24*60*60};

	if (period_type < CM_PEAK_NUM) {
		pCM->peak_period[period_type] = (peak_period>0) ? peak_period : period_default[period_type];
	}
}

/* Set all peak speed period in sec(if=NULL will set to default: hour/day/month)
 */
void     cm_peakperiod_setall (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM])
{
	uint32_t	period_default[CM_PEAK_NUM] = {60*60, 24*60*60, 30*24*60*60};
	memcpy (pCM->peak_period, peak_period?peak_period:period_default, sizeof (pCM->peak_period));
}

