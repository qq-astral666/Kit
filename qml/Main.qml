import QtQuick
import Kit

// A small transparent window that follows the fox around the desktop.
// Pet (C++) moves it and makes it click-through except over the fox.
Window {
    id: win

    required property Pet pet

    readonly property real k: pet.size
    readonly property point head: Qt.point(pet.anchorX + pet.facing * 30 * k,
                                           pet.anchorY - (pet.activity === Pet.Sleep ? 34 : 64) * k)

    width: pet.windowWidth
    height: pet.windowHeight
    visible: false            // shown by Pet::attach once positioned
    color: "transparent"
    title: "Kit"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | Qt.NoDropShadowWindowHint | Qt.WindowDoesNotAcceptFocus

    FrameAnimation {
        running: true
        onTriggered: win.pet.tick(frameTime)
    }

    // Only tells C++ that a press really reached this window; dragging
    // itself is done by polling the cursor.
    MouseArea {
        anchors.fill: parent
        onPressed: (mouse) => {
            win.pet.notePress()
            mouse.accepted = false
        }
    }

    Fox {
        pet: win.pet
        x: win.pet.anchorX
        y: win.pet.anchorY
    }

    DizzyStars {
        visible: win.pet.activity === Pet.Dizzy
        x: win.head.x
        y: win.head.y - 6 * win.k
        clock: win.pet.clock
        size: win.k
    }

    Particles {
        id: particles
        anchors.fill: parent
        size: win.k
    }

    Connections {
        target: win.pet
        function onActivityChanged() {
            switch (win.pet.activity) {
            case Pet.Happy:
                for (let i = 0; i < 3; ++i)
                    particles.heart(win.head.x + (i - 1) * 12 * win.k, win.head.y)
                break
            case Pet.Wag:
                particles.heart(win.head.x, win.head.y)
                break
            case Pet.Startled:
                particles.alert(win.head.x, win.head.y)
                break
            }
        }
    }

    Timer {
        running: win.pet.activity === Pet.Petted
        interval: 340
        repeat: true
        triggeredOnStart: true
        onTriggered: particles.heart(win.head.x, win.head.y)
    }

    Timer {
        running: win.pet.activity === Pet.Sleep
        interval: 1500
        repeat: true
        onTriggered: particles.zzz(win.head.x + 6 * win.k * win.pet.facing, win.head.y)
    }
}
