#include <QCoreApplication>
#include "hseserver.h"

/**
 * @brief Главная точка входа в консольное серверное приложение.
 * * Создает неграфическое событийно-ориентированное ядро `QCoreApplication`,
 * инициализирует объект сервера `HseServer` и запускает его на прослушивание портов.
 * @param argc Количество аргументов командной строки.
 * @param argv Массив строк-аргументов командной строки.
 * @return int Код завершения выполнения приложения при закрытии цикла событий (`a.exec()`).
 */
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    HseServer server;
    server.startServer();

    return a.exec();
}
