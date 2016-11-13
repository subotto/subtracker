#include "categorybutton.h"

CategoryButton::CategoryButton(QWidget *parent) :
    QPushButton(parent),
    target(NULL)
{
    connect(this, SIGNAL(pressed()), this, SLOT(ButtonPressed()));
}

void CategoryButton::set_target(QTreeWidgetItem *target)
{
    this->target = target;
}

void CategoryButton::ButtonPressed()
{
    if (this->target != NULL) {
        target->setExpanded( !target->isExpanded() );
    }
}
