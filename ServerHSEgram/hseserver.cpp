#include "hseserver.h"

/**
 * @brief Конструктор класса HseServer.
 * @param parent Указатель на родительский объект QObject.
 */
HseServer::HseServer(QObject *parent): QTcpServer(parent) {}

/**
 * @brief Деструктор класса HseServer.
 * * Освобождает ресурсы, используемые сервером.
 */
HseServer::~HseServer() {}

/**
 * @brief Запускает сетевой сервер и инициализирует базу данных.
 * * Поднимает прослушивание (listening) на локальном адресе `LocalHost` и порту `5531`.
 * В случае успеха настраивает и открывает подключение к SQLite базе данных `hseDB.db`.
 */
void HseServer::startServer(){
    if (this->listen(QHostAddress::LocalHost,5531))
    {
        qDebug()<<"Server: Listening";
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("C:/PRACTISE/hseDB.db");

        if (!db.open()) {
            qDebug() << "Server: DB is not opened:" << db.lastError().text();
        } else {
            qDebug() << "Server: DB is opened";
        }
    }
}

/**
 * @brief Проверяет наличие пользователя в БД и возвращает его хэшированный пароль.
 * * Делает выборку из таблицы `users` по логину.
 * * @param db Открытый экземпляр базы данных QSqlDatabase.
 * @param login Строка с логином проверяемого пользователя.
 * @return QString Строка с хэшем пароля, если пользователь найден. Если пользователя нет или
 * произошла ошибка SQL, возвращает строку `"Fail"`.
 */
QString HseServer::checkLogin(QSqlDatabase db, const QString &login)
{
    QSqlQuery q(db);

    q.prepare("SELECT login, pass FROM users WHERE login = ? LIMIT 1");
    q.addBindValue(login);

    if (!q.exec() || !q.next())
    {
        qDebug()<<"Server: SQL error"<<q.lastError().text();
        return "Fail";
    }

    return q.value(1).toString();
}

/**
 * @brief Слот-обработчик разрыва соединения с клиентом (disconnected).
 * * Определяет отправителя сигнала через `sender()`, извлекает сокет, удаляет
 * пользователя из ассоциативного массива `onlineUsers` (используя его ID),
 * очищает сессию из `sessions` и ставит сокет в очередь на удаление.
 */
void HseServer::sockDisc()
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    qDebug()<<"Disconnect";
    onlineUsers.remove(UserIdByLogin(db, sessions[socket].login));
    sessions.remove(socket);
    socket->deleteLater();
}

/**
 * @brief Получает уникальный числовой ID пользователя по его текстовому логину.
 * * Выполняет SQL-запрос к таблице `users`.
 * * @param db Открытый экземпляр базы данных QSqlDatabase.
 * @param login Логин искомого пользователя.
 * @return int Идентификатор `userId`, если запись существует. В случае неудачи возвращает `-1`.
 */
int HseServer::UserIdByLogin(QSqlDatabase db, QString &login) {
    QSqlQuery q(db);
    q.prepare("SELECT userId FROM users WHERE login = ? LIMIT 1");
    q.addBindValue(login);
    if (!q.exec() || !q.next()){  return -1;}
    return q.value(0).toInt();
}

/**
 * @brief Главный диспетчер обработки входящих сетевых пакетов от клиентов (readyRead).
 * * Читает все доступные байты из вызвавшего сокета, преобразует их в JSON-документ
 * и анализирует поле `"type"`:
 * - `"start"`: Привязывает переданный клиентом публичный ключ к его аккаунту и высылает метаданные для инициализации чата.
 * - `"askhash"`: Возвращает сохраненный на сервере хэш пароля для безопасной верификации на стороне клиента.
 * - `"login"`: Проверяет соответствие хэша, авторизует сессию и заносит пользователя в пул `onlineUsers`.
 * - `"messageTo"`: Сохраняет зашифрованное E2E-сообщение и nonce в БД, а также пересылает его получателю, если тот находится в сети.
 * - `"get_public_key"`: Возвращает публичный ключ запрашиваемого пользователя для установления защищенного канала.
 * - `"find_dialog"`: Осуществляет поиск существующего или генерацию нового ID диалога между двумя пользователями.
 * - `"getHistory"`: Вызывает обработчик извлечения истории сообщений.
 * - `"check_login"`: Проверяет, свободен ли логин для проведения регистрации.
 * - `"reg"`: Вносит новую учетную запись (логин и хэш) в базу данных `users`.
 */
void HseServer::sockReady()
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    Data = socket->readAll();
    doc = QJsonDocument::fromJson(Data, &docError);
    QJsonObject clientSuccess{
        {"type", "login"},
        {"status", "success"}
    };
    QByteArray pendingData = QJsonDocument(clientSuccess).toJson();
    if (doc.object().value("type").toString() == "start")
    {
        auto login =  sessions[socket].login;
        QString public_user_key = doc.object().value("public_key").toString();
        QByteArray user_key = QByteArray::fromBase64(public_user_key.toLatin1());
        QSqlQuery q(db);
        q.prepare("UPDATE users SET public_key = ? WHERE login = ? AND public_key IS NULL");
        q.addBindValue(user_key);
        q.addBindValue(login);
        if (!q.exec()) {
            qDebug() << "Ошибка сохранения BLOB в базу:" << q.lastError().text();
        } else {
            qDebug() << "Бинарный ключ успешно сохранен!";
        }
        QJsonObject resp{
            {"type","start"},
            {"id", UserIdByLogin(db, sessions[socket].login)},
            {"dialogs",dialogsHandle(socket, db)}
        };
        QByteArray respA = QJsonDocument(resp).toJson();
        socket->write(respA);
        return;
    }
    if (doc.object().value("type").toString() == "askhash")
    {
        auto dcinfo = doc.object().value("login").toString();
        QString passHash = checkLogin(db, dcinfo);
        if (passHash!="Fail")
        {
            QJsonObject client{
                {"type","askhash"},
                {"status","success"},
                {"passHash",passHash}
            };
            QByteArray respA = QJsonDocument(client).toJson();
            socket->write(respA);
        } else {
            QJsonObject client{
                {"type","askhash"},
                {"status","failure"}
            };
            QByteArray respA = QJsonDocument(client).toJson();
            socket->write(respA);
        }

    }
    if (doc.object().value("type").toString() == "login")
    {
        auto dcinfo = doc.object().value("login").toString();
        QString passUser = checkLogin(db, dcinfo);
        if (passUser!="Fail")
        {
            if (passUser == doc.object().value("pass").toString())
            {
                auto us = UserIdByLogin(db, dcinfo);
                sessions[socket].login = dcinfo;
                sessions[socket].user_id = us;
                sessions[socket].authed = true;
                onlineUsers[us] = socket;
                socket->write(pendingData);
            } else {
                QJsonObject client{
                    {"type","login"},
                    {"status","failure"}
                };
                QByteArray respA = QJsonDocument(client).toJson();
                socket->write(respA);
            }

        } else {
            qDebug()<<"User doesn't exists";
        }
    }
    if (doc.object().value("type").toString()=="messageTo")
    {

        if (!sessions.contains(socket) || !sessions[socket].authed) return;
        QSqlQuery q(db);
        int senderId = sessions[socket].user_id;
        q.prepare("SELECT userId FROM users WHERE login = ? LIMIT 1");
        QString login = doc.object().value("user_login").toString();
        q.addBindValue(login);
        if (!q.exec() || !q.next())
        {
            qDebug()<<"Server: send to error";
            return;
        }
        int receiverId = q.value(0).toInt();
        int dialog_id = dialogIdForUsers(db, senderId, receiverId);
        QString login_sup = doc.object().value("login_sup").toString();
        QString message = doc.object().value("message").toString();
        QString nonce = doc.object().value("nonce").toString();
        q.prepare("INSERT INTO messages(user1_id,user2_id, message, dialog_id, nonce) VALUES(?,?,?,?,?)");
        q.addBindValue(senderId);
        q.addBindValue(receiverId);
        q.addBindValue(message);
        q.addBindValue(dialog_id);
        q.addBindValue(nonce);
        if (!q.exec())
        {
            qDebug()<<"SQL: error to send message to bd";
            return;
        }


        if (onlineUsers.contains(receiverId))
        {
            QJsonObject sendTo{
                {"type","messageTo"},
                {"login_rec", login},
                {"message", message},
                {"sender",senderId},
                {"dialog_id",dialog_id},
                {"receiver",receiverId},
                {"login_sup",login_sup},
                {"nonce",nonce}
            };

            QByteArray pendingData = QJsonDocument(sendTo).toJson();

            onlineUsers[receiverId]->write(pendingData);

            return;
        }
        return;
    }
    if (doc.object().value("type").toString()=="get_public_key")
    {
        int user_id = doc.object().value("user_id").toInt();
        QString user_login = doc.object().value("user_login").toString();
        QByteArray public_key;
        QSqlQuery q(db);
        q.prepare("SELECT public_key FROM users WHERE userid = ? LIMIT 1");
        q.addBindValue(user_id);
        if (!q.exec())
        {
            qDebug()<<"Ошибка взятия публичного ключа у: " << user_id;
            return;
        }
        else
        {
            if (q.next())
            {
                public_key = q.value(0).toByteArray();
                QString public_key_data = QString::fromLatin1(public_key.toBase64());
                QJsonObject send_to_client {
                    {"type","get_public_key"},
                    {"public_key",public_key_data},
                    {"user_id",user_id},
                    {"user_login", user_login}
                };
                QByteArray send_bytes_to = QJsonDocument(send_to_client).toJson();
                socket->write(send_bytes_to);
                return;
            }
            else {
                qDebug() << "Запрос выполнился, но такого пользователя нет в базе (пустой результат)";
            }

        }
    }
    if (doc.object().value("type")=="find_dialog")
    {
        QSqlQuery q(db);
        auto user_2 = doc.object().value("with_login").toString();
        auto user_id2 = UserIdByLogin(db, user_2);
        qDebug()<<user_id2;
        qDebug()<<dialogIdForUsers(db, sessions[socket].user_id, user_id2);
        q.prepare("SELECT userid FROM users WHERE login = ? LIMIT 1");
        q.addBindValue(user_2);
        if  (!q.exec() || !q.next())
        {
            QJsonObject errorFind{
                {"type","find_dialog"},
                {"ok","failure"}
            };
            QByteArray errorFindA = QJsonDocument(errorFind).toJson();
            socket->write(errorFindA);
        } else {
            QJsonObject successFind{
                {"type","find_dialog"},
                {"ok","success"},
                {"dialog_id",dialogIdForUsers(db, sessions[socket].user_id, user_id2)},
                {"user_id",user_id2},
                {"user_login",user_2}
            };
            QByteArray successFindA = QJsonDocument(successFind).toJson();
            socket->write(successFindA);
        }


    }

    if (doc.object().value("type")=="getHistory")
    {
        historyHandle(socket, doc.object().value("dialog_id").toInt(), doc.object().value("limit").toInt());
        return;
    }
    if (doc.object().value("type").toString()=="check_login"){
        QSqlQuery q(db);
        QString loginReg = doc.object().value("login").toString();
        q.prepare("SELECT 1 FROM users WHERE login = ? LIMIT 1");
        q.addBindValue(loginReg);
        if  (!q.exec())
        {
            qDebug()<<"SQL: error func(reg)";
            return;
        }
        if (!q.next()){
            QJsonObject resp {
                {"type","check_login"},
                {"ok","success"}
            };
            QByteArray respA = QJsonDocument(resp).toJson();
            socket->write(respA);
        }
        else {
            QJsonObject resp {
                {"type","check_login"},
                {"ok","failure"},
                {"text","user_exist"}
            };
            QByteArray respA = QJsonDocument(resp).toJson();
            socket->write(respA);
            return;
        }
    }
    if (doc.object().value("type").toString() == "reg")
    {
        QSqlQuery q(db);
        QString loginReg = doc.object().value("login").toString();
        q.prepare("SELECT 1 FROM users WHERE login = ? LIMIT 1");
        q.addBindValue(loginReg);
        if  (!q.exec())
        {
            qDebug()<<"SQL: error func(reg)";
            return;
        }
        if (!q.next())
        {
            QJsonObject resp {
                {"type","reg"},
                {"ok","success"}
            };
            QSqlQuery q(db);

            q.prepare("INSERT INTO users(login, pass) VALUES(?,?)");
            q.addBindValue(loginReg);
            q.addBindValue(doc.object().value("pass").toString());
            if(!q.exec())
            {
                qDebug()<<"SQL: error func(reg) 2";
                return;
            }
            QByteArray respA = QJsonDocument(resp).toJson();
            socket->write(respA);
        } else {
            QJsonObject resp {
                {"type","reg"},
                {"ok","failure"},
                {"text","user_exist"}
            };
            QByteArray respA = QJsonDocument(resp).toJson();
            socket->write(respA);
            return;
        }
    }

    qDebug()<<Data;
}

/**
 * @brief Формирует и отправляет клиенту историю сообщений конкретного диалога.
 * * Проверяет права доступа текущей сессии сокета. Выбирает из таблицы `messages`
 * текст, отправителя, получателя и nonce, ограничивая выборку параметром `limit`.
 * Результат сериализуется в формат JSON-массива и уходит обратно клиенту.
 * * @param socket Сетевой сокет запрашивающего клиента.
 * @param DialogId Идентификатор целевого чата (диалога).
 * @param limit Максимальное количество записей для извлечения из базы данных.
 */
void HseServer::historyHandle(QTcpSocket *socket, int DialogId, int limit){
    if (!sessions[socket].authed || !sessions.contains(socket))
    {
        return;
    }

    QSqlQuery q(db);
    q.prepare("SELECT message, user1_id, user2_id, nonce FROM messages WHERE dialog_id = ? LIMIT ?");
    q.addBindValue(DialogId);
    q.addBindValue(limit);

    if (!q.exec()) {
        qDebug() << "Server: sql error hisory" << q.lastError().text();
        return;
    }


    QJsonArray items;
    while (q.next()){
        QJsonObject item {
            {"message",q.value(0).toString()},
            {"sender",q.value(1).toInt()},
            {"ender", q.value(2).toInt()},
            {"nonce", q.value(3).toString()}
        };
        items.append(item);
    }

    QJsonObject resp{
        {"type","history_result"},
        {"dialog_id",DialogId},
        {"items",items}
    };
    QByteArray respA = QJsonDocument(resp).toJson();
    socket->write(respA);
}

/**
 * @brief Возвращает JSON-массив всех активных диалогов пользователя со связанными логинами собеседников.
 * * Выполняет сложный SQL `JOIN` таблицы `dialogs` и `users` с использованием оператора `CASE`,
 * чтобы динамически определить, под каким ID скрывается собеседник текущего пользователя (переданного через `:me`).
 * * @param socket Сетевой сокет запрашивающего клиента.
 * @param db Открытый экземпляр базы данных QSqlDatabase.
 * @return QJsonArray Массив JSON-объектов вида `{"dialog_id": int, "login": string}`.
 */
QJsonArray HseServer::dialogsHandle(QTcpSocket *socket, QSqlDatabase db)
{
    if (!sessions.contains(socket) || !sessions[socket].authed) { return {0};}

    int myUserId = sessions[socket].user_id;

    QSqlQuery q(db);
    q.prepare("SELECT d.dialog_id, u.login FROM dialogs d JOIN users u ON u.userId = CASE WHEN d.user1_id = :me THEN d.user2_id ELSE d.user1_id END WHERE user1_id =:me OR user2_id = :me");
    q.bindValue(":me",myUserId);

    if (!q.exec())
    {
        qDebug()<<"SQL: error func(dialogsHandle): "<<q.lastError();
        return {0};
    }

    QJsonArray items;
    while(q.next())
    {
        QJsonObject item{
            {"dialog_id", q.value(0).toInt()},
            {"login",q.value(1).toString()}
        };
        items.append(item);
    }
    return items;
}

/**
 * @brief Возвращает (или предварительно создает) сквозной ID диалога для двух пользователей.
 * * Для обеспечения детерминированности ключа `dialog_id`, функция упорядочивает ID пользователей:
 * `u2` — меньший ID, `u3` — больший ID. Результирующий композитный ID `u1` составляется конкатенацией строк.
 * Метод пытается вставить новую запись с `INSERT OR IGNORE`, а затем возвращает актуальный ID.
 * * @param db Открытый экземпляр базы данных QSqlDatabase.
 * @param idA Идентификатор первого пользователя.
 * @param idB Идентификатор второго пользователя.
 * @return int Уникальный идентификатор диалога (или `-1` в случае ошибки выполнения запроса).
 */
int HseServer::dialogIdForUsers(QSqlDatabase db, int idA, int idB)
{
    int u2 = qMin(idA, idB);
    int u3 = qMax(idA, idB);
    QString u1 = QString::number(u2)+QString::number(u3);

    QSqlQuery q(db);


    q.prepare("INSERT OR IGNORE INTO dialogs(dialog_id,user1_id,user2_id) VALUES(?,?,?)");
    q.addBindValue(u1);
    q.addBindValue(u2);
    q.addBindValue(u3);
    if (!q.exec()) return -1;


    q.prepare("SELECT dialog_id FROM dialogs WHERE user1_id=? AND user2_id=?");
    q.addBindValue(u2);
    q.addBindValue(u3);
    if (!q.exec() || !q.next()) return -1;

    return q.value(0).toInt();
}

/**
 * @brief Переопределенный виртуальный метод Qt, обрабатывающий появление нового входящего TCP-соединения.
 * * Оборачивает дескриптор в `QTcpSocket`, регистрирует под него чистую пустую структуру сессии
 * в контейнере `sessions`, после чего связывает сигналы сокета `readyRead` и `disconnected`
 * с серверными слотами.
 * * @param socketDescriptor Низкоуровневый дескриптор открытого сетевого сокета.
 */
void HseServer::incomingConnection(qintptr socketDescriptor)
{
    auto socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);
    sessions.insert(socket, {});
    qDebug()<<socketDescriptor<<": Connected to server";
    connect(socket, SIGNAL(readyRead()), this, SLOT(sockReady()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(sockDisc()));
}
