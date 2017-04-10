Measure OS Thread to Thread synchronization latency.

How-It-Works:

The main thread spawns a thread B that will wait on condition variable
and then sleep 10s
Then, main thread stores rdtsc timestamp to beforeTicks and 
signals the condition variable.
Later on, B wakes and immediately reads beforeTicks and gets
final timestamp using rdtsc.
Next, B exits the critical section and computes some stats on 
the timestamp difference.

I have implemented buzy-waiting in main thread to improve test accuracy.

How-To-Build:

Just run make in the project directory:
$ make

How-To-Use-It:

First argument is the main thread affinity passed directly into
pthread_setaffinity_np. Second argument is the second thread's affinity.
$ ./testrw 1 3
