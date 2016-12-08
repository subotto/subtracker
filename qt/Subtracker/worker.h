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
    std::pair<std::unique_lock<std::mutex>, FrameCommands *> edit_commands();
    QSharedPointer<FrameAnalysis> get_last_frame();
    int get_queue_length();

signals:
    void frame_produced();

private:
    void run();

    JPEGReader jpeg_reader;
    Context context;
    // QT guarantees that QSharedPointer are atomic
    QSharedPointer< FrameAnalysis > last_frame;

    std::atomic< bool > running;
};

#endif // WORKER_H
