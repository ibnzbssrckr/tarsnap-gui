#ifndef BUSYWIDGET_H
#define BUSYWIDGET_H

#include <QLabel>
#include <QMovie>
#include <QMouseEvent>

class BusyWidget : public QLabel
{
    Q_OBJECT

public:
    BusyWidget(QWidget *parent = NULL);
    ~BusyWidget();

public slots:
    void animate();
    void stop();

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event);

private:
    QMovie  _animation;
};

#endif // BUSYWIDGET_H
