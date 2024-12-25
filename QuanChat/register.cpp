#include "register.h"
#include "ui_register.h"

Register::Register(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Register)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);//close后关闭窗体
}
Register::Register(QTcpSocket *s,QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Register)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);//close后关闭窗体
    sock=s;//把login里的socket转到mainwindow里，保证让一个客户端无论有几个界面只有一个连接
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
     * 用户名已存在：#01|1
     * 注册成功：#01|0
    */
    QStringList rList=recvStr.split('|');
    if(rList[1]=="0")
    {
        QMessageBox::information(this,"提示","注册成功！");
        this->hide();
        disconnect(sock,SIGNAL(readyRead()),0,0);
        //调回之前的login，不要new，因为还是之前建立连接的那个
        emit closeSig();//触发信号
    }
    else if(rList[1]=="1")
    {
        QMessageBox::warning(this,"失败","注册用户名已存在！");
    }
}

void Register::on_registerReturn_clicked()
{
    this->hide();
    disconnect(sock,SIGNAL(readyRead()),0,0);
    //调回之前的login，不要new，因为还是之前建立连接的那个
    emit closeSig();//触发信号
}

void Register::on_registerOk_clicked()
{
    QString uNameReg=ui->userNameReg->text();
    QString pWordReg=ui->passwordReg->text();
    QString pWordRegRepeat=ui->passwordRepeat->text();
    //判断用户名合法性
    bool isName=true,isPword=true,isSame=true;
    isName=justEnglishOrNumber(uNameReg);
    if(uNameReg.isEmpty()||uNameReg.length()>20)
    {
        isName=false;
    }
    if(isName==false)
    {
        QMessageBox::warning(this,"失败","用户名格式不合法！");
    }
    //判断密码是否合法
    isPword=justEnglishOrNumber(pWordReg);
    if(pWordReg.isEmpty()||pWordRegRepeat.isEmpty()||pWordReg.length()<6||pWordReg.length()>20)
    {
        isPword=false;
    }
    if(isPword==false)
    {
        QMessageBox::warning(this,"失败","密码格式不合法！");
    }
    //判断密码是否相同
    if(pWordReg!=pWordRegRepeat)
    {
        isSame=false;
        QMessageBox::warning(this,"失败","两次输入的密码不相同！");
    }
    //传送给服务器端，#01|uName|password|0 |0是默认头像
    if(isName&&isPword&&isSame)
    {
        QString sendDataReg="#01|"+uNameReg+"|"+pWordReg+"|0";
        qDebug()<<sendDataReg;
        sendDataReg.toStdString().c_str();//先转成C++格式再转成C的格式
        sock->write(sendDataReg.toUtf8());//转变后的编码写入数据，让linux和windows下的显示统一
    }
}
