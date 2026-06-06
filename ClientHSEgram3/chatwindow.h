#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include "cryptoengine.h"
#include "sodium.h"
#include <QColor>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMainWindow>
#include <QPainter>
#include <QPair>
#include <QSoundEffect>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QString>
#include <QStylePainter>
#include <QStyledItemDelegate>
#include <QWidget>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
namespace Ui {
class ChatWindow;
}
/**
 * @class ChatWindow
 * @brief Класс главного окна чата клиентского приложения.
 * * Обеспечивает логику интерфейса, шифрование сообщений «из конца в конец»
 * (E2E), работу с локальной БД и сетевое взаимодействие.
 */
class ChatWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit ChatWindow(QTcpSocket *nsocket, QString login,
                      QWidget *parent = nullptr);
  ~ChatWindow();
  QTcpSocket *socket = nullptr;
  QJsonParseError error;
  QStandardItemModel *dialogsModel = nullptr;
  QStandardItemModel *messageModel = nullptr;
  QString myLogin;
  QSoundEffect *notifySound = nullptr;
  QVector<int> dialogs;
  int myUserId;
  int CurrentDialogId;
  QByteArray myPrivateKey;
  QByteArray myPublicKey;
  QSqlDatabase db;
public slots:
  void sockReady();
  void sockDisc();
private slots:
  QByteArray GetPublicKey(int userID, QString user_login, bool flag = 0,
                          const QByteArray &public_key = QByteArray());
  bool CreateKey(QSqlDatabase &db);
  void playNotice();
  void markDialogUnread(int dialog_id);
  void on_sendmes_clicked();
  void addMessage(const QString &text, bool outgoing);
  void handleOpenDialogResult(QString login, int dialog_id);
  void on_searchDialog_clicked();
  void onClickedDialog(const QModelIndex &index);

private:
  Ui::ChatWindow *ui;
};
// START: AI-DESIGN-GENERATION-CLAUDE-SONNET-4.6
class BubbleMessageDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  static const int PAD_H = 12;
  static const int PAD_V = 8;
  static const int MARGIN = 14;
  // Фиксированная максимальная ширина пузыря в пикселях
  // НЕ зависит от option.rect.width() который может быть 0
  static const int FIXED_MAX_BUBBLE_W = 400;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::TextAntialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    bool isMe = index.data(Qt::UserRole + 5).toBool();
    QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
      painter->restore();
      return;
    }

    // ── Шрифт ────────────────────────────────────────────────
    QFont font = option.font;
    font.setFamily("Press Start 2P");
    font.setPixelSize(9);
    QFontMetrics fm(font);

    // ── Геометрия ─────────────────────────────────────────────
    // Реальная доступная ширина — из option.rect
    // Если rect пустой — используем fallback
    int availW = (option.rect.width() > 10) ? option.rect.width() : 600;

    int maxBubbleW = qMin(static_cast<int>(availW * 0.62), FIXED_MAX_BUBBLE_W);

    QRect textBound = fm.boundingRect(QRect(0, 0, maxBubbleW - PAD_H * 2, 0),
                                      Qt::TextWordWrap | Qt::AlignLeft, text);

    int bw = qMin(textBound.width() + PAD_H * 2, maxBubbleW);
    int bh = textBound.height() + PAD_V * 2;
    bw = qMax(bw, 80);
    bh = qMax(bh, 30);

    int yTop = option.rect.top() + 5;
    int xBox =
        isMe ? option.rect.right() - bw - MARGIN : option.rect.left() + MARGIN;

    // Защита от выхода за границы
    if (xBox < option.rect.left())
      xBox = option.rect.left() + MARGIN;

    QRect box(xBox, yTop, bw, bh);

    // ── Тень ─────────────────────────────────────────────────
    painter->fillRect(box.adjusted(3, 3, 3, 3), QColor(0, 0, 0, 160));

    // ── Заливка ───────────────────────────────────────────────
    QColor fill = isMe ? QColor("#1a3a08") : QColor("#0f0f0a");
    QColor inner = isMe ? QColor("#1e3d0a") : QColor("#111108");

    painter->fillRect(box, fill);
    painter->fillRect(box.adjusted(2, 2, -2, -2), inner);

    // ── Рамка (пиксельная, 2px) ───────────────────────────────
    QColor hi = isMe ? QColor("#aaff44") : QColor("#2a4a12");
    QColor lo = isMe ? QColor("#3a6010") : QColor("#0a0f04");

    // top
    painter->fillRect(box.left(), box.top(), box.width(), 2, hi);
    // left
    painter->fillRect(box.left(), box.top(), 2, box.height(), hi);
    // bottom
    painter->fillRect(box.left(), box.bottom() - 1, box.width(), 2, lo);
    // right
    painter->fillRect(box.right() - 1, box.top(), 2, box.height(), lo);

    // ── Хвостик (пиксельные квадраты 4x4) ────────────────────
    const int ts = 4;
    if (isMe) {
      painter->fillRect(box.right() + 1, box.bottom() - ts, ts, ts, hi);
      painter->fillRect(box.right() + ts + 1, box.bottom() - ts * 2, ts, ts,
                        hi);
    } else {
      painter->fillRect(box.left() - ts - 1, box.bottom() - ts, ts, ts, hi);
      painter->fillRect(box.left() - ts * 2 - 1, box.bottom() - ts * 2, ts, ts,
                        hi);
    }

    // ── Префикс ───────────────────────────────────────────────
    {
      QFont pf;
      pf.setFamily("Courier New");
      pf.setPixelSize(11);
      pf.setBold(true);
      painter->setFont(pf);
      painter->setPen(isMe ? QColor("#aaff44") : QColor("#556622"));
      QString prefix = isMe ? ">" : "<";
      if (isMe) {
        painter->drawText(QRect(box.left() - 14, yTop, 12, 16),
                          Qt::AlignRight | Qt::AlignTop, prefix);
      } else {
        painter->drawText(QRect(box.right() + 2, yTop, 12, 16),
                          Qt::AlignLeft | Qt::AlignTop, prefix);
      }
    }

    // ── Текст ─────────────────────────────────────────────────
    QRect textRect = box.adjusted(PAD_H, PAD_V, -PAD_H, -PAD_V);
    painter->setFont(font);
    painter->setPen(isMe ? QColor("#ccff88") : QColor("#99aa77"));
    painter->drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                      text);

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty())
      return QSize(option.rect.width(), 40);

    QFont font = option.font;
    font.setFamily("Press Start 2P");
    font.setPixelSize(9);
    QFontMetrics fm(font);

    // Ключевой фикс: не используем option.rect.width() если он 0
    int availW = (option.rect.width() > 10) ? option.rect.width() : 600;

    int maxBubbleW = qMin(static_cast<int>(availW * 0.62), FIXED_MAX_BUBBLE_W);

    QRect b = fm.boundingRect(QRect(0, 0, maxBubbleW - PAD_H * 2, 0),
                              Qt::TextWordWrap | Qt::AlignLeft, text);

    int h = b.height() + PAD_V * 2 + 10 + 10;
    return QSize(availW, qMax(h, 44));
  }
};
// ::END
#endif // CHATWINDOW_H
