#include "statusdotdelegate.hpp"
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>

void
StatusDotDelegate::paint(QPainter* p,
                              const QStyleOptionViewItem& opt,
                              const QModelIndex& idx) const
{
    QStyleOptionViewItem o(opt);
    initStyleOption(&o, idx);

    // --- background only
    QStyle* style = o.widget ? o.widget->style() : QApplication::style();
    QStyleOptionViewItem oBg(o);
    oBg.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &oBg, p, o.widget);

    QString rawStatus   = idx.data(Qt::UserRole).toString().toLower();
    QString displayText = idx.data(Qt::DisplayRole).toString();

    // --- dot color from palette
    const QPalette &pal = o.palette;

    QColor color =
      rawStatus.startsWith("running")       ? pal.color(QPalette::Highlight)     // active/accent
    : rawStatus == "stopped"                ? pal.color(QPalette::Inactive, QPalette::Button)    // strong contrast, often red on KDE themes
    : rawStatus == "unconfigured"           ? pal.color(QPalette::Mid)           // neutral grayish
    : pal.color(QPalette::WindowText);                                          // fallback


    // two characters wide padding
    QFontMetrics fm(o.font);
    int char_padding = fm.horizontalAdvance(QString(1, 'M')); // 2 chars wide

    int dot_diameter = 8;

    // --- real dot area
    int d = qMin(o.rect.height(), dot_diameter);

    // --- left dummy dot
    QRect leftDot(o.rect.left() + char_padding,
              o.rect.center().y() - d/2,
              d, d);

    // --- right real dot
    QRect rightDot(o.rect.right() - char_padding - d,
               o.rect.center().y() - d/2,
               d, d);

    // --- draw left dot (clone of status dot)
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(color);
    p->setPen(Qt::NoPen);
    p->drawEllipse(leftDot);
    p->restore();

    // --- draw right dot (clone of status dot)
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(color);
    p->setPen(Qt::NoPen);
    p->drawEllipse(rightDot);
    p->restore();

    // --- text rect in between
    int left = leftDot.right() + dot_diameter;
    int right = rightDot.left() - dot_diameter;
    QRect textRect(left, o.rect.top(), right - left, o.rect.height());

    // --- draw text
    p->save();
    p->setPen(o.palette.color(QPalette::Text));
    p->drawText(textRect, Qt::AlignVCenter | Qt::AlignHCenter, displayText);
    p->restore();
}


QSize
StatusDotDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &idx) const
{
    Q_UNUSED(idx);

    QFontMetrics fm(option.font);
    int minWidth = fm.horizontalAdvance(QString(18, 'M')); // space for 24 chars
    int height = qMax(24, option.rect.height());

    return QSize(minWidth, height); // Qt will treat this as *minimum*, not fixed
}

