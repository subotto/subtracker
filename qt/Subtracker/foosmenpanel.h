#ifndef FOOSMENPANEL_H
#define FOOSMENPANEL_H

#include <QFrame>

#include "treesubframe.h"

namespace Ui {
class FoosmenPanel;
}

class FoosmenPanel : public TreeSubFrame
{
    Q_OBJECT

public:
    explicit FoosmenPanel(MainWindow *main, QWidget *parent = 0);
    ~FoosmenPanel();
    virtual void receive_frame(QSharedPointer<FrameAnalysis> frame);

private:
    Ui::FoosmenPanel *ui;
};

#endif // FOOSMENPANEL_H
