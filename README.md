// Measure OS Thread to Thread synchronization latency.
// The main thread spawns a thread B that will wait on condition variable
// and then sleep 10s
// Then, main thread stores rdtsc timestamp to beforeTicks and 
// signals the condition variable.
// Later on, B wakes and immediately reads beforeTicks and gets
// final timestamp using rdtsc.
// Next, B exits the critical section and does computes some stats.
// 
// 2017.4.10: 
// Unmodified CFS results in very jittery wake up latencies.
// the median bounces every where and CFS may even
// ignore the thread-affinity!
// -zen kernel improves on the jitter but still may
// wake threads on "wrong" core.
// -wastedcores is very good: thread-affinity is respected and
// has smallest wake-up jitter.