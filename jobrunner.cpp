#include "jobrunner.hpp"
#include <cstdio>
#include <chrono>

JobRunner::JobRunner(unsigned n_threads) {
    running = false;
    if(n_threads == 0) {
        nthreads = thread::hardware_concurrency();
    } else {
        nthreads = n_threads;
    }
    if(nthreads == 0) {
        nthreads = 1;
    }
    for(unsigned i=0; i<nthreads; i++) {
        current_job_type[i] = -1;
    }
}

void JobRunner::set_job_creator(JobCreator* job_creator) {
    jobcreator = job_creator;
}

void JobRunner::add_job_handler(unsigned t, JobHandler* job_handler) {
    if(jobhandlers.size() < t+1) {
        jobhandlers.resize(t+1);
    }
    jobhandlers[t] = job_handler;
}

void JobRunner::add_job(unsigned t, void* data) {
    unique_lock<mutex> lock(available_jobs_mutex);
    available_jobs_type.push_back(t);
    available_jobs_data.push_back(data);
    available_jobs_not_empty.notify_one();
}

void JobRunner::job_creator() {
    jobcreator(this);
}

void JobRunner::job_dispatcher() {
    while(true) {
        if(!running) break;
        unique_lock<mutex> lock(available_jobs_mutex);
        if(available_jobs_type.empty()) {
            available_jobs_not_empty.wait(lock);
            continue;
        }
        for(unsigned i=0; i<nthreads; i++) {
            unique_lock<mutex> lock_current(current_job_mutex[i]);
            if(current_job_type[i] == -1) {
                current_job_type[i] = available_jobs_type.front();
                current_job_data[i] = available_jobs_data.front();
                available_jobs_type.pop_front();
                available_jobs_data.pop_front();
                lock_current.unlock();
                current_job_new[i].notify_all();
                break;
            }
        }
    }
}

void JobRunner::job_handler(unsigned id) {
    while(true) {
        if(!running) break;
        unique_lock<mutex> lock(current_job_mutex[id]);
        if(current_job_type[id] == -1) {
            current_job_new[id].wait(lock);
            continue;
        }
        jobhandlers[current_job_type[id]](this, current_job_data[id]);
        current_job_type[id] = -1;
    }
}

void JobRunner::wait_all() {
    threads[0].join();
    while(true) {
        if(!available_jobs_type.empty()) {
            this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            break;
        }
    }
    stop();
}

void JobRunner::start() {
    running = true;
    threads.push_back(thread(&JobRunner::job_creator, this));
    threads.push_back(thread(&JobRunner::job_dispatcher, this));
    for(int i=0; i<nthreads; i++) {
        threads.push_back(thread(&JobRunner::job_handler, this, i));
    }
}

void JobRunner::stop() {
    running = false;
    available_jobs_not_empty.notify_one();
    for(unsigned i=0; i<nthreads; i++) {
        current_job_new[i].notify_one();
    }
    for(thread& t: threads) {
        if(t.joinable()) {
            t.join();
        }
    }
}

JobRunner::~JobRunner() {
    if(running) {
        stop();
    }
}
