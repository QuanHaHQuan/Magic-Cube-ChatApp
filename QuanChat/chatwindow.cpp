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
    //显示好友头像和信息
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
    //跳转到传输文件界面
}

void chatWindow::on_returnButton_clicked()
{
    this->hide();
    disconnect(sock,SIGNAL(readyRead()),0,0);
    emit closeSig();//触发信号，返回
}

void chatWindow::handRecv()
{
    QByteArray rData=sock->readAll();
    QString recvStr(rData);
    /*
     * 发送成功#03|0
     * 对方不在线发送失败：#03|1
     * 收到消息#03|who_sent_it|text
    */
    QStringList rList=recvStr.split('|');
    if(rList[1]=="1")
    {
        QMessageBox::warning(this,"警告","对方不在线，发送失败！");
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
        QMessageBox::warning(this,"发送失败","单次发送内容过长！");
    }
    else//#03|send_to_whom|text
    {
        QString sendChatContent="#03|"+friendName+"|"+chatContent;
        qDebug()<<sendChatContent;
        sendChatContent.toStdString().c_str();
        sock->write(sendChatContent.toUtf8());
        chatContentNow=chatContent;//保存当前发送的内容
    }
}
