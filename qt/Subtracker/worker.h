#ifndef WORKER_H
#define WORKER_H

#include <QThread>
#include <QSharedPointer>

#include "context.h"
#include "framereader.h"

class Worker : public QThread
{
    Q_OBJECT

public:
    Worker();

signals:
    void frame_produced(QSharedPointer< FrameAnalysis > frame);

private:
    void run();

    JPEGReader jpeg_reader;
    Context context;
};

#endif // WORKER_H
