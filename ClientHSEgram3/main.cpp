/**
 * @file main.cpp
 * @brief Точка входа в приложение, инициализация ресурсов и криптографических
 * модулей.
 */
#include "mainwindow.h"
#include "sodium.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>

/**
 * @brief Главная точка входа в клиентское приложение.
 * * Функция выполняет базовую инициализацию инфраструктуры Qt, настраивает
 * элементы графического интерфейса (кастомный ретро-шрифт, QSS-стили), а также
 * производит обязательную инициализацию библиотеки libsodium перед запуском
 * основного цикла обработки событий.
 * * @param argc Количество аргументов командной строки.
 * @param argv Массив аргументов командной строки.
 * @return int Статус завершения приложения (0 в случае успешного выхода из
 * a.exec()).
 */
int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  MainWindow w;
  w.show();

  // Загрузка и настройка шрифта
  int fontId =
      QFontDatabase::addApplicationFont(":/fonts/PressStart2P-Regular.ttf");
  QString fontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);
  QFont pixelFont(fontFamily, 8);
  pixelFont.setStyleStrategy(QFont::NoAntialias);

  // Инициализация криптографической библиотеки
  if (sodium_init() < 0) {
    qDebug() << "libsodium init FAILED";
    return -1;
  }
  qDebug() << "libsodium version: " << sodium_version_string();

  // Загрузка и применение глобальных таблиц стилей (QSS)
  QFile f(":/styles/style.qss");
  if (!f.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "QSS open failed:" << f.errorString();
  } else {
    QString qss = QString::fromUtf8(f.readAll());
    a.setStyleSheet(qss);
    qDebug() << "QSS applied, size =" << qss.size();
  }
  return a.exec();
}
