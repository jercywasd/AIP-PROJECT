#ifndef HSESERVER_H
#define HSESERVER_H

#include <QObject>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QString>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFileInfo>
#include <QSqlError>
#include <QHash>
#include <QJsonArray>
#include "sodium.h"
/**
 * @class HseServer
 * @brief Сетевой TCP-сервер мессенджера на базе QTcpServer.
 * * Осуществляет координацию клиентов, централизованное хранение сообщений в SQLite,
 * распределение публичных криптографических ключей участников для организации E2E-шифрования,
 * а также проверку подлинности сессий.
 */
class HseServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit HseServer(QObject *parent = nullptr);
    ~HseServer();
    struct Session{
        bool authed = false;
        int user_id;
        QString login;
        QByteArray buffer;

    };
    QHash<QTcpSocket*, Session> sessions;
    QHash<int, QTcpSocket*> onlineUsers;
    QByteArray Data;
    QJsonDocument doc;
    QJsonParseError docError;
    QSqlDatabase db;
public slots:
    void startServer();
private slots:
    QJsonArray dialogsHandle(QTcpSocket *socket, QSqlDatabase db);
    void historyHandle(QTcpSocket *socket, int DialogId, int limit);
    void incomingConnection(qintptr socketDescrtiptor);
    void sockReady();
    void sockDisc();
    QString checkLogin(QSqlDatabase db, const QString &login);
    int UserIdByLogin(QSqlDatabase db, QString &login);
    int dialogIdForUsers(QSqlDatabase db, int idA, int idB);
};

#endif // HSESERVER_H
