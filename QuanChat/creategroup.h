#ifndef CREATEGROUP_H
#define CREATEGROUP_H

#include <QMainWindow>
#include <QDialog>
#include <QDebug>
#include <QTcpSocket>
#include <QMessageBox>
namespace Ui {
class createGroup;
}

class createGroup : public QMainWindow
{
    Q_OBJECT

public:
    explicit createGroup(QWidget *parent = 0);
    explicit createGroup(QTcpSocket *s,QWidget *parent = 0);//此处需要传入主界面的socket函数sock
    ~createGroup();
    bool justChineseOrEnglishOrNumber(QString &qstr);
private slots:
    void handRecv();
    void on_returnButton_clicked();

    void on_createOk_clicked();

    void on_friendInvite_clicked();

private:
    Ui::createGroup *ui;
    QTcpSocket *sock;
signals://信号只需要定义不需要实现
    void closeSig();
};

#endif // CREATEGROUP_H
