#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>

int pcm_getcpu()
{
	int id = -1;
	asm volatile (
		"rdtscp\n\t"
		"mov %%ecx, %0\n\t":
		"=r" (id) :: "%rax", "%rcx", "%rdx");
	// processor ID is in ECX: https://www.felixcloutier.com/x86/rdtscp
	// Linux encodes the NUMA node starting at bit 12, so remove the NUMA
	// bits when returning the CPU integer by masking with 0xFFF.
	return id & 0xFFF;
}

struct MyPCM {
	int (*pcm_c_build_core_event)(uint8_t id, const char * argv);
	int (*pcm_c_init)();
	void (*pcm_c_start)();
	void (*pcm_c_stop)();
	uint64_t (*pcm_c_get_cycles)(uint32_t core_id);
	uint64_t (*pcm_c_get_instr)(uint32_t core_id);
	uint64_t (*pcm_c_get_core_event)(uint32_t core_id, uint32_t event_id);
};

struct MyPCM PCM;

#ifndef PCM_DYNAMIC_LIB
/* Library functions declaration (instead of .h file) */
int pcm_c_build_core_event(uint8_t, const char *);
int pcm_c_init();
void pcm_c_start();
void pcm_c_stop();
uint64_t pcm_c_get_cycles(uint32_t);
uint64_t pcm_c_get_instr(uint32_t);
uint64_t pcm_c_get_core_event(uint32_t, uint32_t);
#endif

void test1() {
	PCM.pcm_c_init();
  PCM.pcm_c_start();

  // No memory accessed: no LLC refs or misses
  unsigned s=0;
  for (unsigned i=0; i<10000000; i++) {
    s+=1;
  }

	PCM.pcm_c_stop();

	int lcore_id = pcm_getcpu();

	printf("test1: lcore: %d cpu-cycles: %lu instructions: %lu, instructions/cycle: %3.2f, counter0: %lu, counter1: %lu, counter2: %lu, counter3: %lu\n",
    lcore_id,
		PCM.pcm_c_get_cycles(lcore_id),
		PCM.pcm_c_get_instr(lcore_id),
		(double)PCM.pcm_c_get_instr(lcore_id)/PCM.pcm_c_get_cycles(lcore_id),
		PCM.pcm_c_get_core_event(lcore_id,0),
		PCM.pcm_c_get_core_event(lcore_id,1),
		PCM.pcm_c_get_core_event(lcore_id,2),
		PCM.pcm_c_get_core_event(lcore_id,3));
}

int main(int argc, const char *argv[])
{
  int numEvents = argc - 1;

	void * handle = dlopen("libpcm.so", RTLD_LAZY);
	if(!handle) {
		printf("Abort: could not (dynamically) load shared library \n");
		return -1;
	}

	PCM.pcm_c_build_core_event = (int (*)(uint8_t, const char *)) dlsym(handle, "pcm_c_build_core_event");
	PCM.pcm_c_init = (int (*)()) dlsym(handle, "pcm_c_init");
	PCM.pcm_c_start = (void (*)()) dlsym(handle, "pcm_c_start");
	PCM.pcm_c_stop = (void (*)()) dlsym(handle, "pcm_c_stop");
	PCM.pcm_c_get_cycles = (uint64_t (*)(uint32_t)) dlsym(handle, "pcm_c_get_cycles");
	PCM.pcm_c_get_instr = (uint64_t (*)(uint32_t)) dlsym(handle, "pcm_c_get_instr");
	PCM.pcm_c_get_core_event = (uint64_t (*)(uint32_t,uint32_t)) dlsym(handle, "pcm_c_get_core_event");

	if(PCM.pcm_c_init == NULL || PCM.pcm_c_start == NULL || PCM.pcm_c_stop == NULL ||
			PCM.pcm_c_get_cycles == NULL || PCM.pcm_c_get_instr == NULL ||
			PCM.pcm_c_build_core_event == NULL || PCM.pcm_c_get_core_event == NULL)
		return -1;

    if (numEvents > 4)
    {
        printf("Number of arguments are too many! exit...\n");
        return -2;
    }

    for (int i = 0; i < numEvents; ++i)
    {
        PCM.pcm_c_build_core_event(i, argv[i+1]);
    }

  test1();

	return 0;
}
