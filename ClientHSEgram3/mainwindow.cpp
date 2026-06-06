#include "mainwindow.h"
#include "./ui_mainwindow.h"

/**
 * @brief Конструктор класса MainWindow.
 * * Инициализирует форму стартового окна, создает объект сетевого сокета
 * `QTcpSocket` и связывает его ключевые сигналы (`readyRead`, `disconnected`,
 * `connected`) с соответствующими слотами.
 * * @param parent Указатель на родительский виджет.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  socket = new QTcpSocket(this);
  connect(ui->button_back_to_login, &QPushButton::clicked, this,
          [this]() { ui->stackedWidget->setCurrentWidget(ui->pageLogin); });
  connect(socket, &QTcpSocket::readyRead, this, &MainWindow::sockReady);
  connect(socket, &QTcpSocket::disconnected, this, &MainWindow::sockDisc);
  connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
}

/**
 * @brief Деструктор класса MainWindow.
 * * Освобождает память, выделенную под графический интерфейс.
 */
MainWindow::~MainWindow() { delete ui; }

/**
 * @brief Слот-обработчик успешного установления сетевого соединения
 * (connected).
 * * Проверяет наличие подготовленных к отправке отложенных данных в
 * `pendingData`. Если данные есть, отправляет их в сокет и очищает буфер.
 */
void MainWindow::onConnected() {
  if (!pendingData.isEmpty()) {
    socket->write(pendingData);
    pendingData.clear();
  }
}

/**
 * @brief Слот-обработчик закрытия или принудительного разрыва сетевого
 * соединения.
 * * Планирует безопасное уничтожение объекта сокета через `deleteLater()`.
 */
void MainWindow::sockDisc() { socket->deleteLater(); }

/**
 * @brief Слот для обработки входящих данных от сервера (readyRead).
 * * Читает входящий поток байт, преобразует его в JSON-документ и
 * маршрутизирует логику в зависимости от ответа сервера:
 * - `"type": "login"` при успехе отключает сигналы, динамически создает
 * `ChatWindow`, передает ему сокет, делает окно чата главным и скрывает текущее
 * окно авторизации.
 * - `"type": "askhash"` сверяет введенный пользователем пароль с полученным
 * хэшем Argon2id. При совпадении формирует запрос на авторизацию и инициирует
 * (или использует) подключение к серверу.
 * - `"type": "check_login"` при успешной предварительной проверке логина
 * хэширует пароль через Argon2id, отправляет финальный пакет регистрации
 * `"type": "reg"` и переключает интерфейс на форму входа.
 */
void MainWindow::sockReady() {
  QByteArray Data = socket->readAll();
  QJsonObject doc = QJsonDocument::fromJson(Data, &docError).object();
  if (docError.error != QJsonParseError::NoError)
    return;

  if (doc.value("type").toString() == "login" &&
      doc.value("status").toString() == "success") {
    disconnect(socket, &QTcpSocket::readyRead, this, &MainWindow::sockReady);
    disconnect(socket, &QTcpSocket::disconnected, this, &MainWindow::sockDisc);

    chatWin = new ChatWindow(socket, login);
    socket->setParent(chatWin);
    chatWin->show();

    this->hide();
    return;
  } else if (doc.value("status").toString() == "failure") {
    QMessageBox::warning(this, "Ошибка входа", "Неверный пароль");
    return;
  }
  if (doc.value("type").toString() == "askhash") {
    if (doc.value("status").toString() == "success") {
      QString passHash = doc.value("passHash").toString();
      QString passEnter = ui->pass_line->text();
      if (VerifyPass(passEnter, passHash)) {
        QJsonObject client{
            {"type", "login"}, {"login", login}, {"pass", passHash}};

        pendingData = QJsonDocument(client).toJson();

        if (socket->state() == QAbstractSocket::ConnectedState) {
          socket->write(pendingData);
          pendingData.clear();
          return;
        }

        socket->connectToHost("127.0.0.1", 5531);
      } else {
        QMessageBox::warning(this, "Ошибка входа", "Неверный пароль");
        return;
      }
      ui->login_line->clear();
      ui->pass_line->clear();
    } else {
      QMessageBox::warning(this, "Ошибка входа", "Нет такого пользователя");
      return;
    }
  }
  if (doc.value("type").toString() == "check_login") {
    if (doc.value("ok").toString() == "success") {
      QString pass = ui->line_reg_pass->text();
      QString login = ui->line_reg_login->text();
      pass = hashPassArgon(pass);
      if (pass.isEmpty()) {
        qDebug() << "registerUser: hash generated failure";
        return;
      }
      QString normal_login = login.trimmed().toLower();

      QJsonObject client{
          {"type", "reg"}, {"login", normal_login}, {"pass", pass}};

      pendingData = QJsonDocument(client).toJson();

      socket->write(pendingData);
      ui->line_reg_login->clear();
      ui->line_reg_pass->clear();
      ui->line_reg_repass->clear();

      ui->stackedWidget->setCurrentWidget(ui->pageLogin);

    } else {
      QMessageBox::warning(this, "Ошибка регистрации",
                           doc.value("text").toString());
    }
    return;
  }
}

/**
 * @brief Слот-обработчик нажатия на кнопку "Войти".
 * * Извлекает логин, введенный в поле интерфейса, формирует JSON-запрос
 * `"type": "askhash"` для получения хэшированного пароля с сервера (в целях
 * последующей безопасной локальной верификации). Если сокет уже подключен —
 * сразу отправляет запрос, иначе — инициирует соединение.
 */
void MainWindow::on_pushButton_clicked() {
  login = ui->login_line->text();
  QString pass = ui->pass_line->text();

  QJsonObject client{{"type", "askhash"}, {"login", login}};

  pendingData = QJsonDocument(client).toJson();

  if (socket->state() == QAbstractSocket::ConnectedState) {
    socket->write(pendingData);
    pendingData.clear();
    return;
  }

  socket->connectToHost("127.0.0.1", 5531);
}

/**
 * @brief Слот-обработчик нажатия на кнопку перехода к форме регистрации.
 * * Переключает виджет `QStackedWidget` на страницу регистрации (`pageReg`)
 * и инициирует фоновое TCP-подключение к серверу чата.
 */
void MainWindow::on_pushButton_2_clicked() {
  ui->stackedWidget->setCurrentWidget(ui->pageReg);
  socket->connectToHost("127.0.0.1", 5531);
}

/**
 * @brief Хэширует исходный пароль с помощью стойкого криптографического
 * алгоритма Argon2id.
 * * Использует внутреннюю функцию `crypto_pwhash_str` из libsodium в
 * интерактивном режиме
 * (`crypto_pwhash_OPSLIMIT_INTERACTIVE`, `crypto_pwhash_MEMLIMIT_INTERACTIVE`),
 * что обеспечивает надежную защиту от атак методом перебора (брутфорса).
 * * @param pass Открытый исходный текст пароля.
 * @return QString Строка с результирующим хэшем Argon2id (включает соль и
 * параметры), либо пустая строка в случае ошибки крипто-движка.
 */
QString MainWindow::hashPassArgon(const QString &pass) {
  char hash[crypto_pwhash_argon2id_STRBYTES]{0};

  QByteArray passBytes = pass.toUtf8();

  int result = crypto_pwhash_str(
      hash, passBytes.constData(), (unsigned long long)passBytes.size(),
      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);

  if (result != 0) {
    qDebug() << "hashPasswordError";
    return {};
  }

  return QString::fromLatin1(hash);
}

/**
 * @brief Проверяет соответствие открытого пароля сохраненному хэшу Argon2id.
 * * Функция выполняет криптографически безопасное сравнение за фиксированное
 * время (устойчивое к тайминг-атакам) с помощью `crypto_pwhash_str_verify`.
 * * @param pass Введенный пользователем открытый текст пароля.
 * @param storedHash Эталонный хэш пароля, полученный из базы данных/сервера.
 * @return true Если пароль совпал с хэшем.
 * @return false Если пароль неверный или хэш имеет некорректный формат.
 */
bool MainWindow::VerifyPass(const QString &pass, const QString &storedHash) {
  QByteArray passBytes = pass.toUtf8();

  QByteArray hashBytes = storedHash.toLatin1();
  hashBytes.resize(crypto_pwhash_STRBYTES);

  int result =
      crypto_pwhash_str_verify(hashBytes.constData(), passBytes.constData(),
                               (unsigned long long)passBytes.size());

  return (result == 0);
}

/**
 * @brief Вспомогательный метод для выполнения логики входа пользователя (в
 * текущей реализации пуст).
 */
void MainWindow::LoginUser() {}

/**
 * @brief Вспомогательный метод для выполнения логики регистрации пользователя
 * (в текущей реализации пуст).
 */
void MainWindow::RegisterUser() {}

/**
 * @brief Слот-обработчик нажатия на кнопку подтверждения регистрации.
 * * Проводит валидацию введенных данных на стороне клиента:
 * - Проверка совпадения основного и повторного пароля.
 * - Проверка минимальной длины пароля (не менее 8 символов).
 * - Проверка логина на пустоту и длину (не менее 4 символов).
 * * В случае успешной проверки приводит логин к нижнему регистру без пробелов
 * (нормализация) и отправляет JSON-запрос `"type": "check_login"` на сервер для
 * проверки занятости логина.
 */
void MainWindow::on_button_reg_clicked() {
  QString loginReg = ui->line_reg_login->text();
  QString passReg = ui->line_reg_pass->text();
  QString rePassReg = ui->line_reg_repass->text();
  if (passReg != rePassReg) {
    QMessageBox::warning(this, "Ошибка регистрации", "Пароли не совпадают");
    return;
  }
  if (passReg.size() < 8) {
    QMessageBox::warning(this, "Ошибка регистрации",
                         "Пароль должен быть больше или равен 8 символам");
    return;
  }

  if (loginReg.trimmed().isEmpty()) {
    QMessageBox::warning(this, "Ошибка регистрации", "Пустое поле логина");
    return;
  }

  if (!loginReg.trimmed().isEmpty() && loginReg.size() < 4) {
    QMessageBox::warning(
        this, "Ошибка регистрации",
        "Длина логина должна быть больше или равна 4 символам");
    return;
  }
  QString normal_login = loginReg.trimmed().toLower();

  QJsonObject client{{"type", "check_login"}, {"login", normal_login}};

  pendingData = QJsonDocument(client).toJson();

  socket->write(pendingData);

  return;
}
