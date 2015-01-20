#include "textlabel.h"

TextLabel::TextLabel(QWidget *parent):
    QLabel(parent),
    _elide(Qt::ElideNone)
{

}

TextLabel::~TextLabel()
{

}
Qt::TextElideMode TextLabel::elide() const
{
    return _elide;
}

void TextLabel::setElide(const Qt::TextElideMode &elide)
{
    _elide = elide;
    emit elideChanged(_elide);
}

void TextLabel::setText(const QString &text)
{
    _origText = text;
    QLabel::setText(elideText(text));
}

void TextLabel::clear()
{
    _origText.clear();
    QLabel::clear();
}

void TextLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::setText(elideText(_origText));

    if(event)
        event->ignore();
}

QString TextLabel::elideText(const QString &text)
{
    QFontMetrics metrics(this->font());
    return metrics.elidedText(text, _elide, this->width());
}

