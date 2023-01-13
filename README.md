# CrossMeter

**CrossMeter** is a tiny cross-platform fast speed measure library.

## Catalogue

* [Features and Highlights](#Features-and-Highlights)
* [Where to Use](#Where-to-Use)
* [APIs](#APIs)
* [Usage](#Usage)
* [Example](#Example)
* [Build and Test](#build-and-Test)


## Features and Highlights

* Very tiny
* High performance
* Low memory footprint
* Doesn’t user timer
* Support large number of speed measurement objects
* Support there configurable period peak speed and timestamp: short, medium, long
* Accurate speed measurement
* Independent peak speed period for each speed measurement object
* Support many platforms: Windows, Linux, Unix, MacOS.


## Where to Use

It can track any kind of object for example: RPC call, API call, Packet TX/RX, total TX/RX IPC, specific IPC type TX/RX, memory alloc etc.
For a certain tracking object, you can use the API to do
* Get real-time speed(per sec). If it’s higher than a threshold, you can take some action like log the event, drop or delay etc.
* It can track hour/day/month’s peak speed and timestamp, which can help to tell whether there was performance issue happened in the past, and each tracking object can config different peak speed period for short/medium/long, default is hour/day/month.


## APIs

```c
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

// Cross-platform gettimeofday function 
extern int cm_gettimeofday (struct timeval *tv);

/* Init crossmeter_t, set peak_period in sec(if=NULL will set to default: hour/day/month)
 */
extern void     cm_speed_init (crossmeter_t *pCM, uint32_t peak_period[CM_PEAK_NUM]);

/* Feed speed with incremental val, return real-time speed, not thread-safe
 * pTS is user provided timestamp, can be NULL then use current TS
 */
extern uint32_t cm_speed_feed (crossmeter_t *pCM, uint32_t inc_val, struct timeval *pTS);

/* Get real-time speed
 * bReal=1: Get accurate real-time speed, not thread-safe with cm_speed_feed thread
 * bReal=0: Get real-time speed of last feed, thread-safe with cm_speed_feed thread
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
```


## Usage

* #include "crossmeter.h"

* Embed crossmeter_t to your speed measurement object structure.

* Call cm_speed_int to init speedmeter_t and set short/medium/long peak speed period (NULL will set to default: hour/day/month).

* Call cm_speed_feed to feed incremental value, in most case it’s 1. User can choose to pass a timestamp(can use portable cm_gettimeofday) or pass NULL to use current timestamp. cm_speed_feed will return real-time speed. 
  `Note: cm_speed_feed is not thread-safe, so if there’re multiple feeding thread, please make sure it’s called with protection.`

* Call cm_realspeed_get to get real-time speed. If the getting is in the same thread with feeding or there’s protection, then bReal=1 will return real-time speed, otherwise if there is no protection, you can use bReal=0 to get real-time speed calculated in last feeding and if current getting time has elapsed more than 1s from last feeding, it’ll return 0.

* Call cm_peakspeed_get to get specific period peak speed or cm_peakspeed_getall to get all periods' peak speed. It can be called from different thread with feeding.

* Call cm_peakperiod_get/cm_peakperiod_set to get/set specific period peak period, or call call cm_peakperiod_getall/cm_peakperiod_setall to get/set all peak period’s peak period.


## Example

This is an example and also a test program. After running, it’ll keep printing real-time speed and all peak speed, and you can input a speed value during running to change the feeding speed.

```c
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
```


## Build and Test

On Windows, you can add the source code to a Visual Studio project to build or enter `Tools Command Prompt for VS` from menu to build in command line which is more efficient.

**Windows MSVC**

    cl -D_CRT_SECURE_NO_WARNINGS -W4 User32.Lib crossmeter.c example.c /Feexample.exe

**GCC(Linux, MinGW, Cygwin, MSYS2)**

    gcc -Wall -O3 crossmeter.c  example.c -pthread -o example

Command Line
```
example period   : start test with peak period
example period 1 : start test with peak period and start print thread also
You can input speed value during running
```

Run with peak period=5s/20s/80s. (set short/medium/long to small value is to see test result quickly)

```
./example 5

Config speed 50   <input 50>
CfgSpeed:   50 RealSpeed:   94 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   50 RealSpeed:   84 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   50 RealSpeed:   74 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   50 RealSpeed:   64 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   50 RealSpeed:   54 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   50 RealSpeed:   50 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306

Config speed 20  <input 20>
CfgSpeed:   20 RealSpeed:   49 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   20 RealSpeed:   43 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   20 RealSpeed:   37 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   20 RealSpeed:   31 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   20 RealSpeed:   25 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   20 RealSpeed:   20 PeakSpeed/TS [5s]100 2023-01-10T14:17:00+097306 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306

Config speed 10  <input 10>
CfgSpeed:   10 RealSpeed:   18 PeakSpeed/TS [5s]20 2023-01-10T14:17:05+067344 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   10 RealSpeed:   16 PeakSpeed/TS [5s]20 2023-01-10T14:17:05+067344 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   10 RealSpeed:   14 PeakSpeed/TS [5s]20 2023-01-10T14:17:05+067344 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   10 RealSpeed:   12 PeakSpeed/TS [5s]20 2023-01-10T14:17:05+067344 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306
CfgSpeed:   10 RealSpeed:   10 PeakSpeed/TS [5s]20 2023-01-10T14:17:05+067344 [20s]100 2023-01-10T14:17:00+097306 [80s]100 2023-01-10T14:17:00+097306

CfgSpeed:  200 RealSpeed:  182 PeakSpeed/TS [5s]200 2023-01-10T14:19:45+098667 [20s]500 2023-01-10T14:19:40+098498 [80s]1000 2023-01-10T14:19:18+299543
```

If the getting is in different thread, then calling cm_realspeed_get with bReal=0 will see following result. (Return last feeding calculated speed)

```
./example 5 1

Start print thread
CfgSpeed:  100 RealSpeed:    0 PeakSpeed/TS [5s]0 1970-01-01T08:00:00+000000 [20s]1 2023-01-10T14:34:31+845341 [80s]1 2023-01-10T14:34:31+845341
CfgSpeed:  100 RealSpeed:   22 PeakSpeed/TS [5s]22 2023-01-10T14:34:32+055341 [20s]22 2023-01-10T14:34:32+055341 [80s]22 2023-01-10T14:34:32+055341
CfgSpeed:  100 RealSpeed:   42 PeakSpeed/TS [5s]42 2023-01-10T14:34:32+255341 [20s]42 2023-01-10T14:34:32+255341 [80s]42 2023-01-10T14:34:32+255341
CfgSpeed:  100 RealSpeed:   63 PeakSpeed/TS [5s]63 2023-01-10T14:34:32+465341 [20s]63 2023-01-10T14:34:32+465341 [80s]63 2023-01-10T14:34:32+465341
CfgSpeed:  100 RealSpeed:   84 PeakSpeed/TS [5s]84 2023-01-10T14:34:32+675341 [20s]84 2023-01-10T14:34:32+675341 [80s]84 2023-01-10T14:34:32+675341

Config speed 0
CfgSpeed:    0 RealSpeed:   96 PeakSpeed/TS [5s]96 2023-01-10T14:34:32+795341 [20s]96 2023-01-10T14:34:32+795341 [80s]96 2023-01-10T14:34:32+795341
CfgSpeed:    0 RealSpeed:   96 PeakSpeed/TS [5s]96 2023-01-10T14:34:32+795341 [20s]96 2023-01-10T14:34:32+795341 [80s]96 2023-01-10T14:34:32+795341
CfgSpeed:    0 RealSpeed:   96 PeakSpeed/TS [5s]96 2023-01-10T14:34:32+795341 [20s]96 2023-01-10T14:34:32+795341 [80s]96 2023-01-10T14:34:32+795341
CfgSpeed:    0 RealSpeed:   96 PeakSpeed/TS [5s]96 2023-01-10T14:34:32+795341 [20s]96 2023-01-10T14:34:32+795341 [80s]96 2023-01-10T14:34:32+795341
CfgSpeed:    0 RealSpeed:   96 PeakSpeed/TS [5s]96 2023-01-10T14:34:32+795341 [20s]96 2023-01-10T14:34:32+795341 [80s]96 2023-01-10T14:34:32+795341
CfgSpeed:    0 RealSpeed:    0 PeakSpeed/TS [5s]96 2023-01-10T14:34:32+795341 [20s]96 2023-01-10T14:34:32+795341 [80s]96 2023-01-10T14:34:32+795341

```

If they’re in the same thread, you can call cm_realspeed_get with bReal=1, and you’ll see following result. If there’s thread protection, you can call with bReal=1 in the getting thread to get real-time speed also.

```
./example 5

CfgSpeed:  100 RealSpeed:    1 PeakSpeed/TS [5s]1 2023-01-10T14:35:51+268473 [20s]1 2023-01-10T14:35:51+268473 [80s]1 2023-01-10T14:35:51+268473
CfgSpeed:  100 RealSpeed:   21 PeakSpeed/TS [5s]21 2023-01-10T14:35:51+468470 [20s]21 2023-01-10T14:35:51+468470 [80s]21 2023-01-10T14:35:51+468470
CfgSpeed:  100 RealSpeed:   41 PeakSpeed/TS [5s]41 2023-01-10T14:35:51+668470 [20s]41 2023-01-10T14:35:51+668470 [80s]41 2023-01-10T14:35:51+668470
CfgSpeed:  100 RealSpeed:   60 PeakSpeed/TS [5s]60 2023-01-10T14:35:51+858499 [20s]60 2023-01-10T14:35:51+858499 [80s]60 2023-01-10T14:35:51+858499
CfgSpeed:  100 RealSpeed:   80 PeakSpeed/TS [5s]80 2023-01-10T14:35:52+058526 [20s]80 2023-01-10T14:35:52+058526 [80s]80 2023-01-10T14:35:52+058526
CfgSpeed:  100 RealSpeed:   96 PeakSpeed/TS [5s]96 2023-01-10T14:35:52+258539 [20s]96 2023-01-10T14:35:52+258539 [80s]96 2023-01-10T14:35:52+258539
CfgSpeed:  100 RealSpeed:   96 PeakSpeed/TS [5s]100 2023-01-10T14:35:52+298539 [20s]100 2023-01-10T14:35:52+298539 [80s]100 2023-01-10T14:35:52+298539

Config speed 0
CfgSpeed:    0 RealSpeed:   80 PeakSpeed/TS [5s]100 2023-01-10T14:35:52+298539 [20s]100 2023-01-10T14:35:52+298539 [80s]100 2023-01-10T14:35:52+298539
CfgSpeed:    0 RealSpeed:   60 PeakSpeed/TS [5s]100 2023-01-10T14:35:52+298539 [20s]100 2023-01-10T14:35:52+298539 [80s]100 2023-01-10T14:35:52+298539
CfgSpeed:    0 RealSpeed:   40 PeakSpeed/TS [5s]100 2023-01-10T14:35:52+298539 [20s]100 2023-01-10T14:35:52+298539 [80s]100 2023-01-10T14:35:52+298539
CfgSpeed:    0 RealSpeed:   20 PeakSpeed/TS [5s]100 2023-01-10T14:35:52+298539 [20s]100 2023-01-10T14:35:52+298539 [80s]100 2023-01-10T14:35:52+298539
CfgSpeed:    0 RealSpeed:    0 PeakSpeed/TS [5s]100 2023-01-10T14:35:52+298539 [20s]100 2023-01-10T14:35:52+298539 [80s]100 2023-01-10T14:35:52+298539
```

[Goto Top](#Catalogue)