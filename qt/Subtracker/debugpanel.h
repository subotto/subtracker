#ifndef DEBUGPANEL_H
#define DEBUGPANEL_H

#include <QFrame>

#include "treesubframe.h"

namespace Ui {
class DebugPanel;
}

class DebugPanel : public TreeSubFrame
{
    Q_OBJECT

public:
    explicit DebugPanel(MainWindow *main, QWidget *parent = 0);
    ~DebugPanel();
    void receive_frame(QSharedPointer<FrameAnalysis> frame);
    void receive_settings(const FrameSettings &settings);

private:
    Ui::DebugPanel *ui;

    std::vector< QSharedPointer< VideoWidget > > videos;
};

#endif // DEBUGPANEL_H
