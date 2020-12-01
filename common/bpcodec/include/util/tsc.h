/* Cross-platform TSC code ... courtesy of http://www.mcs.anl.gov/~kazutomo/rdtsc.html */

#ifndef _BP_TSC_UTIL_H
#define _BP_TSC_UTIL_H

#include <unistd.h>
#include <stdint.h>

#if defined(__x86_64__)

// No fence - we're just expecting to use this for rough timing
static __inline__ uint64_t rdtsc(void)
{
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return (((uint64_t)hi) << 32) | ((uint64_t)lo);
}

// https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/ia-32-ia-64-benchmark-code-execution-paper.pdf
static __inline__ uint64_t rdtsc_time_start(void) {
    uint32_t hi, lo;
    __asm__ __volatile__ ("CPUID\n\t"
                          "RDTSC\n\t"
                          "mov %%edx, %0\n\t"
                          "mov %%eax, %1\n\t"
    : "=r" (hi), "=r" (lo)::
    "%rax", "%rbx", "%rcx", "%rdx");
    return (((uint64_t)hi) << 32) | ((uint64_t)lo);
}

static __inline__ uint64_t rdtsc_time_end(void) {
    uint32_t hi, lo;
    asm volatile("RDTSCP\n\t"
                 "mov %%edx, %0\n\t"
                 "mov %%eax, %1\n\t"
                 "CPUID\n\t": "=r" (hi), "=r" (lo)::
    "%rax", "%rbx", "%rcx", "%rdx");
    return (((uint64_t)hi) << 32) | ((uint64_t)lo);
}
/*
#elif defined(_USE_PPC)

static __inline__ uint64_t rdtsc(void)
{
    uint64_t result=0;
    uint32_t upper, lower,tmp;
    __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
    result = upper;
    result = result<<32;
    result = result|lower;

    return(result);
}
*/
#else

#include <sys/time.h>

// FIXME: oh, this is so terrible ...
static __inline__ uint64_t rdtsc(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)((tv.tv_sec * (double)1000000.0) + (double)tv.tv_usec);
}

#endif

static __inline__ uint64_t tsc_freq(uint64_t wait)
{
    uint64_t start = rdtsc();
    usleep(wait);
    return (rdtsc() - start) / 3;
}

#endif
