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
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>

#define STRERR_BUFSIZE  128

static unsigned long flag = PERF_FLAG_FD_CLOEXEC;

static inline int
sys_perf_event_open(struct perf_event_attr *attr,
                      pid_t pid, int cpu, int group_fd,
                      unsigned long flags)
{
        int fd;

        fd = syscall(__NR_perf_event_open, attr, pid, cpu,
                     group_fd, flags);

#ifdef HAVE_ATTR_TEST
        if (unlikely(test_attr__enabled))
                test_attr__open(attr, pid, cpu, fd, group_fd, flags);
#endif
        return fd;
}


static int perf_flag_probe(void)
{
	/* use 'safest' configuration as used in perf_evsel__fallback() */
	struct perf_event_attr attr = {
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_CPU_CLOCK,
		.exclude_kernel = 1,
	};
	int fd;
	int err;
	int cpu;
	pid_t pid = -1;
	char sbuf[STRERR_BUFSIZE];

	cpu = sched_getcpu();
	if (cpu < 0)
		cpu = 0;

	/*
	 * Using -1 for the pid is a workaround to avoid gratuitous jump label
	 * changes.
	 */
	while (1) {
		/* check cloexec flag */
		fd = sys_perf_event_open(&attr, pid, cpu, -1,
					 PERF_FLAG_FD_CLOEXEC);
		if (fd < 0 && pid == -1 && errno == EACCES) {
			pid = 0;
			continue;
		}
		break;
	}
	err = errno;

	if (fd >= 0) {
		close(fd);
		return 1;
	}

	if (err != EINVAL && err != EBUSY) {
		printf("perf_event_open(..., PERF_FLAG_FD_CLOEXEC) failed with unexpected error %d (%s)\n",
		  err, strerror(err));
	}
		  

	/* not supported, confirm error related to PERF_FLAG_FD_CLOEXEC */
	while (1) {
		fd = sys_perf_event_open(&attr, pid, cpu, -1, 0);
		if (fd < 0 && pid == -1 && errno == EACCES) {
			pid = 0;
			continue;
		}
		break;
	}
	err = errno;

	if (fd >= 0)
		close(fd);

	if (fd < 0 && err != EBUSY) {
		printf("perf_event_open(..., 0) failed unexpectedly with error %d (%s)\n",
		      err, strerror(err));
		return -1;
	}

	return 0;
}

unsigned long perf_event_open_cloexec_flag(void)
{
	static bool probed;

	if (!probed) {
		if (perf_flag_probe() <= 0)
			flag = 0;
		probed = true;
	}

	return flag;
}


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

#define MAX_USED_EVENTS_NR 4

int main(int argc, char **argv)
{
        struct perf_event_attr pe;
        long long count[MAX_USED_EVENTS_NR + 1];
	int fd, flag;
	int i,pid,loop=0;
	(void)argc;

	pid =atoi(argv[1]);
	printf("pid=%d\r\n",pid);
	
	for(i=0;i<MAX_USED_EVENTS_NR;i++) {
		memset(&pe, 0, sizeof(struct perf_event_attr));
		pe.type = 4;
		pe.size = sizeof(struct perf_event_attr);
		pe.config = sample_events_array[i].id;
		pe.disabled = 0;
		pe.read_format = PERF_FORMAT_GROUP;
		flag = perf_event_open_cloexec_flag();
		//flag = PERF_FLAG_FD_OUTPUT;
		flag = 0;

		if (i == 0) {
			fd = perf_event_open(&pe, pid, -1, -1, flag);
			if (fd < 0) {
	              		fprintf(stderr, "Error opening leader %llx\n", pe.config);
	              		exit(EXIT_FAILURE);
	           	}
		} else {
			pe.read_format = 0;

			int tmpfd = perf_event_open(&pe, pid, -1, fd, flag);
			//int tmpfd = perf_event_open(&pe, pid, -1, fd, 0);
			if (tmpfd < 0) {
	              		printf("[%d] %ld: %s\n", __LINE__, pe.config, strerror(errno));
	              		exit(EXIT_FAILURE);
	           	}

		}

	}

   //	ioctl(fd, PERF_EVENT_IOC_RESET, 0);
   //	ioctl(fd[i], PERF_EVENT_IOC_ENABLE, 0);
	int prevCount[MAX_USED_EVENTS_NR + 1] = {0}; 
	
	do {
		read(fd, count, sizeof(count));
		printf("loop=%d\n", loop++);
		printf("0: %d\n",  count[0]);
		for(i = 1; i<= MAX_USED_EVENTS_NR; i++) {
		        printf("[%20s] %lld\n", sample_events_array[i-1].name, count[i] - prevCount[i]);
		        prevCount[i] = count[i];
		}

		usleep(500000);
	}while(1);
	
	close(fd);

	return 0;	 
}

