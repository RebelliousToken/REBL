#ifndef ADDEDITREBLNODE_H
#define ADDEDITREBLNODE_H

#include <QDialog>

namespace Ui {
class AddEditRebelliousNode;
}


class AddEditRebelliousNode : public QDialog
{
    Q_OBJECT

public:
    explicit AddEditRebelliousNode(QWidget *parent = 0);
    ~AddEditRebelliousNode();

protected:

private slots:
    void on_okButton_clicked();
    void on_cancelButton_clicked();

signals:

private:
    Ui::AddEditRebelliousNode *ui;
};

#endif // ADDEDITREBLNODE_H
