#include "rebelliousnodeconfigdialog.h"
#include "ui_rebelliousnodeconfigdialog.h"

#include <QModelIndex>

RebelliousNodeConfigDialog::RebelliousNodeConfigDialog(QWidget *parent, QString nodeAddress, QString privkey) :
    QDialog(parent),
    ui(new Ui::RebelliousNodeConfigDialog)
{
    ui->setupUi(this);
    QString desc = "rpcallowip=127.0.0.1<br>rpcuser=REPLACEME<br>rpcpassword=REPLACEME<br>staking=0<br>server=1<br>listen=1<br>port=REPLACEMEWITHYOURPORT<br>masternode=1<br>masternodeaddr=" + nodeAddress + "<br>masternodeprivkey=" + privkey + "<br>";
    ui->detailText->setHtml(desc);
}

RebelliousNodeConfigDialog::~RebelliousNodeConfigDialog()
{
    delete ui;
}
