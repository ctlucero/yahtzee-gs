// The Qt layer: shows the App's 320x200 IIgs screen scaled to the window,
// with the IIgs's slightly tall pixels (4:3 overall), and forwards the
// mouse, keyboard and a 60 Hz tick. All game logic lives in src/screen and
// src/core.
#include "../screen/app.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QWidget>

class GsView : public QWidget {
public:
    GsView()
        : image_(gs::Canvas::W, gs::Canvas::H, QImage::Format_RGB32)
    {
        setWindowTitle(QStringLiteral("Yahtzee"));
        setMinimumSize(320, 240);
        resize(960, 720);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        clock_.start();
        connect(&timer_, &QTimer::timeout, this, [this] { onTimer(); });
        timer_.setTimerType(Qt::PreciseTimer);
        timer_.start(5);

        // Game > Options... is remembered between runs.
        QSettings st;
        gs::Options o;
        o.yahtzeeBonus = st.value(QStringLiteral("options/yahtzeeBonus"), false).toBool();
        o.modernRoll = st.value(QStringLiteral("options/modernRoll"), false).toBool();
        app_.setOptions(o);
        app_.onOptionsChanged = [](const gs::Options& opt) {
            QSettings st;
            st.setValue(QStringLiteral("options/yahtzeeBonus"), opt.yahtzeeBonus);
            st.setValue(QStringLiteral("options/modernRoll"), opt.modernRoll);
        };
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (app_.dirty()) {
            const gs::Canvas& cv = app_.canvas();
            for (int y = 0; y < gs::Canvas::H; ++y) {
                auto* line = reinterpret_cast<QRgb*>(image_.scanLine(y));
                for (int x = 0; x < gs::Canvas::W; ++x) line[x] = 0xFF000000u | cv.rgb(x, y);
            }
            app_.clearDirty();
        }
        QPainter p(this);
        p.fillRect(rect(), Qt::black);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(target(), image_);
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const QPoint s = toScreen(e->position().toPoint());
        app_.mouseDown(s.x(), s.y());
        refresh();
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPoint s = toScreen(e->position().toPoint());
        app_.mouseMove(s.x(), s.y());
        refresh();
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const QPoint s = toScreen(e->position().toPoint());
        app_.mouseUp(s.x(), s.y());
        refresh();
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        // The Apple (Command) key maps to Ctrl, or the Meta/Alt keys.
        const bool cmd = e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier);
        if (cmd) {
            const QString t = QKeySequence(e->key()).toString();
            if (t.size() == 1) app_.key(t.at(0).toLatin1(), true);
        } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            app_.key('\r', false);
        } else if (e->key() == Qt::Key_Backspace) {
            app_.key(8, false);
        } else if (e->key() == Qt::Key_Left) {
            app_.key(0x1C, false);
        } else if (e->key() == Qt::Key_Right) {
            app_.key(0x1D, false);
        } else {
            const QString t = e->text();
            if (t.size() == 1 && t.at(0).unicode() >= 32 && t.at(0).unicode() < 127)
                app_.key(char(t.at(0).unicode()), false);
        }
        refresh();
    }

private:
    QRect target() const
    {
        // Keep the 4:3 shape of the IIgs display.
        int w = width(), h = height();
        if (w * 3 > h * 4) w = h * 4 / 3;
        else h = w * 3 / 4;
        return QRect((width() - w) / 2, (height() - h) / 2, w, h);
    }

    QPoint toScreen(QPoint p) const
    {
        const QRect t = target();
        int x = (p.x() - t.x()) * gs::Canvas::W / qMax(1, t.width());
        int y = (p.y() - t.y()) * gs::Canvas::H / qMax(1, t.height());
        return {qBound(0, x, gs::Canvas::W - 1), qBound(0, y, gs::Canvas::H - 1)};
    }

    void onTimer()
    {
        // Run exactly 60 ticks per second regardless of timer jitter.
        const qint64 due = clock_.elapsed() * 60 / 1000;
        while (ticks_ < due) { app_.tick(); ++ticks_; }
        // The original hides the pointer while the computer plays.
        setCursor(app_.busy() ? Qt::BlankCursor : Qt::ArrowCursor);
        if (app_.quitRequested()) close();
        refresh();
    }

    void refresh() { if (app_.dirty()) update(); }

    gs::App app_;
    QImage image_;
    QTimer timer_;
    QElapsedTimer clock_;
    qint64 ticks_ = 0;
};

int main(int argc, char** argv)
{
    QApplication qapp(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("yahtzee-gs"));
    QApplication::setApplicationName(QStringLiteral("Yahtzee"));
    QApplication::setDesktopFileName(QStringLiteral("io.github.yahtzee_gs.Yahtzee"));
    GsView view;
    view.show();
    return qapp.exec();
}
