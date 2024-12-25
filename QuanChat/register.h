#ifndef REGISTER_H
#define REGISTER_H

#include <Qdialog>
#include <QDebug>
#include <QTcpSocket>
#include <QMessageBox>
#include <QMainWindow>

namespace Ui {
class Register;
}

class Register : public QMainWindow
{
    Q_OBJECT

public:
    explicit Register(QWidget *parent = 0);
    explicit Register(QTcpSocket *s,QWidget *parent=0);
    ~Register();
    bool justEnglishOrNumber(QString &qstr);

private slots:
    void handRecv();
    void on_registerReturn_clicked();
    void on_registerOk_clicked();

private:
    Ui::Register *ui;
    QTcpSocket *sock;
signals://信号只需要定义不需要实现
    void closeSig();
};

#endif // REGISTER_H
