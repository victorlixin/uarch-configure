/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

struct st_event{
	unsigned int id;
	char *name;
};

#define MAX_EVENTS_NR 5

struct st_event sample_events_array[MAX_EVENTS_NR] = 
{
	{0x3c,"UNHALTED_CYCLE"},
	{0x19c,"IDQ_UOPS_NOT_DILIVER"},
	{0x2c2,"RETIRED"},
	{0x30d,"RECOVERY_CYCLE"},
	{0x10e,"UOPS_ISSUED"}
};

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                       int cpu, int group_fd, unsigned long flags)
{
           int ret;

           ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                          group_fd, flags);
           return ret;
}

#define MAX_USED_EVENTS_NR 5

int main(int argc, char **argv)
{
        struct perf_event_attr pe;
        long long count;
	int fd[MAX_USED_EVENTS_NR];
	int i,pid,loop=0;
	(void)argc;

	pid =atoi(argv[1]);
	printf("pid=%d\r\n",pid);
	
	for(i=0;i<MAX_USED_EVENTS_NR;i++) {
		if(sample_events_array[i].id == 0) {
			break;
		}
		memset(&pe,0,sizeof(struct perf_event_attr));
		pe.type = PERF_TYPE_RAW;
		pe.size = sizeof(struct perf_event_attr);
		pe.config = sample_events_array[i].id;
		pe.disabled = 1;

		fd[i] = perf_event_open(&pe, pid, -1, -1, 0);
		if (fd[i] == -1) {
              		fprintf(stderr, "Error opening leader %llx\n", pe.config);
              		exit(EXIT_FAILURE);
           	}

           	ioctl(fd[i], PERF_EVENT_IOC_RESET, 0);
           	ioctl(fd[i], PERF_EVENT_IOC_ENABLE, 0);
	}	
	
	int prevCount[MAX_USED_EVENTS_NR] = {0};
	do {
		printf("loop = %d\n", loop++);
		for(i=0;i<MAX_USED_EVENTS_NR;i++) {
			//ioctl(fd[i], PERF_EVENT_IOC_DISABLE, 0);
			read(fd[i], &count, sizeof(long long));
		        printf("[%20s] %lld\n", sample_events_array[i].name, count-prevCount[i]);
		        prevCount[i] = count;
		}
		usleep(500000);
	}while(1);
	
	for(i=0;i<MAX_USED_EVENTS_NR;i++) {
		close(fd[i]);
	}
	 
}

