// Copyright (c) 2016-2017 The Qtum Core developers
// Copyright (c) 2018 The Rebellious developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "addressfield.h"
//#include "wallet/wallet.h"
//#include "base58.h"
#include "qvalidatedlineedit.h"
#include "bitcoinaddressvalidator.h"
#include <boost/foreach.hpp>
#include <QLineEdit>
#include <QCompleter>

using namespace std;

AddressField::AddressField(QWidget *parent) :
    QComboBox(parent),
    m_addressType(AddressField::UTXO),
    m_addressTableModel(0),
    m_addressColumn(0),
    m_typeRole(Qt::UserRole),
    m_receive("R")

{
    // Set editable state
    setComboBoxEditable(false);

    // Connect signals and slots
    connect(this, SIGNAL(addressTypeChanged(AddressType)), SLOT(on_addressTypeChanged()));
}

QString AddressField::currentText() const
{
    if(isEditable())
    {
        return lineEdit()->text();
    }

    int index = currentIndex();
    if(index == -1)
    {
        return QString();
    }

    return itemText(index);
}

bool AddressField::isValidAddress()
{
    if(!isEditable())
    {
        if(currentIndex() != -1)
            return true;
        else
            return false;
    }

}

void AddressField::setComboBoxEditable(bool editable)
{
    QValidatedLineEdit *validatedLineEdit = new QValidatedLineEdit(this);
    setLineEdit(validatedLineEdit);
    setEditable(editable);
    if(editable)
    {
        QValidatedLineEdit *validatedLineEdit = (QValidatedLineEdit*)lineEdit();
        validatedLineEdit->setCheckValidator(new BitcoinAddressCheckValidator(parent()));
        completer()->setCompletionMode(QCompleter::InlineCompletion);
        connect(validatedLineEdit, SIGNAL(editingFinished()), this, SLOT(on_editingFinished()));
    }
}

void AddressField::on_refresh()
{
    // Initialize variables
    QString currentAddress = currentText();
    m_stringList.clear();


    // Fill the list with address
    if(m_addressType == AddressField::UTXO)
    {

        // Add all available addresses if 0 address ballance for token is enabled
        if(m_addressTableModel)
        {
            // Fill the list with user defined address
            for(int row = 0; row < m_addressTableModel->rowCount(); row++)
            {
                QModelIndex index = m_addressTableModel->index(row, m_addressColumn);
                QString strAddress = m_addressTableModel->data(index).toString();
                QString type = m_addressTableModel->data(index, m_typeRole).toString();

            }

        }
    }

    // Update the current index
    int index = m_stringList.indexOf(currentAddress);
    m_stringModel.setStringList(m_stringList);
    setModel(&m_stringModel);
    setCurrentIndex(index);
}

void AddressField::on_addressTypeChanged()
{
    m_stringList.clear();
    on_refresh();
}

void AddressField::on_editingFinished()
{
    Q_EMIT editTextChanged(QComboBox::currentText());
}


void AddressField::setReceive(const QString &receive)
{
    m_receive = receive;
}

void AddressField::setTypeRole(int typeRole)
{
    m_typeRole = typeRole;
}

void AddressField::setAddressColumn(int addressColumn)
{
    m_addressColumn = addressColumn;
}

void AddressField::setAddressTableModel(QAbstractItemModel *addressTableModel)
{
    if(m_addressTableModel)
    {
        disconnect(m_addressTableModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(on_refresh()));
        disconnect(m_addressTableModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(on_refresh()));
    }

    m_addressTableModel = addressTableModel;
    connect(m_addressTableModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(on_refresh()));
    connect(m_addressTableModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(on_refresh()));

    on_refresh();
}
