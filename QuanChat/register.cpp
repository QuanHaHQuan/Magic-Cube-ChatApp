#include "register.h"
#include "ui_register.h"

Register::Register(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Register)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);// Close the window when close is triggered
}
Register::Register(QTcpSocket *s,QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Register)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);// Close the window when close is triggered
    sock=s;// Transfer the socket from login to mainwindow to ensure that a client has only one connection regardless of the number of interfaces
    connect(s,SIGNAL(readyRead()),this,SLOT(handRecv()));
}
Register::~Register()
{
    delete ui;
}
bool Register::justEnglishOrNumber(QString &qstr)
{
    bool bret=true;
    QByteArray strJudge=qstr.toLatin1();
    const char *s=strJudge.data();
    while(*s)
    {
        if((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||(*s>='0'&&*s<='9'))
        {

        }
        else
        {
            bret=false;
            break;
        }
        s++;
    }
    return bret;
}

void Register::handRecv()
{
    QByteArray rData=sock->readAll();
    QString recvStr(rData);
    /*
     * Username already exists: #01|1
     * Registration successful: #01|0
    */
    QStringList rList=recvStr.split('|');
    if(rList[1]=="0")
    {
        QMessageBox::information(this,"Notice","Registration successful!");
        this->hide();
        disconnect(sock,SIGNAL(readyRead()),0,0);
        // Go back to the previous login interface without creating a new one, as it's still using the existing connection
        emit closeSig();// Trigger signal
    }
    else if(rList[1]=="1")
    {
        QMessageBox::warning(this,"Failure","Username already exists!");
    }
}

void Register::on_registerReturn_clicked()
{
    this->hide();
    disconnect(sock,SIGNAL(readyRead()),0,0);
    // Go back to the previous login interface without creating a new one, as it's still using the existing connection
    emit closeSig();// Trigger signal
}

void Register::on_registerOk_clicked()
{
    QString uNameReg=ui->userNameReg->text();
    QString pWordReg=ui->passwordReg->text();
    QString pWordRegRepeat=ui->passwordRepeat->text();
    // Check the validity of the username
    bool isName=true,isPword=true,isSame=true;
    isName=justEnglishOrNumber(uNameReg);
    if(uNameReg.isEmpty()||uNameReg.length()>20)
    {
        isName=false;
    }
    if(isName==false)
    {
        QMessageBox::warning(this,"Failure","Invalid username format!");
    }
    // Check the validity of the password
    isPword=justEnglishOrNumber(pWordReg);
    if(pWordReg.isEmpty()||pWordRegRepeat.isEmpty()||pWordReg.length()<6||pWordReg.length()>20)
    {
        isPword=false;
    }
    if(isPword==false)
    {
        QMessageBox::warning(this,"Failure","Invalid password format!");
    }
    // Check if the two passwords match
    if(pWordReg!=pWordRegRepeat)
    {
        isSame=false;
        QMessageBox::warning(this,"Failure","The two passwords do not match!");
    }
    // Send to the server, #01|uName|password|0 |0 is the default avatar
    if(isName&&isPword&&isSame)
    {
        QString sendDataReg="#01|"+uNameReg+"|"+pWordReg+"|0";
        qDebug()<<sendDataReg;
        sendDataReg.toStdString().c_str();// Convert to C++ format first, then to C format
        sock->write(sendDataReg.toUtf8());// Encode the data before writing, ensuring uniform display in Linux and Windows
    }
}
