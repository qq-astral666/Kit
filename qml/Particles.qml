import QtQuick

// Short-lived effects over the fox: hearts, "z", "!". Window coordinates.
Item {
    id: root

    property real size: 1

    function heart(x, y) {
        heartComponent.createObject(root, { x: x, y: y, drift: (Math.random() - 0.5) * 36 * size })
    }
    function zzz(x, y) {
        zComponent.createObject(root, { x: x, y: y })
    }
    function alert(x, y) {
        alertComponent.createObject(root, { x: x, y: y })
    }

    Component {
        id: heartComponent
        Item {
            id: h
            property real drift: 0
            property real rise: 0
            property real side: 0

            Blob {
                x: h.side
                y: h.rise
                scale: root.size * 1.3
                path: "M 0 4 C -7 -1 -7 -8 -2.6 -8 C -1 -8 0 -6.8 0 -5.6 C 0 -6.8 1 -8 2.6 -8 C 7 -8 7 -1 0 4 Z"
                fill: Colors.heart
            }
            ParallelAnimation {
                running: true
                NumberAnimation { target: h; property: "rise"; to: -62 * root.size; duration: 1150; easing.type: Easing.OutQuad }
                NumberAnimation { target: h; property: "side"; to: h.drift; duration: 1150; easing.type: Easing.InOutSine }
                SequentialAnimation {
                    NumberAnimation { target: h; property: "scale"; from: 0.2; to: 1.0; duration: 240; easing.type: Easing.OutBack }
                    PauseAnimation { duration: 520 }
                    NumberAnimation { target: h; property: "opacity"; to: 0; duration: 390 }
                }
                onFinished: h.destroy()
            }
        }
    }

    Component {
        id: zComponent
        Item {
            id: zz
            property real rise: 0
            Text {
                x: -width / 2 + zz.rise * -0.35
                y: zz.rise - height
                text: "z"
                font.pixelSize: 15 * root.size
                font.weight: Font.Black
                font.italic: true
                color: Colors.zzz
                style: Text.Outline
                styleColor: "white"
            }
            ParallelAnimation {
                running: true
                NumberAnimation { target: zz; property: "rise"; to: -44 * root.size; duration: 2200; easing.type: Easing.OutSine }
                NumberAnimation { target: zz; property: "scale"; from: 0.5; to: 1.4; duration: 2200 }
                SequentialAnimation {
                    PauseAnimation { duration: 1400 }
                    NumberAnimation { target: zz; property: "opacity"; to: 0; duration: 800 }
                }
                onFinished: zz.destroy()
            }
        }
    }

    Component {
        id: alertComponent
        Item {
            id: a
            Text {
                x: -width / 2
                y: -height - 6 * root.size
                text: "!"
                font.pixelSize: 24 * root.size
                font.weight: Font.Black
                color: Colors.alert
                style: Text.Outline
                styleColor: "white"
            }
            SequentialAnimation {
                running: true
                NumberAnimation { target: a; property: "scale"; from: 0; to: 1.25; duration: 140; easing.type: Easing.OutBack }
                NumberAnimation { target: a; property: "scale"; to: 1; duration: 90 }
                PauseAnimation { duration: 550 }
                NumberAnimation { target: a; property: "opacity"; to: 0; duration: 250 }
                ScriptAction { script: a.destroy() }
            }
        }
    }
}
