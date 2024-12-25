#include "login.h"
#include "ui_login.h"

login::login(QWidget *parent) ://构造时连接主机
    QMainWindow(parent),
    ui(new Ui::login)
{
    ui->setupUi(this);
    sock=new QTcpSocket();
    sock->connectToHost("192.168.56.1",8989);
    //sock->connectToHost("192.168.43.123",8989);//连接主机
    connect(sock,SIGNAL(connected()),this,SLOT(handConnected()));
    //建立一个连接信号和其槽函数handconnected的关系
}

login::~login()
{
    delete ui;
}

void login::handConnected()//判断连接
{
    ui->loginButton->setEnabled(true);
    ui->registerButton->setEnabled(true);
    //建立读信号的关系
    connect(sock,SIGNAL(readyRead()),this,SLOT(handRead()));
}

void login::handRead()//读到数据则调用
{
    QByteArray recvArray=sock->readAll();
    QString recvStr(recvArray);
    /*
     * 登录用户名不存在：#02|1
     * 登录密码错误：#02|2
     * 登录成功：#02|0
     */
    QStringList recvList=recvStr.split('|');//用|拆开且返回一个QStringList["a","b","c"]
    if(recvList[1]=="0")//成功
    {
        QMessageBox::information(this,"提示","登录成功！");
        //这里待写成功后跳转到主页面的代码
    }
    else if(recvList[1]=="1")
    {
        QMessageBox::warning(this,"失败","登录用户名不存在！");
    }
    else if(recvList[1]=="2")
    {
        QMessageBox::warning(this,"失败","密码错误！");
    }
}

void login::handHide()//返回该页面后重新连接
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
    sendData.toStdString().c_str();//先转成C++格式再转成C的格式
    sock->write(sendData.toUtf8());//转变后的编码henxia写入数据，让linux和windows下的显示统一
}

void login::on_registerButton_clicked()
{
    disconnect(sock,SIGNAL(readyRead()),0,0);//断开与服务器建立回应的连接，注意不能全部断开连接了，要指定一下，或者在new之前断开连接
    Register *reg =new Register(sock);//从堆控件申请不会自动释放
    //createGroup *reg =new createGroup(sock);
    //chatWindow *reg=new chatWindow(sock,"me","andy","0");
    reg->show();
    connect(reg,SIGNAL(closeSig()),this,SLOT(handHide()));//如果收到第二个关闭的信号则显示前一个页面
    this->hide();//隐藏当前窗口   
}
