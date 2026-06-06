#include "chatwindow.h"
#include "ui_chatwindow.h"

/**
 * @brief Конструктор класса ChatWindow.
 * * Инициализирует пользовательский интерфейс, настраивает сетевые соединения
 * (сигналы и слоты), загружает звуковые уведомления, инициализирует локальную
 * базу данных SQLite, проверяет наличие криптографических ключей и отправляет
 * стартовый пакет на сервер.
 * * @param nsocket Указатель на объект QTcpSocket для обмена данными с
 * сервером.
 * @param login Логин текущего пользователя.
 * @param parent Указатель на родительский виджет.
 */
ChatWindow::ChatWindow(QTcpSocket *nsocket, QString login, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::ChatWindow), socket(nsocket),
      myLogin(login) {
  ui->setupUi(this);
  // START: AI-DESIGN-GENERATION-CLAUDE-SONNET-4.6
  ui->dialogs->setSpacing(1);
  ui->dialogs->setFrameShape(QFrame::NoFrame);
  ui->dialogs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  ui->dialog->setSpacing(4);
  ui->dialog->setFrameShape(QFrame::NoFrame);
  ui->dialog->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->dialog->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  ui->dialog->setWordWrap(true);
  ui->dialog->setUniformItemSizes(false);
  ui->dialog->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // ::END

  connect(socket, &QTcpSocket::readyRead, this, &ChatWindow::sockReady);
  connect(socket, &QTcpSocket::disconnected, this, &ChatWindow::sockDisc);
  connect(ui->dialogs, &QListView::clicked, this, &ChatWindow::onClickedDialog);
  connect(ui->message_line, &QLineEdit::returnPressed, this,
          &ChatWindow::on_sendmes_clicked);
  ui->dialog->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  ui->dialog->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->dialog->setWordWrap(true);
  ui->dialog->setUniformItemSizes(false);
  notifySound = new QSoundEffect(this);
  notifySound->setSource(QUrl("qrc:/sounds/notify.wav"));
  qDebug() << notifySound->isLoaded();
  notifySound->setVolume(0.5f);

  db = QSqlDatabase::addDatabase("QSQLITE");
  QString dbPath = QCoreApplication::applicationDirPath() + "/client_e2e.db";
  db.setDatabaseName(dbPath);

  if (!db.open()) {
    qDebug() << "Client: DB is not opened:" << db.lastError().text();
  } else {
    qDebug() << "Client: DB is opened, path:" << dbPath;

    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS keys ("
           "my_private_key BLOB, "
           "my_public_key BLOB)");

    q.exec("CREATE TABLE IF NOT EXISTS public_keys ("
           "user_login TEXT UNIQUE, "
           "user_id INTEGER UNIQUE, "
           "public_key BLOB)");

    qDebug() << "Структура базы данных проверена/создана успешно!";
  }

  CreateKey(db);

  // START: AI-DESIGN-GENERATION-CLAUDE-SONNET-4.6
  {
    QPixmap pix(28, 28);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);

    QColor arrowColor("#aaff44");
    p.fillRect(2, 10, 4, 8, arrowColor);
    p.fillRect(6, 10, 4, 8, arrowColor);
    p.fillRect(10, 10, 4, 8, arrowColor);
    p.fillRect(14, 6, 4, 4, arrowColor);
    p.fillRect(14, 10, 4, 4, arrowColor);
    p.fillRect(14, 18, 4, 4, arrowColor);
    p.fillRect(18, 10, 4, 8, arrowColor);
    p.fillRect(22, 12, 4, 4, arrowColor);
    p.end();

    ui->sendmes->setIcon(QIcon(pix));
    ui->sendmes->setIconSize(QSize(28, 28));
    ui->sendmes->setText("");
    ui->sendmes->setFixedSize(44, 44);
    ui->sendmes->setCursor(Qt::PointingHandCursor);
    ui->sendmes->setToolTip("[ SEND ] Enter");
  }
  //:: END

  ui->line_name->setText(myLogin);
  ui->line_name->setReadOnly(1);

  // START: AI-DESIGN-GENERATION-CLAUDE-SONNET-4.6
  dialogsModel = new QStandardItemModel(this);
  messageModel = new QStandardItemModel(this);

  ui->dialogs->setModel(dialogsModel);
  ui->dialog->setModel(messageModel);

  ui->dialog->setItemDelegate(new BubbleMessageDelegate(this));

  ui->dialog->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  ui->dialog->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->dialog->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  ui->dialog->setWordWrap(true);
  ui->dialog->setUniformItemSizes(false);
  ui->dialog->setSpacing(6);

  ui->dialog->setEditTriggers(QAbstractItemView::NoEditTriggers);

  ui->dialog->setFrameShape(QFrame::NoFrame);
  ui->dialog->setLineWidth(0);

  ui->dialog->setSelectionMode(QAbstractItemView::NoSelection);
  ui->dialog->setFocusPolicy(Qt::NoFocus);

  ui->dialog->setStyleSheet("QListView {"
                            "    background-color: #0a0a0a;"
                            "    border: none;"
                            "    outline: none;"
                            "    padding: 8px 4px;"
                            "}"
                            "QListView::item {"
                            "    background: transparent;"
                            "    border: none;"
                            "    padding: 0;"
                            "    margin: 0;"
                            "}"
                            "QListView::item:selected {"
                            "    background: transparent;"
                            "}");

  ui->dialogs->setSpacing(2);
  ui->dialogs->setFrameShape(QFrame::NoFrame);
  ui->dialogs->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // ::END

  CryptoEngine::init();
  CreateKey(db);
  QString pubKeyBase64 = QString::fromLatin1(myPublicKey.toBase64());
  QJsonObject req{{"type", "start"}, {"public_key", pubKeyBase64}};
  QByteArray reqA = QJsonDocument(req).toJson();
  socket->write(reqA);
}

/**
 * @brief Деструктор класса ChatWindow.
 * * Освобождает ресурсы, выделенные под пользовательский интерфейс.
 */
ChatWindow::~ChatWindow() { delete ui; }

/**
 * @brief Проверяет наличие или генерирует новую пару ключей E2E.
 * * Функция ищет ключи в локальной базе данных. Если они найдены, то загружает
 * их. Если ключи отсутствуют, с помощью CryptoEngine генерируется новая пара,
 * которая затем сохраняется в базу данных.
 * * @param db Ссылка на открытый объект локальной базы данных QSqlDatabase.
 * @return true Если ключи успешно загружены или сгенерированы и сохранены.
 * @return false Если база данных закрыта или произошла ошибка записи.
 */
bool ChatWindow::CreateKey(QSqlDatabase &db) {
  if (!db.isOpen())
    return false;

  QSqlQuery checkQuery(db);
  checkQuery.exec("SELECT my_private_key, my_public_key FROM keys LIMIT 1");

  if (checkQuery.next()) {
    myPrivateKey = checkQuery.value(0).toByteArray();
    myPublicKey = checkQuery.value(1).toByteArray();
    qDebug() << "Ключи успешно загружены из локальной БД!";
    return true;
  }

  auto keyPair = CryptoEngine::GenerateKeyPair();
  myPublicKey = keyPair.first;
  myPrivateKey = keyPair.second;

  QSqlQuery insertQuery(db);
  insertQuery.prepare(
      "INSERT INTO keys(my_private_key, my_public_key) VALUES (?,?)");
  insertQuery.addBindValue(myPrivateKey);
  insertQuery.addBindValue(myPublicKey);

  if (!insertQuery.exec()) {
    qCritical() << "Критическая ошибка сохранения ключей в БД:"
                << insertQuery.lastError().text();
    return false;
  }

  qDebug() << "Сгенерированы и сохранены новые ключи E2E!";
  return true;
}

/**
 * @brief Слот-обработчик клика по элементу списка диалогов.
 * * Очищает текущую модель сообщений, сбрасывает счетчик непрочитанных
 * сообщений для выбранного диалога (если они были) и отправляет на сервер
 * JSON-запрос на получение истории сообщений (`getHistory`) для выбранного
 * `dialog_id`.
 * * @param index Индекс выбранного элемента в QListView.
 */
void ChatWindow::onClickedDialog(const QModelIndex &index) {
  messageModel->clear();
  QStandardItem *item = dialogsModel->itemFromIndex(index);
  if (!item) {
    return;
  }

  int dialogId = item->data(Qt::UserRole + 1).toInt();

  if (item->data(Qt::UserRole + 3).toInt() > 0) {
    item->setData(0, Qt::UserRole + 3);
    QString login = item->data(Qt::UserRole + 2).toString();
    item->setText(login);
  }
  ui->user_id_line->setText(item->data(Qt::UserRole + 2).toString());

  QJsonObject req{
      {"type", "getHistory"}, {"dialog_id", dialogId}, {"limit", 50}};
  QByteArray reqA = QJsonDocument(req).toJson();
  socket->write(reqA);
  return;
}

/**
 * @brief Слот для обработки входящих данных из сокета (readyRead).
 * * Читает сырые данные от сервера, парсит их в JSON и распределяет обработку
 * в зависимости от поля "type":
 * - "start": инициализация диалогов текущего пользователя.
 * - "messageTo": обработка входящего сообщения (дешифрование и вывод на экран
 * или инкремент счетчика непрочитанных).
 * - "find_dialog": обработка результата поиска/создания диалога.
 * - "get_public_key": получение и сохранение публичного ключа собеседника.
 * - "history_result": загрузка, дешифрование и отображение истории сообщений.
 */
void ChatWindow::sockReady() {
  QByteArray recv = socket->readAll();
  qDebug() << "ПРИШЛО ОТ СЕРВЕРА:" << recv; // <-- ДОБАВЬ СТРОКУ
  QJsonDocument doc = QJsonDocument::fromJson(recv, &error);
  if (doc.object().value("type").toString() == "start") {
    myUserId = doc.object().value("id").toInt();
    QJsonArray items = doc.object().value("dialogs").toArray();
    qDebug() << "Hello";

    for (const auto &it : items) {
      QJsonObject item = it.toObject();
      int dialogId = item.value("dialog_id").toInt();
      dialogs.append(dialogId);
      QString login = item.value("login").toString();
      handleOpenDialogResult(login, dialogId);
    }
    return;
  }

  if (doc.object().value("type").toString() == "messageTo") {
    int dialog_id = doc.object().value("dialog_id").toInt();
    QString login = doc.object().value("login_sup").toString();
    int user_id = doc.object().value("sender").toInt();
    qDebug() << "Im here: " + login;
    if (CurrentDialogId == dialog_id) {
      QSqlQuery q(db);
      QByteArray public_user_key;
      q.prepare("SELECT public_key FROM public_keys WHERE user_id = ?");
      q.addBindValue(user_id);
      if (q.exec() && q.next()) {
        public_user_key = q.value(0).toByteArray();
      } else {
        public_user_key = GetPublicKey(user_id, login);
        qDebug() << "Ошибка в messageTo";
        return;
      }
      QJsonObject payload;
      payload["ciphertext"] = doc.object().value("message").toString();
      payload["nonce"] = doc.object().value("nonce").toString();
      QByteArray sharedKey =
          CryptoEngine::FindSharedKey(myPrivateKey, public_user_key);
      QString message = CryptoEngine::decryptMessage(payload, sharedKey);
      int sender = doc.object().value("sender").toInt();
      bool sourceMe = (sender == myUserId);
      addMessage(message, sourceMe);
      qDebug() << message;
    } else {

      QSqlQuery checkKey(db);
      checkKey.prepare("SELECT public_key FROM public_keys WHERE user_id = ?");
      checkKey.addBindValue(user_id);
      bool keyExists = false;
      if (checkKey.exec() && checkKey.next()) {
        QByteArray existingKey = checkKey.value(0).toByteArray();
        if (!existingKey.isEmpty()) {
          keyExists = true;
          qDebug() << "Публичный ключ уже есть в БД для user_id =" << user_id;
        }
      }
      if (!keyExists) {
        qDebug()
            << "Публичный ключ отсутствует, запрашиваем у сервера для user_id ="
            << user_id;
        GetPublicKey(user_id, login);
      }

      bool dialogExists = false;
      for (auto id : dialogs) {
        if (dialog_id == id) {
          dialogExists = true;
          break;
        }
      }
      if (!dialogExists) {
        dialogs.append(dialog_id);
        handleOpenDialogResult(login, dialog_id);
      }

      playNotice();
      markDialogUnread(dialog_id);
    }
  }

  if (doc.object().value("type").toString() == "find_dialog" &&
      doc.object().value("ok") == "failure") {
    // realize
    return;
  } else if (doc.object().value("ok") == "success") {
    auto user_id = doc.object().value("user_id").toInt();
    int dialog_id = doc.object().value("dialog_id").toInt();
    QString user_login = doc.object().value("user_login").toString();
    qDebug() << "I'm here: " << "User_id: " << user_id;
    GetPublicKey(user_id, user_login);
    bool flag = 0;
    for (auto id : dialogs) {
      if (dialog_id == id) {
        flag = 1;
        break;
      }
    }
    if (flag == 0) {
      dialogs.append(dialog_id);
      handleOpenDialogResult(ui->addDialog->text(),
                             doc.object().value("dialog_id").toInt());
      ;
    }
    return;
  }
  if (doc.object().value("type").toString() == "get_public_key") {
    auto user_id = doc.object().value("user_id").toInt();
    QString user_login = doc.object().value("user_login").toString();
    qDebug() << "I'm here: " << "User_id: " << user_id;
    QString public_key = doc.object().value("public_key").toString();
    QByteArray public_key_raw = QByteArray::fromBase64(public_key.toLatin1());
    GetPublicKey(user_id, user_login, 1, public_key_raw);
    return;
  }
  if (doc.object().value("type").toString() == "history_result") {
    QJsonArray items = doc.object().value("items").toArray();
    CurrentDialogId = doc.object().value("dialog_id").toInt();
    if (items.isEmpty()) {
      qDebug() << "История диалога" << CurrentDialogId << "пуста.";
      return;
    }
    QJsonObject ender_dragon = items[0].toObject();
    int sender_dragon = ender_dragon.value("sender").toInt();
    int sender_bragon = ender_dragon.value("ender").toInt();
    QByteArray public_brother_key;
    if (sender_dragon != myUserId) {
      QSqlQuery q(db);
      q.prepare("SELECT public_key FROM public_keys WHERE user_id = ?");
      q.addBindValue(sender_dragon);
      if (!q.exec())
        return;
      if (q.next())
        public_brother_key = q.value(0).toByteArray();
    } else {
      QSqlQuery q(db);
      q.prepare("SELECT public_key FROM public_keys WHERE user_id = ?");
      q.addBindValue(sender_bragon);
      if (!q.exec())
        return;
      if (q.next())
        public_brother_key = q.value(0).toByteArray();
    }
    QByteArray their_shared_key =
        CryptoEngine::FindSharedKey(myPrivateKey, public_brother_key);

    for (const auto &it : items) {
      QJsonObject item = it.toObject();
      QString text = item.value("message").toString();
      QJsonObject payload;
      payload["ciphertext"] = text;
      payload["nonce"] = item.value("nonce").toString();
      int sender = item.value("sender").toInt();
      bool sourceMe = (sender == myUserId);
      if (sourceMe) {
        text = CryptoEngine::decryptMessage(payload, their_shared_key);
      } else {
        text = CryptoEngine::decryptMessage(payload, their_shared_key);
      }
      addMessage(text, sourceMe);
    }
    return;
  }

  qDebug() << doc.object().value("type");
}

/**
 * @brief Добавляет текстовое сообщение графическую модель отображения
 * сообщений.
 * * Создает новый элемент списка `QStandardItem`, выравнивает текст по правому
 * краю, если сообщение отправлено текущим пользователем (`sourceMe == true`),
 * или по левому краю, если оно входящее. После добавления прокручивает
 * представление вниз.
 * * @param text Расшифрованный текст сообщения.
 * @param sourceMe Флаг авторства сообщения (true — от себя, false — от
 * собеседника).
 */
void ChatWindow::addMessage(const QString &text, bool sourceMe) {
  if (text.trimmed().isEmpty()) {
    qDebug() << "addMessage: пустой текст, пропуск";
    return;
  }

  auto *it = new QStandardItem(text);

  it->setData(sourceMe, Qt::UserRole + 5);

  messageModel->appendRow(it);

  ui->dialog->scrollToBottom();
}
/**
 * @brief Слот-обработчик закрытия/разрыва сетевого соединения.
 * * Планирует безопасное удаление объекта `QTcpSocket` через механизм
 * `deleteLater()`.
 */
void ChatWindow::sockDisc() { socket->deleteLater(); }

/**
 * @brief Двухрежимный метод взаимодействия с публичными ключами пользователей.
 * * Если `flag == 0`, метод формирует и отправляет запрос на сервер для
 * получения публичного ключа пользователя. Возвращает пустой массив. Если `flag
 * == 1`, метод сохраняет (или игнорирует дубликаты) переданный публичный ключ
 * `public_key` в локальную таблицу `public_keys`.
 * * @param userID Идентификатор удаленного пользователя.
 * @param user_login Логин удаленного пользователя.
 * @param flag Режим работы метода (0 — запросить у сервера, 1 — сохранить в
 * БД).
 * @param public_key Сырые данные публичного ключа (используется только при flag
 * = 1).
 * @return QByteArray Возвращает сохраненный ключ в режиме сохранения, либо
 * пустой массив в режиме запроса.
 */
QByteArray ChatWindow::GetPublicKey(int userID, QString user_login, bool flag,
                                    const QByteArray &public_key) {
  if (flag == 0) {
    QJsonObject send_to_server{{"type", "get_public_key"},
                               {"user_id", userID},
                               {"user_login", user_login}};
    QByteArray pendingData = QJsonDocument(send_to_server).toJson();
    socket->write(pendingData);
    return QByteArray();
  } else {
    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO public_keys(user_login, user_id, "
              "public_key) VALUES(?, ?, ?)");
    q.addBindValue(user_login);
    q.addBindValue(userID);
    q.addBindValue(public_key);
    if (!q.exec()) {
      qDebug() << "Ошибка вставки публичного ключа другого пользователя";
    } else {
      return public_key;
    }
  }
}

/**
 * @brief Слот-обработчик клика по кнопке отправки сообщения (или нажатия
 * Enter).
 * * Берет текст из поля ввода, извлекает из БД публичный ключ собеседника по
 * его логину, генерирует общий секрет (Shared Key) на базе `myPrivateKey`,
 * шифрует сообщение и отправляет JSON-пакет `"type": "messageTo"` на сервер.
 * Также отображает отправленное сообщение в окне чата и очищает строку ввода.
 */
void ChatWindow::on_sendmes_clicked() {
  QString user_login = ui->user_id_line->text();
  int user_id;
  QString message = ui->message_line->text();
  QByteArray my_private_key = myPrivateKey;
  QByteArray my_public_key = myPublicKey;
  QByteArray their_public_key;
  QSqlQuery q(db);
  q.prepare("SELECT public_key, user_id FROM public_keys WHERE user_login = ?");
  q.addBindValue(user_login);
  if (q.exec() && q.next()) {
    their_public_key = q.value(0).toByteArray();
    user_id = q.value(1).toInt();
    qDebug() << "Публичный ключ пользователя успешно загружен из локальной БД";
  } else {
    qDebug() << "Ключ (не)успешно загружен из локальной БД"
             << q.lastError().text();
    return;
  }
  QByteArray sharedKey =
      CryptoEngine::FindSharedKey(my_private_key, their_public_key);
  QJsonObject encryptPayload = CryptoEngine::encryptMessage(message, sharedKey);

  QJsonObject sendTo{{"type", "messageTo"},
                     {"user_id", user_id},
                     {"message", encryptPayload["ciphertext"]},
                     {"login_sup", myLogin},
                     {"nonce", encryptPayload["nonce"]},
                     {"user_login", user_login}};

  QByteArray pendingData = QJsonDocument(sendTo).toJson();
  socket->write(pendingData);
  addMessage(message, 1);
  ui->message_line->clear();
  return;
}

/**
 * @brief Добавляет новый диалог в начало списка диалогов (`dialogsModel`).
 * * Создает элемент `QStandardItem`, привязывает к нему метаданные (dialog_id,
 * login, счетчик непрочитанных) через механизм `Qt::UserRole`, делает его
 * нередактируемым и вставляет на нулевую позицию списка.
 * * @param login Логин собеседника.
 * @param dialog_id Идентификатор общего диалога.
 */
void ChatWindow::handleOpenDialogResult(QString login, int dialog_id) {

  auto *item = new QStandardItem(login);
  item->setData(dialog_id, Qt::UserRole + 1);
  item->setData(login, Qt::UserRole + 2);
  item->setData(0, Qt::UserRole + 3);
  dialogsModel->insertRow(0, item);
  item->setEditable(false);
}

/**
 * @brief Воспроизводит звуковой сигнал входящего уведомления.
 * * Если объект `notifySound` успешно инициализирован, запускает проигрывание
 * wav-файла.
 */
void ChatWindow::playNotice() {
  qDebug() << notifySound->status();
  if (notifySound) {
    notifySound->play();
  }
}

/**
 * @brief Помечает диалог как содержащий непрочитанные сообщения.
 * * Перебирает список диалогов в модели, находит нужный по `dialog_id`,
 * инкрементирует внутреннее значение счетчика непрочитанных (`Qt::UserRole +
 * 3`) и обновляет отображаемый текст элемента, добавляя количество сообщений в
 * скобках.
 * * @param dialog_id Идентификатор диалога, куда пришло новое сообщение.
 */
void ChatWindow::markDialogUnread(int dialog_id) {
  for (int i = 0; i < dialogsModel->rowCount(); ++i) {
    auto *it = dialogsModel->item(i);
    if (it->data(Qt::UserRole + 1).toInt() == dialog_id) {
      int unread = it->data(Qt::UserRole + 3).toInt() + 1;
      it->setData(unread, Qt::UserRole + 3);

      QString login = it->data(Qt::UserRole + 2).toString();
      qDebug() << login;
      it->setText(login + " (" + QString::number(unread) + ")");
      break;
    }
  }
}

/**
 * @brief Слот-обработчик нажатия на кнопку поиска диалога.
 * * Считывает текст (логин пользователя) из поля ввода `addDialog` и отправляет
 * на сервер запрос `"type": "find_dialog"` для инициализации или поиска беседы.
 */
void ChatWindow::on_searchDialog_clicked() {
  QJsonObject proto_dialog_add = {{"type", "find_dialog"},
                                  {"with_login", ui->addDialog->text()}};

  QByteArray proto_dialog = QJsonDocument(proto_dialog_add).toJson();
  socket->write(proto_dialog);
}
