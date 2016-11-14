#include "categorybutton.h"

CategoryButton::CategoryButton(QWidget *parent) :
    QPushButton(parent),
    target(NULL)
{
    connect(this, SIGNAL(pressed()), this, SLOT(ButtonPressed()));
    this->update_icon();
}

CategoryButton::CategoryButton(const QString &text, QWidget *parent, QTreeWidgetItem *target) :
    QPushButton(text, parent),
    target(target) {
    connect(this, SIGNAL(pressed()), this, SLOT(ButtonPressed()));
    this->update_icon();
}

void CategoryButton::set_target(QTreeWidgetItem *target)
{
    this->target = target;
    this->update_icon();
}

void CategoryButton::update_icon() {
    if (this->target != NULL) {
        bool expanded = this->target->isExpanded();
        QStyle *style = this->style();
        QIcon icon = style->standardIcon(expanded ? QStyle::SP_ArrowDown : QStyle::SP_ArrowForward);
        this->setIcon(icon);
    }
}

void CategoryButton::ButtonPressed()
{
    if (this->target != NULL) {
        target->setExpanded( !target->isExpanded() );
        this->update_icon();
    }
}
