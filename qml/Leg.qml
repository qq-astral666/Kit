import QtQuick

// One leg hanging from its hip at (0,0). `angle` swings it (degrees,
// positive = foot moves backwards), `stretch` lengthens it.
Item {
    id: leg

    property real angle: 0
    property real stretch: 1
    property bool far: false

    rotation: angle
    transformOrigin: Item.TopLeft

    Item {
        transform: Scale { yScale: leg.stretch }

        Blob {
            path: "M -4 14 L -4 1 C -4 -2 4 -2 4 1 L 4 14 C 4 15.5 -4 15.5 -4 14 Z"
            fill: leg.far ? Colors.furFar : Colors.fur
        }
        Blob {
            path: "M -4 9 L 4 9 L 4 14 C 4 15.8 -4 15.8 -4 14 Z"
            fill: Colors.sock
        }
    }
}
