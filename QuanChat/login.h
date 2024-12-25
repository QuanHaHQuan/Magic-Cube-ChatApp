#ifndef LOGIN_H
#define LOGIN_H

#include <QMainWindow>
#include <QDialog>
#include <QDebug>
#include <QTcpSocket>
#include <QMessageBox>
#include <register.h>
//#include <creategroup.h>
//#include <chatwindow.h>
namespace Ui {
class login;
}

class login : public QMainWindow
{
    Q_OBJECT

public:
    explicit login(QWidget *parent = 0);
    ~login();

private slots://槽函数单独声明
    void handConnected();
    void handRead();
    void handHide();
    void on_loginButton_clicked();

    void on_registerButton_clicked();

private:
    Ui::login *ui;
    QTcpSocket *sock;//NEW一个socket指针
};

#endif // LOGIN_H
