#include "treesubframe.h"

TreeSubFrame::TreeSubFrame(MainWindow *main, QWidget *parent) :
    QFrame(parent), main(main)
{
    main->register_sub_frame(this);
}
