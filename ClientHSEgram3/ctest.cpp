#include "ctest.h"
#include "cryptoengine.h"

void HseMessengerTest::testKeyPairGeneration() {

  auto keyPairA = CryptoEngine::GenerateKeyPair();

  QVERIFY(!keyPairA.first.isEmpty());
  QVERIFY(!keyPairA.second.isEmpty());

  QCOMPARE(keyPairA.first.size(), 32);
  QCOMPARE(keyPairA.second.size(), 32);
}

void HseMessengerTest::testE2EEncryptionDecryption() {

  auto aliceKeys = CryptoEngine::GenerateKeyPair();
  auto bobKeys = CryptoEngine::GenerateKeyPair();

  QByteArray aliceSharedKey =
      CryptoEngine::FindSharedKey(aliceKeys.second, bobKeys.first);

  QByteArray bobSharedKey =
      CryptoEngine::FindSharedKey(bobKeys.second, aliceKeys.first);

  QVERIFY(!aliceSharedKey.isEmpty());
  QCOMPARE(aliceSharedKey, bobSharedKey);

  QString originalMessage = "Привет! Это мастерская настроения.";

  QJsonObject encryptedPayload =
      CryptoEngine::encryptMessage(originalMessage, aliceSharedKey);
  QVERIFY(encryptedPayload.contains("ciphertext"));
  QVERIFY(encryptedPayload.contains("nonce"));

  QString decryptedMessage =
      CryptoEngine::decryptMessage(encryptedPayload, bobSharedKey);

  QCOMPARE(decryptedMessage, originalMessage);
}

void HseMessengerTest::testNegativeScenarios() {
  auto aliceKeys = CryptoEngine::GenerateKeyPair();
  auto bobKeys = CryptoEngine::GenerateKeyPair();
  QByteArray aliceSharedKey =
      CryptoEngine::FindSharedKey(aliceKeys.second, bobKeys.first);

  QString originalMessage = "Секретный текст";
  QJsonObject encryptedPayload =
      CryptoEngine::encryptMessage(originalMessage, aliceSharedKey);

  QByteArray wrongSharedKey(32, 'X');
  QString decryptedWithWrongKey =
      CryptoEngine::decryptMessage(encryptedPayload, wrongSharedKey);

  QVERIFY(decryptedWithWrongKey.contains("Не удалось расшифровать сообщение") ||
          decryptedWithWrongKey != originalMessage);

  QJsonObject corruptedPayload = encryptedPayload;
  QString badCipher = corruptedPayload["ciphertext"].toString();
  if (!badCipher.isEmpty()) {
    badCipher[0] = (badCipher[0] == 'A') ? 'B' : 'A';
    corruptedPayload["ciphertext"] = badCipher;
  }

  QString decryptedCorrupted =
      CryptoEngine::decryptMessage(corruptedPayload, aliceSharedKey);
  QVERIFY(decryptedCorrupted.contains("Не удалось расшифровать сообщение"));

  QByteArray brokenPrivateKey = QByteArray("short_key"); // меньше 32 байт
  QByteArray sharedKeyResult =
      CryptoEngine::FindSharedKey(brokenPrivateKey, bobKeys.first);

  QVERIFY(sharedKeyResult.isEmpty());
}
QTEST_GUILESS_MAIN(HseMessengerTest)
