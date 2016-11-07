#ifndef WORKER_H
#define WORKER_H

#include <QThread>

class Worker : public QThread
{
public:
    Worker();

private:
    void run();
};

#endif // WORKER_H
