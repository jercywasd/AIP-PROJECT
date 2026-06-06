#ifndef CTEST_H
#define CTEST_H

#include <QtTest>

class HseMessengerTest : public QObject {
  Q_OBJECT

private slots:
  void testKeyPairGeneration();
  void testE2EEncryptionDecryption();
  void testNegativeScenarios();
};

#endif // CTEST_H
