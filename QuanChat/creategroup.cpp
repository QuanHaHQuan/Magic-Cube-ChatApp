#include "creategroup.h"
#include "ui_creategroup.h"

createGroup::createGroup(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::createGroup)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);//close后关闭窗体
}

createGroup::createGroup(QTcpSocket *s,QWidget *parent):
    QMainWindow(parent),
    ui(new Ui::createGroup)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);//close后关闭窗体
    sock=s;
    connect(s,SIGNAL(readyRead()),this,SLOT(handRecv()));
}

createGroup::~createGroup()
{
    delete ui;
}

void createGroup::handRecv()
{
    QByteArray rData=sock->readAll();
    QString recvStr(rData);
    /*暂定的群名接收协议，可以更改
     * 群名已存在：#04|1
     * 创建成功：#04|0
    */
    QStringList rList=recvStr.split('|');
    if(rList[1]=="0")
    {
        QMessageBox::information(this,"提示","群创建成功！");
        this->hide();
        disconnect(sock,SIGNAL(readyRead()),0,0);
        emit closeSig();//触发关闭窗口信号
    }
    else if(rList[1]=="1")
    {
        QMessageBox::warning(this,"失败","群名称已存在！");
    }
}

bool createGroup::justChineseOrEnglishOrNumber(QString &qstr)
{//用unicode码判断是否为中英文和数字
    int count=qstr.count();
    bool bret =true;
    for(int i=0;i<count;i++)
    {
        QChar c=qstr.at(i);
        ushort uni=c.unicode();
        if((uni>=0x4E00&&uni<=0x9FA5)||(uni>='0'&&uni<='9')||(uni>='A'&&uni<='Z')||(uni>='a'&&uni<='z'))
        {

        }
        else
        {
            bret=false;
            break;
        }
    }
    return bret;
}

void createGroup::on_returnButton_clicked()//返回
{
    this->hide();
    disconnect(sock,SIGNAL(readyRead()),0,0);
    emit closeSig();//触发信号
}

void createGroup::on_createOk_clicked()
{
    QString groupName=ui->groupName->text();
    bool isName=true;
    isName=(justChineseOrEnglishOrNumber(groupName));
    if(groupName.isEmpty()||groupName.length()>20)
        isName=false;
    if(isName==false)
    {
        QMessageBox::warning(this,"失败","群名称不合法！");
    }
    else//写入格式#04|groupName
    {
        QString sendDataGroup="#04|"+groupName;
        //qDebug()<<sendDataGroup;
        sendDataGroup.toStdString().c_str();//先转成C++格式再转成C的格式
        sock->write(sendDataGroup.toUtf8());//转变后的编码写入数据，让linux和windows下的显示统一
    }
}

void createGroup::on_friendInvite_clicked()
{
    //转到好友邀请页面？
}
