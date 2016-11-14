#ifndef TREESUBFRAME_H
#define TREESUBFRAME_H

class TreeSubFrame;

#include <QFrame>
#include <QSharedPointer>
#include "frameanalysis.h"
#include "mainwindow.h"

class TreeSubFrame : public QFrame
{
    Q_OBJECT
public:
    explicit TreeSubFrame(MainWindow *main, QWidget *parent = 0);
    virtual void receive_frame(QSharedPointer<FrameAnalysis> frame) = 0;
};

#endif // TREESUBFRAME_H
