#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <Qdialog>
#include <QDebug>
#include <QTcpSocket>
#include <QMessageBox>
#include <QMainWindow>

namespace Ui {
class chatWindow;
}

class chatWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit chatWindow(QWidget *parent = 0);
    explicit chatWindow(QTcpSocket *s,QString mName,QString fName,QString fHead,QWidget *parent=0);
    ~chatWindow();

private slots:
    void handRecv();
    void on_fileSend_clicked();

    void on_returnButton_clicked();

    void on_sendMessage_clicked();

private:
    Ui::chatWindow *ui;
    QTcpSocket *sock;
    QString friendName,friendHead,myName;
    QString chatContentNow;
signals://信号只需要定义不需要实现
    void closeSig();
};

#endif // CHATWINDOW_H
