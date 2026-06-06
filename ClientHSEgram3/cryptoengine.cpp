#include "cryptoengine.h"

#include <QDebug>
#include <QJsonDocument>
#include <sodium.h>

/**
 * @brief Конструктор класса CryptoEngine.
 * @param parent Указатель на родительский объект QObject.
 */
CryptoEngine::CryptoEngine(QObject *parent) : QObject(parent) {}

/**
 * @brief Инициализирует библиотеку libsodium.
 * * Данную функцию необходимо вызвать один раз перед использованием любых
 * других криптографических функций библиотеки. Безопасна для повторного вызова.
 * * @return true Если библиотека успешно инициализирована (или уже была
 * инициализирована).
 * @return false Если произошла критическая ошибка при инициализации.
 */
bool CryptoEngine::init() {
  if (sodium_init() < 0) {
    qCritical() << "Ошибка инициализации libsodium";
    return false;
  }
  return true;
}

/**
 * @brief Генерирует новую асимметричную пару ключей (публичный и приватный).
 * * Использует функцию `crypto_box_keypair` для создания пары ключей,
 * предназначенной для обмена ключами и шифрования (X25519).
 * * @return QPair<QByteArray, QByteArray> Пара, где первый элемент (first) —
 * публичный ключ, а второй (second) — приватный (секретный) ключ.
 */
QPair<QByteArray, QByteArray> CryptoEngine::GenerateKeyPair() {
  QByteArray publicKey(crypto_box_PUBLICKEYBYTES, 0);
  QByteArray privateKey(crypto_box_SECRETKEYBYTES, 0);

  crypto_box_keypair(reinterpret_cast<unsigned char *>(publicKey.data()),
                     reinterpret_cast<unsigned char *>(privateKey.data()));

  return qMakePair(publicKey, privateKey);
}

/**
 * @brief Вычисляет общий секретный ключ (Shared Key) на основе схемы
 * Диффи-Хеллмана.
 * * Функция `crypto_box_beforenm` выполняет скалярное умножение на
 * эллиптической кривой X25519, используя собственный приватный ключ и публичный
 * ключ собеседника. Полученный результат далее используется для симметричного
 * шифрования сообщений.
 * * @param myPrivateKey Свой приватный ключ (размером
 * crypto_box_SECRETKEYBYTES).
 * @param theirPublicKey Публичный ключ собеседника (размером
 * crypto_box_PUBLICKEYBYTES).
 * @return QByteArray Общий секретный ключ (размером crypto_box_BEFORENMBYTES),
 * либо пустой массив в случае ошибки.
 */
QByteArray CryptoEngine::FindSharedKey(const QByteArray &myPrivateKey,
                                       const QByteArray &theirPublicKey) {
  if (myPrivateKey.size() != crypto_box_SECRETKEYBYTES ||
      theirPublicKey.size() != crypto_box_PUBLICKEYBYTES) {
    qCritical() << "Ошибка генерации ключей: неверный размер";
    return QByteArray();
  }

  QByteArray sharedKey(crypto_box_BEFORENMBYTES, 0);
  int result = crypto_box_beforenm(
      reinterpret_cast<unsigned char *>(sharedKey.data()),
      reinterpret_cast<const unsigned char *>(theirPublicKey.data()),
      reinterpret_cast<const unsigned char *>(myPrivateKey.data()));
  if (result != 0) {
    qCritical() << "Ошибка генерации sharedKey";
    return QByteArray();
  }
  return sharedKey;
}

/**
 * @brief Шифрует текстовое сообщение с использованием алгоритма
 * ChaCha20-Poly1305 (IETF).
 * * Функция преобразует строку в UTF-8, генерирует случайный одноразовый код
 * (nonce) с помощью криптографически стойкого генератора `randombytes_buf` и
 * выполняет шифрование с аутентификацией данных (AEAD). Результат упаковывается
 * в JSON.
 * * @param message Исходный открытый текст сообщения.
 * @param sharedKey Общий секретный сессионный ключ.
 * @return QJsonObject JSON-объект, содержащий зашифрованный текст
 * ("ciphertext") и одноразовый код ("nonce"), закодированные в Base64. В случае
 * ошибки возвращает пустой объект.
 */
QJsonObject CryptoEngine::encryptMessage(const QString &message,
                                         const QByteArray &sharedKey) {
  QJsonObject resultPayload;

  if (sharedKey.size() != crypto_box_BEFORENMBYTES) {
    qWarning() << "Общий секрет не инициализирован";
    return resultPayload;
  }

  QByteArray plainTextBytes = message.toUtf8();

  QByteArray nonce(crypto_aead_chacha20poly1305_ietf_NPUBBYTES, 0);
  randombytes_buf(nonce.data(), nonce.size());

  QByteArray cipherText(
      plainTextBytes.size() + crypto_aead_chacha20poly1305_ietf_ABYTES, 0);
  unsigned long long cipherText_len = 0;

  int res = crypto_aead_chacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(cipherText.data()), &cipherText_len,
      reinterpret_cast<const unsigned char *>(plainTextBytes.constData()),
      plainTextBytes.size(), nullptr, 0, nullptr,
      reinterpret_cast<const unsigned char *>(nonce.constData()),
      reinterpret_cast<const unsigned char *>(sharedKey.constData()));

  if (res != 0) {
    qWarning() << "Ошибка при шифровании сообщения";
    return resultPayload;
  }

  cipherText.resize(cipherText_len);
  resultPayload["ciphertext"] = QString::fromLatin1(cipherText.toBase64());
  resultPayload["nonce"] = QString::fromLatin1(nonce.toBase64());

  return resultPayload;
}

/**
 * @brief Расшифровывает сообщение, зашифрованное алгоритмом ChaCha20-Poly1305
 * (IETF).
 * * Извлекает зашифрованный текст и nonce из JSON-объекта, декодирует их из
 * Base64, проверяет целостность данных (имитовставку Poly1305) и подлинность
 * сообщения. Если данные были изменены в процессе передачи, расшифровка
 * завершится ошибкой.
 * * @param payload JSON-объект с полями "ciphertext" и "nonce" в формате
 * Base64.
 * @param sharedKey Общий секретный сессионный ключ.
 * @return QString Восстановленный открытый текст (в UTF-8), либо сообщение об
 * ошибке в случае нарушения целостности или невалидных ключей.
 */
QString CryptoEngine::decryptMessage(const QJsonObject &payload,
                                     const QByteArray &sharedKey) {
  if (sharedKey.size() != crypto_box_BEFORENMBYTES) {
    return QString("[Ошибка: Ключ шифрования отсутствует]");
  }

  QByteArray ciphertext =
      QByteArray::fromBase64(payload["ciphertext"].toString().toLatin1());
  QByteArray nonce =
      QByteArray::fromBase64(payload["nonce"].toString().toLatin1());

  if (ciphertext.size() < crypto_aead_chacha20poly1305_ietf_ABYTES) {
    return QString("[Ошибка: Некорректный размер сообщения]");
  }

  QByteArray decrypted(
      ciphertext.size() - crypto_aead_chacha20poly1305_ietf_ABYTES, 0);
  unsigned long long decrypted_len = 0;

  int result = crypto_aead_chacha20poly1305_ietf_decrypt(
      reinterpret_cast<unsigned char *>(decrypted.data()), &decrypted_len,
      nullptr, reinterpret_cast<const unsigned char *>(ciphertext.constData()),
      ciphertext.size(), nullptr, 0,
      reinterpret_cast<const unsigned char *>(nonce.constData()),
      reinterpret_cast<const unsigned char *>(sharedKey.constData()));

  if (result != 0) {
    return QString("Не удалось расшифровать сообщение");
  }

  decrypted.resize(decrypted_len);
  return QString::fromUtf8(decrypted);
}
