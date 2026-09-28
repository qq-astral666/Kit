#pragma once

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class Pet;
class QAction;
class QActionGroup;

// Menu bar icon: summon, size, hide, launch at login, quit.
class Tray : public QObject {
    Q_OBJECT

public:
    explicit Tray(Pet* pet, QObject* parent = nullptr);

private:
    static QIcon makeIcon();
    void sync();

    Pet* m_pet;
    QSystemTrayIcon m_tray;
    QMenu m_menu;
    QMenu m_sizeMenu;
    QActionGroup* m_sizeGroup = nullptr;
    QAction* m_hideAction = nullptr;
    QAction* m_loginAction = nullptr;
    QAction* m_dockAction = nullptr;
};
