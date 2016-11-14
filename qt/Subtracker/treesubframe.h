#ifndef TREESUBFRAME_H
#define TREESUBFRAME_H

class TreeSubFrame;

#include <QFrame>
#include <QSharedPointer>
#include "frameanalysis.h"
#include "framesettings.h"
#include "mainwindow.h"

class TreeSubFrame : public QFrame
{
    Q_OBJECT
public:
    explicit TreeSubFrame(MainWindow *main, QWidget *parent = 0);
    virtual void receive_frame(QSharedPointer<FrameAnalysis> frame) = 0;
    virtual void receive_settings(const FrameSettings &settings) = 0;

protected:
    MainWindow *main;
};

#endif // TREESUBFRAME_H
