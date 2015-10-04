/*
Copyright (C) 2007 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "sleep.h"

static unsigned int sleep_granularity;

#define NUMSAMPLES 50

static int get_sleep_granularity()
{
	unsigned long long basetime;
	unsigned long long sleeptime[NUMSAMPLES];
	int i;
	unsigned long long accum;
	unsigned int avg;
	unsigned int min;
	unsigned int max;
	unsigned int maxdist;
	unsigned int samples;
	double stddev;
	double tmpf;

	samples = NUMSAMPLES;

	for(i=0;i<NUMSAMPLES;i++)
	{
		Sys_MicroSleep(1);
		basetime = Sys_IntTime();
		Sys_MicroSleep(1);
		sleeptime[i] = Sys_IntTime() - basetime;
	}

	do
	{
		max = 0;
		min = 1000000;
		accum = 0;
		for(i=0;i<samples;i++)
		{
			accum+= sleeptime[i];
			stddev+= ((double)sleeptime[i])*((double)sleeptime[i]);
		
			if (sleeptime[i] < min)
				min = sleeptime[i];
			if (sleeptime[i] > max)
				max = sleeptime[i];
		}

		avg = accum/samples;

		stddev = 0;
		for(i=0;i<samples;i++)
		{
			tmpf = ((double)sleeptime[i]) - avg;
			tmpf*= tmpf;
			stddev+= tmpf;
		}
		stddev = sqrt(stddev/samples);

		maxdist = 0;
		for(i=0;i<samples;i++)
		{
			if (abs(sleeptime[i]-avg) > maxdist)
				maxdist = abs(sleeptime[i]-avg);
		}

#if 0
		printf("avg: %06d min: %06d max: %06d maxdist: %06d stddev: %.2f\n", avg, min, max, maxdist, (float)stddev);
#endif

		for(i=0;i<samples;i++)
		{
			if (abs(sleeptime[i]-avg) == maxdist)
			{
				memmove(&sleeptime[i], &sleeptime[i+1], (samples-i-1)*sizeof(*sleeptime));
				samples--;
				break;
			}
		}
	} while(stddev > (((double)avg)*0.01) && avg > 500); 

	if (samples < NUMSAMPLES/5)
	{
#if 0
		printf("System timing too unstable, assuming 8ms granularity\n");
#endif
		avg = 8000;
	}

	return avg;
}

void Sleep_Init()
{
	sleep_granularity = get_sleep_granularity();

	Com_Printf("System sleep granularity: %d us\n", sleep_granularity);
	
#ifdef __linux__
	/* Linux sucks horse cocks */
	sleep_granularity+= sleep_granularity;
#endif

	sleep_granularity+= sleep_granularity*2/100;
}

void Sleep_Sleep(unsigned int sleeptime)
{
	unsigned int real_sleep_time;
	unsigned long long curtime;
	unsigned long long endtime;

	endtime = Sys_IntTime();

	endtime += sleeptime;

	if (sleeptime >= sleep_granularity)
	{
		real_sleep_time = sleeptime - sleep_granularity;

		Sys_MicroSleep(real_sleep_time);
	}

	do
	{
		curtime = Sys_IntTime();
	} while(curtime < endtime);

#if 0
	if (curtime.tv_usec > 100)
	{
		printf("Asked to sleep %d us, Sys_MicroSlept %d us\n", sleeptime, real_sleep_time);
		printf("Overslept %d us\n", curtime.tv_usec);
	}
#endif
}

