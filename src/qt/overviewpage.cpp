// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The REBL developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "init.h"
#include "darksend.h"
#include "optionsmodel.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QMargins>
#include <QDesktopServices>
#include <QUrl>

#define DECORATION_SIZE 25
#define ICON_OFFSET 13
#define NUM_ITEMS 5
#define ROW_SPACING 30
#define CORRECTION 20
#define BORDER_RADIUS 5

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate() : QAbstractItemDelegate(), unit(BitcoinUnits::REBL)
    {
    }

    inline void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        painter->save();

        QFont font = painter->font();

        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(option.rect.x()-ICON_OFFSET, option.rect.y()-CORRECTION, option.rect.width()+40, option.rect.height()+(CORRECTION * 2.5), BORDER_RADIUS, BORDER_RADIUS);
        QPen pen(QColor(255, 255, 255), 0);
        painter->setPen(pen);
        painter->fillPath(path, QColor(255, 255, 255));
        painter->drawPath(path);

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        mainRect.moveLeft(ICON_OFFSET);
        QRect decorationRect(mainRect.left()+ICON_OFFSET, mainRect.top() + 4, DECORATION_SIZE, DECORATION_SIZE);
        int xspace = DECORATION_SIZE + ICON_OFFSET + 10;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2 * ypad) / 2;

        QRect dateRect(mainRect.left() + xspace, mainRect.top() + ypad + 20, mainRect.width() - xspace - ICON_OFFSET, mainRect.height());
        QRect amountRect(mainRect.left() + xspace, mainRect.top() + ypad, mainRect.width() - xspace - ICON_OFFSET, mainRect.height());
        QRect labelRect(mainRect.left() + xspace, mainRect.top() + ypad - 20, mainRect.width() - xspace, mainRect.height());
        QRect addressRect(mainRect.left() + xspace, mainRect.top() + ypad, mainRect.width() - xspace, mainRect.height());
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString label = index.data(TransactionTableModel::LabelRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = COLOR_BLACK;
        if (value.canConvert<QBrush>()) {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;

        font.setPixelSize(16);
        painter->setPen(COLOR_BLACK);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, (label.isEmpty() ? tr("Unknown") : label), &boundingRect);

        QString address = "(" + index.data(TransactionTableModel::AddressRole).toString() + ")";

        font.setPixelSize(14);
        painter->setPen(COLOR_BLUE);
        painter->setFont(font);
        painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool()) {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top() + ypad + halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if (amount < 0) {
            foreground = COLOR_NEGATIVE;
        } else if (!confirmed) {
            foreground = COLOR_UNCONFIRMED;
        } else {
            foreground = COLOR_BLUE;
        }
        painter->setPen(foreground);

        font.setPixelSize(16);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        QString amountText = BitcoinUnits::simpleFormat(BitcoinUnits::REBL, amount, true, BitcoinUnits::separatorAlways, 2);

        if (!confirmed) {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText);

        painter->setPen(COLOR_BLACK);
        painter->drawText(dateRect, Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget* parent) : QWidget(parent),
                                              ui(new Ui::OverviewPage),
                                              clientModel(0),
                                              walletModel(0),
                                              currentBalance(-1),
                                              currentUnconfirmedBalance(-1),
                                              currentImmatureBalance(-1),
                                              currentWatchOnlyBalance(-1),
                                              currentWatchUnconfBalance(-1),
                                              currentWatchImmatureBalance(-1),
                                              txdelegate(new TxViewDelegate()),
                                              filter(0)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS+ROW_SPACING * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listTransactions->setSpacing(ROW_SPACING);
//    ui->listTransactions->setMinimumWidth(1150);
    ui->listTransactions->setUniformItemSizes(true);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    const int shadowBlurRadius = 10;
    const int shadowOffsetX = 0;
    const int shadowOffsetY = 3;
    QColor shadowColor = QColor(0, 0, 0, 50);

    QGraphicsDropShadowEffect *transactionShadow = new QGraphicsDropShadowEffect();
    transactionShadow->setBlurRadius(shadowBlurRadius);
    transactionShadow->setColor(shadowColor);
    transactionShadow->setOffset(shadowOffsetX, shadowOffsetY);
    ui->listTransactions->setGraphicsEffect(transactionShadow);

    QGraphicsDropShadowEffect *balancesShadow = new QGraphicsDropShadowEffect();
    balancesShadow->setBlurRadius(shadowBlurRadius);
    balancesShadow->setColor(shadowColor);
    balancesShadow->setOffset(shadowOffsetX, shadowOffsetY);
    ui->balancesWidget->setGraphicsEffect(balancesShadow);

    QGraphicsDropShadowEffect *supportBlockShadow = new QGraphicsDropShadowEffect();
    supportBlockShadow->setBlurRadius(shadowBlurRadius);
    supportBlockShadow->setColor(shadowColor);
    supportBlockShadow->setOffset(shadowOffsetX, shadowOffsetY);
    ui->supportBlock->setGraphicsEffect(supportBlockShadow);

    QPixmap totalBalanceIcon(":/icons/balance");
    totalBalanceIcon = totalBalanceIcon.scaled(QSize(25,25),  Qt::KeepAspectRatio);
    QLabel* totalBalanceIconLabel = new QLabel();
    QLabel* totalBalanceTextLabel = new QLabel(tr("Total Balance"));
    totalBalanceIconLabel->setPixmap(totalBalanceIcon);
    ui->totalBalanceTextHLayout->addWidget(totalBalanceIconLabel);
    ui->totalBalanceTextHLayout->addWidget(totalBalanceTextLabel);

    QPixmap supportIcon(":/icons/support");
    supportIcon = supportIcon.scaled(QSize(25,25),  Qt::KeepAspectRatio);
    QLabel* supportIconLabel = new QLabel();
    QLabel* supportTextLabel = new QLabel(tr("Support"));
    supportIconLabel->setPixmap(supportIcon);
    ui->supportHLayout->addWidget(supportIconLabel);
    ui->supportHLayout->addWidget(supportTextLabel);

    totalBalanceTextLabel->setStyleSheet("color:#BABABA;font-size:14px;");

    ui->labelTotal->setStyleSheet("font-size:18px;font-weight:400;color:#00A4B3!important;");

    ui->phoneButton->setCursor(Qt::PointingHandCursor);
    ui->emailButton->setCursor(Qt::PointingHandCursor);
    ui->socialButton->setCursor(Qt::PointingHandCursor);
    ui->phoneButtonText->setCursor(Qt::PointingHandCursor);
    ui->emailButtonText->setCursor(Qt::PointingHandCursor);
    ui->socialButtonText->setCursor(Qt::PointingHandCursor);


    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");

       {
        if (!fMasterNode) {
            timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(darksendStatus()));
            timer->start(1000);
        }
    }


    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex& index)
{
    if (filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{

}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& anonymizedBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentAnonymizedBalance = anonymizedBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;

    const int decimalsWidth = 2;

    ui->labelBalance->setText(BitcoinUnits::simpleFormatWithUnit(nDisplayUnit, balance - immatureBalance, false, BitcoinUnits::separatorAlways, decimalsWidth));
    ui->labelUnconfirmed->setText(BitcoinUnits::simpleFormatWithUnit(nDisplayUnit, unconfirmedBalance, false, BitcoinUnits::separatorAlways, decimalsWidth));
    ui->labelImmature->setText(BitcoinUnits::simpleFormatWithUnit(nDisplayUnit, immatureBalance, false, BitcoinUnits::separatorAlways, decimalsWidth));
    ui->labelTotal->setText(BitcoinUnits::simpleFormatWithUnit(nDisplayUnit, balance + unconfirmedBalance, false, BitcoinUnits::separatorAlways, decimalsWidth));
    updateDarksendProgress();

    static int cachedTxLocks = 0;

    if (cachedTxLocks != nCompleteTXLocks) {
        cachedTxLocks = nCompleteTXLocks;
        ui->listTransactions->update();
    }
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    if (showWatchOnly) {
        ui->labelUnconfirmed->setIndent(20);
        ui->labelImmature->setIndent(20);
        ui->labelTotal->setIndent(20);
    }
}

void OverviewPage::setClientModel(ClientModel* model)
{
    this->clientModel = model;
    if (model) {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
    if (model && model->getOptionsModel()) {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(), model->getAnonymizedBalance(),
            model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("REBL")


    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance, currentAnonymizedBalance,
                currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = nDisplayUnit;

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString& warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
}

void OverviewPage::updateDarksendProgress()
{
    if (!pwalletMain) return;

    QString strAmountAndRounds;
    QString strAnonymizeRebelliousAmount = BitcoinUnits::formatHtmlWithUnit(nDisplayUnit, nAnonymizeRebelliousAmount * COIN, false, BitcoinUnits::separatorAlways);

    if (currentBalance == 0) {


        // when balance is zero just show info from settings
        strAnonymizeRebelliousAmount = strAnonymizeRebelliousAmount.remove(strAnonymizeRebelliousAmount.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = strAnonymizeRebelliousAmount + " / " + tr("%n Rounds", "", nDarksendRounds);

        return;
    }

    CAmount nDenominatedConfirmedBalance;
    CAmount nDenominatedUnconfirmedBalance;
    CAmount nAnonymizableBalance;
    CAmount nNormalizedAnonymizedBalance;
    double nAverageAnonymizedRounds;

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return;

        nDenominatedConfirmedBalance = pwalletMain->GetDenominatedBalance(false);
        nDenominatedUnconfirmedBalance = pwalletMain->GetDenominatedBalance(true, false);
        nAnonymizableBalance = pwalletMain->GetAnonymizableBalance();
        nNormalizedAnonymizedBalance = pwalletMain->GetNormalizedAnonymizedBalance();
        nAverageAnonymizedRounds = pwalletMain->GetAverageAnonymizedRounds();
    }

    CAmount nMaxToAnonymize = nAnonymizableBalance + currentAnonymizedBalance + nDenominatedUnconfirmedBalance;

    // If it's more than the anon threshold, limit to that.
    if (nMaxToAnonymize > nAnonymizeRebelliousAmount * COIN) nMaxToAnonymize = nAnonymizeRebelliousAmount * COIN;

    if (nMaxToAnonymize == 0) return;

    if (nMaxToAnonymize >= nAnonymizeRebelliousAmount * COIN) {
        strAnonymizeRebelliousAmount = strAnonymizeRebelliousAmount.remove(strAnonymizeRebelliousAmount.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = strAnonymizeRebelliousAmount + " / " + tr("%n Rounds", "", nDarksendRounds);
    } else {
        QString strMaxToAnonymize = BitcoinUnits::formatHtmlWithUnit(nDisplayUnit, nMaxToAnonymize, false, BitcoinUnits::separatorAlways);
        strMaxToAnonymize = strMaxToAnonymize.remove(strMaxToAnonymize.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = "<span style='color:red;'>" +
                             QString(BitcoinUnits::factor(nDisplayUnit) == 1 ? "" : "~") + strMaxToAnonymize +
                             " / " + tr("%n Rounds", "", nDarksendRounds) + "</span>";
    }

    // calculate parts of the progress, each of them shouldn't be higher than 1
    // progress of denominating
    float denomPart = 0;
    // mixing progress of denominated balance
    float anonNormPart = 0;
    // completeness of full amount anonimization
    float anonFullPart = 0;

    CAmount denominatedBalance = nDenominatedConfirmedBalance + nDenominatedUnconfirmedBalance;
    denomPart = (float)denominatedBalance / nMaxToAnonymize;
    denomPart = denomPart > 1 ? 1 : denomPart;
    denomPart *= 100;

    anonNormPart = (float)nNormalizedAnonymizedBalance / nMaxToAnonymize;
    anonNormPart = anonNormPart > 1 ? 1 : anonNormPart;
    anonNormPart *= 100;

    anonFullPart = (float)currentAnonymizedBalance / nMaxToAnonymize;
    anonFullPart = anonFullPart > 1 ? 1 : anonFullPart;
    anonFullPart *= 100;

    // apply some weights to them ...
    float denomWeight = 1;
    float anonNormWeight = nDarksendRounds;
    float anonFullWeight = 2;
    float fullWeight = denomWeight + anonNormWeight + anonFullWeight;
    // ... and calculate the whole progress
    float denomPartCalc = ceilf((denomPart * denomWeight / fullWeight) * 100) / 100;
    float anonNormPartCalc = ceilf((anonNormPart * anonNormWeight / fullWeight) * 100) / 100;
    float anonFullPartCalc = ceilf((anonFullPart * anonFullWeight / fullWeight) * 100) / 100;
    float progress = denomPartCalc + anonNormPartCalc + anonFullPartCalc;
    if (progress >= 100) progress = 100;


    QString strToolPip = ("<b>" + tr("Overall progress") + ": %1%</b><br/>" +
                          tr("Denominated") + ": %2%<br/>" +
                          tr("Mixed") + ": %3%<br/>" +
                          tr("Anonymized") + ": %4%<br/>" +
                          tr("Denominated inputs have %5 of %n rounds on average", "", nDarksendRounds))
                             .arg(progress)
                             .arg(denomPart)
                             .arg(anonNormPart)
                             .arg(anonFullPart)
                             .arg(nAverageAnonymizedRounds);
}

void OverviewPage::darksendStatus()
{
#if 0
    static int64_t nLastDSProgressBlockTime = 0;

    int nBestHeight = chainActive.Height();

    // we we're processing more then 1 block per second, we'll just leave
    //if (((nBestHeight - darksendPool.cachedNumBlocks) / (GetTimeMillis() - nLastDSProgressBlockTime + 1) > 1)) return;
    nLastDSProgressBlockTime = GetTimeMillis();

    if (!fEnableDarksend) {
        if (nBestHeight != darksendPool.cachedNumBlocks) {
            darksendPool.cachedNumBlocks = nBestHeight;
            updateDarksendProgress();

            ui->darksendEnabled->setText(tr("Disabled"));
            ui->darksendStatus->setText("");
            ui->toggleDarksend->setText(tr("Start Rebellioussend"));
        }

        return;
    }

    // check darksend status and unlock if needed
    if (nBestHeight != darksendPool.cachedNumBlocks) {
        // Balance and number of transactions might have changed
        darksendPool.cachedNumBlocks = nBestHeight;
        updateDarksendProgress();

        ui->darksendEnabled->setText(tr("Enabled"));
    }

    QString strStatus = QString(darksendPool.GetStatus().c_str());

    QString s = tr("Last Darksend message:\n") + strStatus;

    if (s != ui->darksendStatus->text())
        LogPrintf("Last Darksend message: %s\n", strStatus.toStdString());

    ui->darksendStatus->setText(s);

    if (darksendPool.sessionDenom == 0) {
        ui->labelSubmittedDenom->setText(tr("N/A"));
    } else {
        std::string out;
        darksendPool.GetDenominationsToString(darksendPool.sessionDenom, out);
        QString s2(out.c_str());
        ui->labelSubmittedDenom->setText(s2);
    }
#endif
}

void OverviewPage::darksendAuto()
{
#if 0
    darksendPool.DoAutomaticDenominating();
#endif
}

void OverviewPage::darksendReset()
{
#if 0
    darksendPool.Reset();

    QMessageBox::warning(this, tr("Darksend"),
        tr("Darksend was successfully reset."),
        QMessageBox::Ok, QMessageBox::Ok);
#endif
}

void OverviewPage::toggleDarksend()
{
#if 0
    QSettings settings;
    // Popup some information on first mixing
    QString hasMixed = settings.value("hasMixed").toString();
    if (hasMixed.isEmpty()) {
        QMessageBox::information(this, tr("Darksend"),
            tr("If you don't want to see internal Darksend fees/transactions select \"Most Common\" as Type on the \"Transactions\" tab."),
            QMessageBox::Ok, QMessageBox::Ok);
        settings.setValue("hasMixed", "hasMixed");
    }
    if (!fEnableDarksend) {
        int64_t balance = currentBalance;
        float minAmount = 14.90 * COIN;
        if (balance < minAmount) {
            QString strMinAmount(BitcoinUnits::formatWithUnit(nDisplayUnit, minAmount));
            QMessageBox::warning(this, tr("Darksend"),
                tr("Darksend requires at least %1 to use.").arg(strMinAmount),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        // if wallet is locked, ask for a passphrase
        if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock(false));
            if (!ctx.isValid()) {
                //unlock was cancelled
                darksendPool.cachedNumBlocks = std::numeric_limits<int>::max();
                QMessageBox::warning(this, tr("Darksend"),
                    tr("Wallet is locked and user declined to unlock. Disabling Darksend."),
                    QMessageBox::Ok, QMessageBox::Ok);
                if (fDebug) LogPrintf("Wallet is locked and user declined to unlock. Disabling Darksend.\n");
                return;
            }
        }
    }

    fEnableDarksend = !fEnableDarksend;
    darksendPool.cachedNumBlocks = std::numeric_limits<int>::max();

    if (!fEnableDarksend) {
        ui->toggleDarksend->setText(tr("Start Rebellioussend"));
        darksendPool.UnlockCoins();
    } else {
        ui->toggleDarksend->setText(tr("Stop Rebellioussend"));

        /* show darksend configuration if client has defaults set */

        if (nAnonymizeRebelliousAmount == 0) {
            DarksendConfig dlg(this);
            dlg.setModel(walletModel);
            dlg.exec();
        }
    }
#endif
}

void OverviewPage::on_chargeButton_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io"));
}

void OverviewPage::on_phoneButton_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io/contact/"));
}

void OverviewPage::on_phoneButtonText_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io/contact/"));
}

void OverviewPage::on_emailButton_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io/contact/"));
}

void OverviewPage::on_emailButtonText_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io/contact/"));
}

void OverviewPage::on_socialButton_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io/community/"));
}

void OverviewPage::on_socialButtonText_clicked()
{
    QDesktopServices::openUrl(QUrl("http://rebellious.io/community/"));
}
