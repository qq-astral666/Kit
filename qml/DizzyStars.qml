import QtQuick

// Three stars circling above the head after a hard landing.
Item {
    id: root

    property real clock: 0
    property real size: 1

    Repeater {
        model: 3
        Text {
            required property int index
            readonly property real angle: root.clock * 4 + index * 2 * Math.PI / 3
            x: Math.cos(angle) * 16 * root.size - width / 2
            y: Math.sin(angle) * 5 * root.size - height / 2
            scale: 0.8 + 0.25 * Math.sin(angle)
            z: Math.sin(angle)
            text: "★"
            font.pixelSize: 11 * root.size
            color: Colors.star
            style: Text.Outline
            styleColor: "#8A5A00"
        }
    }
}
