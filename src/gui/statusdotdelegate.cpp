#include "statusdotdelegate.hpp"
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>

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

    // --- dot color
    QColor color = rawStatus.startsWith("running")       ? QColor("green")
                : rawStatus == "stopped"                ? QColor("red")
                : rawStatus == "unconfigured"           ? QColor("gray")
                : rawStatus.contains("starting")        ? QColor("orange")
                : rawStatus.contains("stopping")        ? QColor("orange")
                : QColor("orange");

    int d = qMin(o.rect.height(), 6);

    // --- left dummy dot
    QRect leftDot(o.rect.left() + 6, o.rect.center().y() - d/2, d, d);

    // --- right real dot
    QRect rightDot(o.rect.right() - d - 6, o.rect.center().y() - d/2, d, d);

    // --- draw dummy dot
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(QColor("gray")); // decorative
    p->setPen(Qt::NoPen);
    p->drawEllipse(leftDot);
    p->restore();

    // --- draw real status dot
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(color);
    p->setPen(Qt::NoPen);
    p->drawEllipse(rightDot);
    p->restore();

    // --- text rect in between
    int left = leftDot.right() + 6;
    int right = rightDot.left() - 6;
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
    int minWidth = fm.horizontalAdvance(QString(16, 'M')); // space for 24 chars
    int height = qMax(24, option.rect.height());

    return QSize(minWidth, height); // Qt will treat this as *minimum*, not fixed
}

