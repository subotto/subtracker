#include "jobrunner.hpp"
#include <cstdio>

void jobcreator(JobRunner* jr) {
    for(int i=0; i<20; i++) {
        int* x = new int;
        *x = i;
        jr->add_job(i%2, x);
    }
}

void jobhandler0(JobRunner* jr, void* data) {
    printf("%d\n", *(int*)data);
}

void jobhandler1(JobRunner* jr, void* data) {
    printf("-%d\n", *(int*)data);
}

int main() {
    JobRunner J;
    J.set_job_creator(&jobcreator);
    J.add_job_handler(0, &jobhandler0);
    J.add_job_handler(1, &jobhandler1);
    J.start();
    J.wait_all();
}
