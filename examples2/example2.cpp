#include <pgm_index.hpp>
#include <pgm_index_variants.hpp>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>                                                                                                  
#include <sched.h>                                                                                                      
#include <errno.h>                                                                                                      
#include <stdio.h>                                                                                                      
#include <fcntl.h>                                                                                                      
#include <unistd.h>                                                                                                     
#include <string.h>                                                                                                     
#include <stdlib.h>                                                                                                     
#include <inttypes.h>

#include <dlfcn.h>                                                                                                      

#include <random>

int calls;
int loop1;
int loop2;

int runs=10;
int verbose=0;
int epsilon(64);
int valueSize(16);
int keys(1000000);
float probability(0.5);
int randomValueSizeMax(0);
int binominalTrials(1<<20);
int uniformDistBound(keys*10);
std::string indexType("PGMIndex");
std::string keyGenerator("uniform");
const std::size_t Epsilon = 64;

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

void usageAndExit() {
  printf("usage: pcm_benchmark [...options...]\n");
  printf("\n");
  printf("   -e <integer>\n");
  printf("      PGM time/space trade-off error factor. Default %d\n", epsilon);
  printf("\n");
  printf("   -T <PGMIndex|BucketingPGMIndex|EliasFanoPGMIndex|CompressedPGMIndex>\n");
  printf("      PGM Index tree index variant. Default '%s'\n", indexType.c_str());
  printf("\n");
  printf("   -g <uniform|binomial|negative_binomial|geometric>\n");
  printf("      Random number generator each from STL library. Default '%s'\n", keyGenerator.c_str());
  printf("      Use -r, -t, -p to modify distribution of numbers.\n");
  printf("\n");
  printf("   -n <integer>\n");
  printf("      Number of keys to generate. Default %d\n", keys);
  printf("\n");
  printf("   -R <integer>\n");
  printf("      Number of times to run each test. Default %d\n", runs);
  printf("\n");
  printf("   -v <integer>\n");
  printf("      Value size. All values will of the same size. Default %d\n", valueSize);
  printf("      Specifiy -v or -V; last usage on command line wins\n");
  printf("\n");
  printf("   -V <integer>\n");
  printf("      Maximum value size. All values will be randomly sized in 1..N. Default disabled\n");
  printf("      Specifiy -v or -V; last usage on command line wins\n");
  printf("\n");
  printf("   -r <integer>\n");
  printf("      The upper bound for uniform generator. Default range [0, %d)\n", uniformDistBound);
  printf("\n");
  printf("   -t <integer>\n");
  printf("      The number of trials to run for binomial generators. Use with -p. Default %d\n", binominalTrials);
  printf("\n");
  printf("   -p <double>\n");
  printf("      The probability fitting parameter for binominal or geometric generator. Default %lf\n", probability);
  printf("\n");
  printf("   -l\n");
  printf("      add logging to stdout. repeat for greater verbosity\n");
  exit(2);
}

void parseCommandLine(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "lg:n:v:V:r:t:p:e:T:R:"))!=-1) {
    switch(opt) {
      case 'l':
        ++verbose;
        break;
      
      case 'g':
        if (strcmp(optarg, "uniform")==0 ||
            strcmp(optarg, "binomial")==0 ||
            strcmp(optarg, "negative_binomial")==0 ||
            strcmp(optarg, "geometric")==0) {
          keyGenerator = optarg;
        } else {
          usageAndExit();
        }
        break;

      case 'n':
        keys = atoi(optarg);
        if (keys <= 0) {
          usageAndExit();
        }
        break;

      case 'v':
        valueSize = atoi(optarg);
        if (valueSize <= 0) {
          usageAndExit();
        }
        randomValueSizeMax = 0;
        break;

      case 'V':
        randomValueSizeMax = atoi(optarg);
        if (randomValueSizeMax <= 0) {
          usageAndExit();
        }
        valueSize = 0;
        break;

      case 'r':
        uniformDistBound = atoi(optarg);
        if (uniformDistBound <= 0) {
          usageAndExit();
        }
        break;

      case 't':
        binominalTrials = atoi(optarg);
        if (binominalTrials <= 0) {
          usageAndExit();
        }
        break;

      case 'p':
        probability = atof(optarg);
        if (probability<=0 || probability>=1) {
          usageAndExit();
        }
        break;

      case 'R':
        runs = atoi(optarg);
        if (runs<=0) {
          usageAndExit();
        }
        break;

      case 'e':
        epsilon = atoi(optarg);
        if (epsilon<=0) {
          usageAndExit();
        }
        {  
          std::size_t *tmp = const_cast<std::size_t*>(&Epsilon);
          *tmp = static_cast<std::size_t>(epsilon);
        }
        break;
  
      case 'T':
        if (strcmp(optarg, "PGMIndex")==0 ||
            strcmp(optarg, "BucketingPGMIndex")==0 ||
            strcmp(optarg, "EliasFanoPGMIndex")==0 ||
            strcmp(optarg, "CompressedPGMIndex")==0) {
          indexType = optarg;
        } else {
          usageAndExit();
        }
        break;

      default:
        usageAndExit();
    }
  }
}

int main(int argc, char **argv) {
  int cpu = sched_getcpu();                                                                                             
  cpu_set_t mask;                                                                                                       
  CPU_ZERO(&mask);                                                                                                      
  CPU_SET(cpu, &mask);                                                                                                  
                                                                                                                        
  // Pin caller's (current) thread to cpu                                                                               
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {                                                           
    fprintf(stderr, "Error: could not pin thread to core %d: %s\n", cpu, strerror(errno));                            
    return 1;                                                                                                         
  }

  std::random_device rd;
  std::mt19937 generator(rd());

  std::vector<uint64_t> data(keys);
  if (verbose>1) {
    printf("reserved space for %lu keys\n", data.size());
  }

  if (keyGenerator=="uniform") {
    if (verbose) {
      printf("generating %d random 64-bit keys from dense uniform distribuion between %d and %d\n", keys, 0, uniformDistBound);
    }
    std::uniform_int_distribution<uint64_t> ugen(0, uniformDistBound);
    for (int i=0; i<keys; ++i) {
      data[i] = ugen(generator);
    }
  } else if (keyGenerator=="binomial") {
    if (verbose) {
      printf("generating %d random 64-bit keys from binominal distribuion trials %d probability %f\n", keys, binominalTrials, probability);
    }
    std::binomial_distribution<uint64_t> bgen(binominalTrials, probability);
    for (int i=0; i<keys; ++i) {
      data[i] = bgen(generator);
    }
  } else if (keyGenerator=="negative_binomial") {
    if (verbose) {
      printf("generating %d random 64-bit keys from negative binominal distribuion trials %d probability %f\n", keys, binominalTrials, probability);
    }
    std::negative_binomial_distribution<uint64_t> nbgen(binominalTrials, probability);
    for (int i=0; i<keys; ++i) {
      data[i] = nbgen(generator);
    }
  } else {
    if (verbose) {
      printf("generating %d random 64-bit keys from geometric distribuion probability %f\n", keys, probability);
    } 
    std::geometric_distribution<uint64_t> pgen(probability);
    for (int i=0; i<keys; ++i) {
      data[i] = pgen(generator);
    }
  }

  std::sort(data.begin(), data.end());                                                                          

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

  if (indexType == "PGMIndex") {
    pgm::PGMIndex<uint64_t, Epsilon> pgm(data);

    for (int r=0; r<runs; ++r) {
      calls = loop1 = loop2 = 0;

      PCM.pcm_c_init();                                                                                                     
      PCM.pcm_c_start(); 

      // Test code
      for (volatile int i=0; i<keys; ++i) {
        pgm.search(data[i]);
      }

      PCM.pcm_c_stop();                                                                                                     

	    int lcore_id = pcm_getcpu();

      printf("test1: cpu-cycles: %lu instructions: %lu, instructions/cycle: %3.2f, counter0: %lu, counter1: %lu, counter2: %lu, counter3: %lu\n",
        PCM.pcm_c_get_cycles(lcore_id),
        PCM.pcm_c_get_instr(lcore_id),
        (double)PCM.pcm_c_get_instr(lcore_id)/(double)PCM.pcm_c_get_cycles(lcore_id),
        PCM.pcm_c_get_core_event(lcore_id,0),
        PCM.pcm_c_get_core_event(lcore_id,1),
        PCM.pcm_c_get_core_event(lcore_id,2),
        PCM.pcm_c_get_core_event(lcore_id,3));


      printf("calls %d loops1 %d loops2 %d\n", calls, loop1, loop2);
    }
  }

  return 0;
}
