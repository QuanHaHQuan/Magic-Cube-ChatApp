#include "creategroup.h"
#include "ui_creategroup.h"

createGroup::createGroup(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::createGroup)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose); // Close window after closing
}

createGroup::createGroup(QTcpSocket *s, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::createGroup)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose); // Close window after closing
    sock = s;
    connect(s, SIGNAL(readyRead()), this, SLOT(handRecv()));
}

createGroup::~createGroup()
{
    delete ui;
}

void createGroup::handRecv()
{
    QByteArray rData = sock->readAll();
    QString recvStr(rData);
    /* Temporary group name receiving protocol, can be changed
     * Group name already exists: #04|1
     * Creation successful: #04|0
    */
    QStringList rList = recvStr.split('|');
    if (rList[1] == "0")
    {
        QMessageBox::information(this, "Notice", "Group created successfully!");
        this->hide();
        disconnect(sock, SIGNAL(readyRead()), 0, 0);
        emit closeSig(); // Trigger close window signal
    }
    else if (rList[1] == "1")
    {
        QMessageBox::warning(this, "Failure", "Group name already exists!");
    }
}

bool createGroup::justChineseOrEnglishOrNumber(QString &qstr)
{ // Use Unicode code to determine if it is Chinese, English, or a number
    int count = qstr.count();
    bool bret = true;
    for (int i = 0; i < count; i++)
    {
        QChar c = qstr.at(i);
        ushort uni = c.unicode();
        if ((uni >= 0x4E00 && uni <= 0x9FA5) || (uni >= '0' && uni <= '9') || (uni >= 'A' && uni <= 'Z') || (uni >= 'a' && uni <= 'z'))
        {

        }
        else
        {
            bret = false;
            break;
        }
    }
    return bret;
}

void createGroup::on_returnButton_clicked() // Return
{
    this->hide();
    disconnect(sock, SIGNAL(readyRead()), 0, 0);
    emit closeSig(); // Trigger signal
}

void createGroup::on_createOk_clicked()
{
    QString groupName = ui->groupName->text();
    bool isName = true;
    isName = (justChineseOrEnglishOrNumber(groupName));
    if (groupName.isEmpty() || groupName.length() > 20)
        isName = false;
    if (isName == false)
    {
        QMessageBox::warning(this, "Failure", "Group name is invalid!");
    }
    else // Write format #04|groupName
    {
        QString sendDataGroup = "#04|" + groupName;
        // qDebug() << sendDataGroup;
        sendDataGroup.toStdString().c_str(); // Convert to C++ format before converting to C format
        sock->write(sendDataGroup.toUtf8()); // Convert encoding and write data to unify display on both Linux and Windows
    }
}

void createGroup::on_friendInvite_clicked()
{
    // Go to friend invitation page?
}
