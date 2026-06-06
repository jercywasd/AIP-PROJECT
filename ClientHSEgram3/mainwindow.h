#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "chatwindow.h"
#include "sodium.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMainWindow>
#include <QMessageBox>
#include <QString>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
/**
 * @class MainWindow
 * @brief Класс окна авторизации и регистрации пользователей.
 * * Предоставляет графический интерфейс (GUI) для ввода учетных данных,
 * выполняет клиентскую валидацию строк, осуществляет криптографическую защиту
 * паролей с помощью Argon2id и управляет жизненным циклом сетевого сокета до
 * момента входа в чат.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();
  ChatWindow *chatWin;
  QTcpSocket *socket;
  struct ClientsBuff {
    QString login;
    QString pass;
  };
  QString login;
  QByteArray pendingData;
  QJsonDocument doc;
  QJsonParseError docError;
public slots:

  void sockReady();
  void sockDisc();
  void onConnected();

private slots:
  QString hashPassArgon(const QString &pass);
  void LoginUser();
  bool VerifyPass(const QString &pass, const QString &storedHash);
  void RegisterUser();

  void on_pushButton_clicked();

  void on_pushButton_2_clicked();

  void on_button_reg_clicked();

private:
  Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
