#ifndef _JOBRUNNER_HPP_
#define _JOBRUNNER_HPP_

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <list>
#include <vector>
using namespace std;

class JobRunner;

typedef void (JobCreator)(JobRunner*);
typedef void (JobHandler)(JobRunner*, void*);

const int maxthreads = 100;

class JobRunner {
private:
    int nthreads;
    atomic<bool> running;
    JobCreator* jobcreator;
    vector<JobHandler*> jobhandlers;
    vector<thread> threads;

    atomic<int> current_job_type[maxthreads];
    atomic<void*> current_job_data[maxthreads];
    mutex current_job_mutex[maxthreads];
    condition_variable current_job_new[maxthreads];

    list<unsigned> available_jobs_type;
    list<void*> available_jobs_data;
    mutex available_jobs_mutex;
    condition_variable available_jobs_not_empty;

    void job_dispatcher();
    void job_handler(unsigned id);
    void job_creator();
public:
    JobRunner(unsigned n_threads = 0);
    void add_job(unsigned t, void* data);
    void set_job_creator(JobCreator* job_creator);
    void add_job_handler(unsigned t, JobHandler* job_handler);
    void start();
    void stop();
    void wait_all();
    ~JobRunner();
};

#endif
