#ifndef CATEGORYBUTTON_H
#define CATEGORYBUTTON_H

#include <QPushButton>
#include <QTreeWidget>

class CategoryButton : public QPushButton
{
    Q_OBJECT
public:
    CategoryButton(QWidget *parent);
    void set_target(QTreeWidgetItem *target);

    CategoryButton(const QString &text, QWidget *parent, QTreeWidgetItem *target);
private slots:
    void ButtonPressed();

private:
    QTreeWidgetItem* target;
    void update_icon();
};

#endif // CATEGORYBUTTON_H
