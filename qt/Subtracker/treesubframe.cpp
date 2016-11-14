#include "treesubframe.h"

TreeSubFrame::TreeSubFrame(MainWindow *main, QWidget *parent) :
    QFrame(parent)
{
    main->register_sub_frame(this);
}
