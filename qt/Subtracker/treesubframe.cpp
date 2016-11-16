#include "treesubframe.h"

TreeSubFrame::TreeSubFrame(MainWindow *main, QWidget *parent) :
    QFrame(parent), main(main)
{
    this->main->register_sub_frame(this);
}

void TreeSubFrame::set_main_on_children() {
    for (auto &child : this->findChildren< VideoWidget* >()) {
        child->set_main(this->main);
    }
}
