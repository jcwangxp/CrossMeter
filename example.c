#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crossmeter.h"

/*

Build

#Windows MSVC
	cl -D_CRT_SECURE_NO_WARNINGS -W4 User32.Lib crossmeter.c example.c /Feexample.exe

#GCC(Linux, MinGW, Cygwin, MSYS2)
	gcc -Wall -O3 crossmeter.c  example.c -pthread -o example

*/

#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#else
#include <windows.h>
#define localtime_r(timep, result)	localtime_s(result, timep)
typedef DWORD pthread_t;
int pthread_create(pthread_t *thread, const void *attr, void *(*start_routine) (void *), void *arg)
{
	(void) attr;
	CreateThread (0, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, thread);
	return 0;
}
#endif

void msleep(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static volatile int cfg_speed = 100, bPrintThread = 0, bRun = 1;

static void dump_speed (crossmeter_t	*pCM, int bReal)
{
	uint32_t			i, real, peak, len;
    struct 	timeval 	ts;
	struct tm 			tm_val;
	time_t 				time_val;
	char 				timestamp[30];

	real = cm_realspeed_get(pCM, bReal);
	printf ("CfgSpeed:%5u RealSpeed:%5u PeakSpeed/TS", cfg_speed, real);
	for (i = 0; i < CM_PEAK_NUM; ++i) {
		peak = cm_peakspeed_get(pCM, i, &ts);
		time_val = (time_t)ts.tv_sec;
		localtime_r(&time_val, &tm_val);
		len = (uint32_t)strftime(timestamp, 20, "%Y-%m-%dT%H:%M:%S", &tm_val);
		sprintf (timestamp+len, "+%06d", (int)ts.tv_usec);
		printf (" [%ds]%u %s", cm_peakperiod_get(pCM, i), peak, timestamp);
	}
	printf ("\n");
}

static void *test_thread (void *arg)
{
    struct 	timeval ts;
	crossmeter_t	*pCM = arg;
	uint64_t		cur_ts, last_ts = 0, last_print=0;
	uint32_t		speed;

	while (bRun) {
	    cm_gettimeofday(&ts);
		cur_ts = ts.tv_sec * 1000000 + ts.tv_usec;
		speed = cfg_speed;
		if (speed && (cur_ts - last_ts >= 1000000/speed)) {
			cm_speed_feed (pCM, 1, NULL);	// Use real-time TS
			// cm_speed_feed (pCM, 1, &ts); // Use passed TS
			last_ts = cur_ts;
		}

		if (!bPrintThread && (cur_ts - last_print >= 1000000/5)) {
			dump_speed (pCM, 1);
			last_print = cur_ts;
		}
	}

    return NULL;
}

static void *print_thread (void *arg)
{
	while (bRun) {
		dump_speed (arg, 0);
		msleep(200);
	}
    return NULL;
}

int main (int argc, char ** argv)
{
	uint32_t		period[CM_PEAK_NUM], i, len;
    pthread_t 		tid;
	crossmeter_t	sm;

	if (argc <= 1) {
		printf ("%s period   : start test with peak period\n", argv[0]);
		printf ("%s period 1 : start test with peak period and start print thread also\n", argv[0]);
		printf ("You can input speed value during running\n");
		return 0;
	}

	period[0] = atoi(argv[1]);
	period[1] = period[0] * 4;
	period[2] = period[1] * 4;
	cm_speed_init (&sm, period);
	if (argc > 2) {
		printf ("Start print thread\n");
		bPrintThread = 1;
		pthread_create (&tid, NULL, print_thread, &sm);
	}
	pthread_create (&tid, NULL, test_thread, &sm);

	while (1) {
		scanf ("%d", &cfg_speed);
		if (cfg_speed < 0) {
			uint32_t 		peak_speed[CM_PEAK_NUM];
			uint32_t		peak_period[CM_PEAK_NUM];
			struct timeval 	peak_ts[CM_PEAK_NUM];
			struct tm 		tm_val;
			time_t 			time_val;
			char 			timestamp[30];

			bRun = 0;
			msleep (1);
			cm_peakspeed_getall(&sm, peak_speed, peak_ts);
			cm_peakperiod_getall (&sm, peak_period);
			printf ("CfgSpeed:%5u RealSpeed:%5u PeakSpeed/TS", 0, cm_realspeed_get(&sm, 0));
			for (i = 0; i < CM_PEAK_NUM; ++i) {
				time_val = (time_t)peak_ts[i].tv_sec;
				localtime_r(&time_val, &tm_val);
				len = (uint32_t)strftime(timestamp, 20, "%Y-%m-%dT%H:%M:%S", &tm_val);
				sprintf (timestamp+len, "+%06d", (int)peak_ts[i].tv_usec);
				printf (" [%ds]%u %s", peak_period[i], peak_speed[i], timestamp);
			}
			dump_speed (&sm, 0);
			break;
		} else {
			printf ("Config speed %d\n", cfg_speed);
		}
	}

	return 0;
}
