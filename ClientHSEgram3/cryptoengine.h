#ifndef CRYPTOENGINE_H
#define CRYPTOENGINE_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>
/**
 * @class CryptoEngine
 * @brief Класс-утилита для выполнения криптографических операций.
 * * Обертка над библиотекой libsodium, предоставляющая методы для генерации
 * ключей X25519, вычисления сессионных ключей (Shared Key) и симметричного
 * шифрования/дешифрования сообщений по алгоритму ChaCha20-Poly1305.
 */
class CryptoEngine : public QObject {
  Q_OBJECT
public:
  explicit CryptoEngine(QObject *parent = nullptr);

  static bool init();

  static QPair<QByteArray, QByteArray> GenerateKeyPair();

  static QByteArray FindSharedKey(const QByteArray &myPrivateKey,
                                  const QByteArray &theirPublicKey);

  static QJsonObject encryptMessage(const QString &message,
                                    const QByteArray &sharedKey);

  static QString decryptMessage(const QJsonObject &payload,
                                const QByteArray &sharedKey);
};

#endif // CRYPTOENGINE_H
