#include "Tray.h"

#include "Native.h"
#include "Pet.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

Tray::Tray(Pet* pet, QObject* parent)
    : QObject(parent)
    , m_pet(pet)
{
    m_menu.addAction(QStringLiteral("Call Kit Here"), m_pet, &Pet::summon);

    m_sizeMenu.setTitle(QStringLiteral("Size"));
    m_sizeGroup = new QActionGroup(this);
    const std::pair<const char*, Pet::Size> sizes[] = {
        { "Small", Pet::Small }, { "Medium", Pet::Medium }, { "Large", Pet::Large } };
    for (const auto& entry : sizes) {
        const Pet::Size size = entry.second;
        QAction* action = m_sizeMenu.addAction(QString::fromLatin1(entry.first));
        action->setCheckable(true);
        action->setData(int(size));
        m_sizeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, size] { m_pet->setSizeSetting(size); });
    }
    m_menu.addMenu(&m_sizeMenu);

    m_hideAction = m_menu.addAction(QStringLiteral("Hide Kit"));
    m_hideAction->setCheckable(true);
    connect(m_hideAction, &QAction::toggled, m_pet, &Pet::setHidden);

    m_dockAction = m_menu.addAction(QStringLiteral("Let Kit Walk on the Dock…"), this, [] {
        native::requestAccessibility();
    });

    m_menu.addSeparator();
    m_loginAction = m_menu.addAction(QStringLiteral("Launch at Login"));
    m_loginAction->setCheckable(true);
    connect(m_loginAction, &QAction::triggered, this, [this](bool on) {
        QString error;
        if (!native::setLaunchAtLogin(on, &error) && !error.isEmpty())
            m_tray.showMessage(QStringLiteral("Kit"), error, QSystemTrayIcon::Warning, 4000);
        sync();
    });

    m_menu.addSeparator();
    m_menu.addAction(QStringLiteral("Quit Kit"), qApp, &QCoreApplication::quit);

    connect(&m_menu, &QMenu::aboutToShow, this, &Tray::sync);
    connect(m_pet, &Pet::sizeChanged, this, &Tray::sync);

    m_tray.setIcon(makeIcon());
    m_tray.setToolTip(QStringLiteral("Kit"));
    m_tray.setContextMenu(&m_menu);
    sync();
    m_tray.show();
}

void Tray::sync()
{
    for (QAction* action : m_sizeGroup->actions())
        action->setChecked(action->data().toInt() == int(m_pet->sizeSetting()));
    const QSignalBlocker blockHide(m_hideAction);
    m_hideAction->setChecked(m_pet->hidden());
    m_loginAction->setChecked(native::launchAtLoginEnabled());
    m_dockAction->setVisible(!native::accessibilityTrusted());
}

QIcon Tray::makeIcon()
{
    // A fox face at 18 pt, rendered @2x, marked as a mask (template image) so
    // macOS tints it for light and dark menu bars.
    constexpr qreal dpr = 2.0;
    QPixmap pm(QSize(18, 18) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);

    QPainterPath face;
    face.moveTo(2.0, 1.5);
    face.lineTo(7.0, 6.2);
    face.lineTo(11.0, 6.2);
    face.lineTo(16.0, 1.5);
    face.lineTo(16.6, 8.5);
    face.lineTo(9.0, 16.5);
    face.lineTo(1.4, 8.5);
    face.closeSubpath();
    p.drawPath(face);

    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.drawEllipse(QPointF(6.0, 9.6), 1.15, 1.15);
    p.drawEllipse(QPointF(12.0, 9.6), 1.15, 1.15);
    p.end();

    QIcon icon(pm);
    icon.setIsMask(true);
    return icon;
}
