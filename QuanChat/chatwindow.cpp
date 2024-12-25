#include "chatwindow.h"
#include "ui_chatwindow.h"

chatWindow::chatWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::chatWindow)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
}

chatWindow::chatWindow(QTcpSocket *s,QString mName,QString fName,QString fHead,QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::chatWindow)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    sock=s;
    // Display friend's avatar and information
    friendName=fName;
    friendHead=fHead;
    myName=mName;
    ui->friendIcon->setText(friendHead);
    ui->friendMessage->setText(friendName);
    connect(s,SIGNAL(readyRead()),this,SLOT(handRecv()));
}
chatWindow::~chatWindow()
{
    delete ui;
}

void chatWindow::on_fileSend_clicked()
{
    // Navigate to file transfer interface
}

void chatWindow::on_returnButton_clicked()
{
    this->hide();
    disconnect(sock,SIGNAL(readyRead()),0,0);
    emit closeSig();// Trigger signal, return
}

void chatWindow::handRecv()
{
    QByteArray rData=sock->readAll();
    QString recvStr(rData);
    /*
     * Send success #03|0
     * Other party is offline, send failed: #03|1
     * Received message #03|who_sent_it|text
    */
    QStringList rList=recvStr.split('|');
    if(rList[1]=="1")
    {
        QMessageBox::warning(this,"Warning","The other party is offline, sending failed!");
    }
    if(rList[1]=="0")
    {
        ui->chatBox->append(myName+":"+chatContentNow);
        ui->chatEdit->setText("");
    }
    if(rList[1]!="0"&&rList[1]!="1")
    {
        ui->chatBox->append(rList[1]+":"+rList[2]);
    }
}

void chatWindow::on_sendMessage_clicked()
{
    QString chatContent=ui->chatEdit->toPlainText();
    QString chatTem=chatContent;
    chatTem.toUtf8();
    if(chatTem.length()>4095)
    {
        QMessageBox::warning(this,"Send Failed","The content to send is too long!");
    }
    else//#03|send_to_whom|text
    {
        QString sendChatContent="#03|"+friendName+"|"+chatContent;
        qDebug()<<sendChatContent;
        sendChatContent.toStdString().c_str();
        sock->write(sendChatContent.toUtf8());
        chatContentNow=chatContent;// Save the current sent content
    }
}
