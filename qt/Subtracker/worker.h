#ifndef WORKER_H
#define WORKER_H

#include <QThread>
#include <QSharedPointer>
#include <atomic>

#include "context.h"
#include "framereader.h"

class Worker : public QThread
{
    Q_OBJECT

public:
    Worker(const FrameSettings &settings);
    void stop();
    void set_settings(const FrameSettings &settings);

signals:
    void frame_produced(QSharedPointer< FrameAnalysis > frame);

private:
    void run();

    JPEGReader jpeg_reader;
    Context context;

    std::atomic< bool > running;
};

#endif // WORKER_H
