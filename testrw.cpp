/*
 * Copyright (C) 2016 Jarmo Antero Tiitto. (email: jarmo.tiitto@gmail.com)
 *
 * This file is part of testschedlat.
 *
 * testschedlat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * testschedlat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with testschedlat; if not, see <http://www.gnu.org/licenses/>.
 *
 **/
#include <iostream>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <sys/time.h>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <sstream>

double CpuFrequency=4334.0; // CPU frequency in MHz
 
pthread_cond_t cv;
pthread_mutex_t m;
pthread_t thread2;
 
struct timeval before, after;
 
typedef unsigned long long ticks;
std::atomic<ticks> beforeTicks, afterTicks;

std::atomic<bool> spin_var(false);
std::atomic<int> bootloop(0);

static __inline__ ticks getrdtsc()
{
     unsigned a, d;
     asm volatile("rdtsc" : "=a" (a), "=d" (d));
 
     return (((ticks)a) | (((ticks)d) << 32));
}
 
void *beginthread2(void *v)
{
   int loops = 0;
   ticks ticksum = 0;
   ticks maxticks = 0;
   ticks start, stop;
   std::vector<ticks> median;
   for (;;)
   {
      // Wait for a signal from thread 1
      pthread_mutex_lock(&m);
      pthread_cond_wait(&cv, &m);
      // notice thread 1 atomically.
      //spin_var = true;
      // Get starting ticks (written in main())
      start = beforeTicks;
      // Get the ending ticks
      stop=getrdtsc();
      afterTicks=stop;
      pthread_mutex_unlock(&m);
      
      // update worst seen ticks
      maxticks = std::max((stop-start), maxticks);
      // record stats.
      ticksum += stop-start;
      median.push_back(stop-start);
      ++loops;
      
      if(loops > 100 ) {
        // get median ticks
        std::sort(median.begin(), median.end(), std::less<ticks>());
        ticks medticks = median[median.size()/2];
        median.clear();
        
        // Display the time elapsed
        std::cout << "Ticks elapsed: " << (ticksum / loops) << " avg, median "
          << medticks/CpuFrequency << " us, worst: "  
          << maxticks/CpuFrequency << " us" <<std::endl;
        loops = 0;
        ticksum = 0;
        maxticks = 0;
      }
      
   }
 
   return NULL;
}


void* buzyrunner(void *v) {
  // init
  while(!spin_var) {
    pthread_mutex_lock(&m);
    pthread_cond_wait(&cv, &m);
    ++bootloop;
    pthread_mutex_unlock(&m);
  }
  // spin.
  while(spin_var) {
    ++bootloop;
  }
  return NULL;
}
template<typename T>
T read_value(std::string str) {
  std::stringstream ss;
  T val;
  ss<<str;
  ss>>val;
  return val;
}
 
int main(int argc, char *argv[])
{
  int core1=0, core2=0;

  if (argc < 3)
  {
    std::cout << "Usage: " << argv[0] << " producer_corenum consumer_corenum" << std::endl;
    return 1;
  }
  
  pthread_mutex_init(&m, NULL);
  pthread_cond_init(&cv, NULL);
  /*
   * Gain bogus measurement of cpu speed 
   */
  // start buzyrunner thread to drive any CPU core to max MHz
  // before reading /proc/cpuinfo.
  // I'm just too lazy to think better way to get this number.
  std::cout << "Warming cpu info.." << std::endl;
  bootloop = 0;
  spin_var = false;
  pthread_create(&thread2, NULL, buzyrunner, NULL);
  // Handshake with the thread.
  while(bootloop < 10) {
    // Signal thread 2
    pthread_mutex_lock(&m);
    spin_var = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m);
  }
  
  std::cout << "Getting cpu info.." << std::endl;
  // Get BogoMips measurement to calibrate our wait loop,
  // number of cores and max MHz from /proc/cpuinfo.
  double bogomips = 0.0;
  double maxmhz = 0.0;
  int maxcpus = 0;
  std::ifstream cpuinfo("/proc/cpuinfo");
  while(cpuinfo) {
    std::string info;
    std::getline(cpuinfo, info);
    if(info.find("processor	: ") == 0) {
      maxcpus = std::max<int>(read_value<int>(info.substr(std::strlen("processor	: "))), maxcpus);
    
    } else if(info.find("cpu MHz		: ") == 0) {
      maxmhz = std::max<double>(read_value<double>(info.substr(std::strlen("cpu MHz		: "))), maxmhz);
     
    } else if(info.find("bogomips	: ") == 0) {
      bogomips += read_value<double>(info.substr(std::strlen("bogomips	: ")));
    }
  }
  // stop the busyspin thread.
  spin_var = false;
  maxcpus++;
  CpuFrequency = maxmhz;
  std::cout << "CPU Max MHz:" << maxmhz << std::endl;
  std::cout << "CPU Max cores:" << maxcpus << std::endl;
  std::cout << "CPU BogoMIPS per core:" << bogomips / maxcpus << std::endl;
  
  // Get core numbers on which to perform the test
  core1 = atoi(argv[1]);
  core2 = atoi(argv[2]);

  std::cout << "Core 1: " << core1 << std::endl;
  std::cout << "Core 2: " << core2 << std::endl;
  
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core1, &cpuset);

  // Set affinity of the first (current) thread to core1
  pthread_t self=pthread_self();
  if (pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)!=0)
  {
    perror("pthread_setaffinity_np");
    return 1;
  }

  CPU_ZERO(&cpuset);
  CPU_SET(core2, &cpuset);

  // Create second thread
  pthread_create(&thread2, NULL, beginthread2, NULL);
  // Set affinity of the second thread to core2
  if (pthread_setaffinity_np(thread2, sizeof(cpu_set_t), &cpuset)!=0)
  {
    perror("pthread_setaffinity_np");
    return 1;
  }

  // Run the test
  for (;;)
  {
    // Sleep for ~10 milliseconds. This raises the latency vastly
    // and causes the whole process to be shelfed in the scheduler.
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // second method of waiting is busy spinning to see if
    // scheduler works differently for CPU running at 100%.
    ticks spin = ticks(bogomips / maxcpus) * 1000;  // ~10s
    while(spin > 0) {
      asm volatile("nop");
      --spin;
    }
    
    // Get the starting ticks
    beforeTicks=getrdtsc();
    // Signal thread 2
    pthread_mutex_lock(&m);
    //spin_var = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m);
    
    // spin until thead 2 notices us.
    /*while(spin_var.exchange(false) == false) {
      asm volatile("rep; nop");
    }*/
  }
}