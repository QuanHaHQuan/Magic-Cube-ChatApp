#include "login.h"
#include "ui_login.h"

login::login(QWidget *parent) :// Connect to host during construction
    QMainWindow(parent),
    ui(new Ui::login)
{
    ui->setupUi(this);
    sock=new QTcpSocket();
    sock->connectToHost("192.168.56.1",8989);
    //sock->connectToHost("192.168.43.123",8989);// Connect to host
    connect(sock,SIGNAL(connected()),this,SLOT(handConnected()));
    // Establish a connection signal and its slot function handConnected
}

login::~login()
{
    delete ui;
}

void login::handConnected()// Check connection
{
    ui->loginButton->setEnabled(true);
    ui->registerButton->setEnabled(true);
    // Establish the read signal connection
    connect(sock,SIGNAL(readyRead()),this,SLOT(handRead()));
}

void login::handRead()// Called when data is received
{
    QByteArray recvArray=sock->readAll();
    QString recvStr(recvArray);
    /*
     * Login username does not exist: #02|1
     * Login password incorrect: #02|2
     * Login successful: #02|0
     */
    QStringList recvList=recvStr.split('|');// Split by | and return a QStringList["a","b","c"]
    if(recvList[1]=="0")// Success
    {
        QMessageBox::information(this,"Info","Login successful!");
        // Code to redirect to the main page after success (to be written here)
    }
    else if(recvList[1]=="1")
    {
        QMessageBox::warning(this,"Failure","Username does not exist!");
    }
    else if(recvList[1]=="2")
    {
        QMessageBox::warning(this,"Failure","Incorrect password!");
    }
}

void login::handHide()// Reconnect after returning to this page
{
    connect(sock,SIGNAL(readyRead()),this,SLOT(handRead()));
    show();
}

void login::on_loginButton_clicked()
{
    QString uName=ui->userName->text();
    QString pWord=ui->userPassword->text();
    //#02|uName|password
    QString sendData="#02|"+uName+"|"+pWord;

    qDebug()<<sendData;
    sendData.toStdString().c_str();// Convert to C++ format first, then to C format
    sock->write(sendData.toUtf8());// Convert encoding and write data, ensuring consistent display on both Linux and Windows
}

void login::on_registerButton_clicked()
{
    disconnect(sock,SIGNAL(readyRead()),0,0);// Disconnect the response connection with the server, note that not all connections should be disconnected, specify it, or disconnect before creating a new one
    Register *reg =new Register(sock);// Dynamically allocate control without automatic release
    //createGroup *reg =new createGroup(sock);
    //chatWindow *reg=new chatWindow(sock,"me","andy","0");
    reg->show();
    connect(reg,SIGNAL(closeSig()),this,SLOT(handHide()));// If the second close signal is received, show the previous page
    this->hide();// Hide the current window   
}
