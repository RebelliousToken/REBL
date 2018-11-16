#ifndef REBLNODECONFIGDIALOG_H
#define REBLNODECONFIGDIALOG_H

#include <QDialog>

namespace Ui {
    class RebelliousNodeConfigDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class RebelliousNodeConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RebelliousNodeConfigDialog(QWidget *parent = 0, QString nodeAddress = "123.456.789.123:28666", QString privkey="MASTERNODEPRIVKEY");
    ~RebelliousNodeConfigDialog();

private:
    Ui::RebelliousNodeConfigDialog *ui;
};

#endif // REBLNODECONFIGDIALOG_H
