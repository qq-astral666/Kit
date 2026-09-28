import QtQuick

// The head, pivoting at the neck (0,0), facing right.
Item {
    id: head

    // open | big | happy | closed | dizzy
    property string eyes: "open"
    property real earRot: 0
    property real lookX: 0
    property real lookY: 0
    property real blush: 0.45

    // Far ear, behind the head.
    Item {
        x: 4
        y: -30
        rotation: head.earRot
        transformOrigin: Item.TopLeft
        Blob {
            x: -4
            y: 30
            path: "M -2 -27 L -3 -45 C -3 -47 -1 -48 0 -46 L 9 -31 Z"
            fill: Colors.furDark
        }
    }

    Blob {
        path: "M -6 -14 C -8 -26 0 -34 12 -34 C 22 -34 28 -28 30 -22 L 40 -16 C 42 -14 41 -11 38 -10 C 30 -6 20 -4 12 -4 C 2 -4 -5 -8 -6 -14 Z"
        fill: Colors.fur
    }
    Blob {
        path: "M 12 -4 C 20 -4 30 -6 38 -10 C 41 -11 42 -13 40 -15 C 34 -12 26 -12 20 -14 C 16 -15 12 -16 8 -14 C 4 -12 4 -6 12 -4 Z"
        fill: Colors.cream
    }
    Blob {
        path: "M -6 -12 C -6 -8 -2 -5 4 -4 C 2 -7 1 -10 2 -13 Z"
        fill: Colors.cream
    }

    // Near ear.
    Item {
        x: 12
        y: -32
        rotation: head.earRot
        transformOrigin: Item.TopLeft
        Blob {
            x: -12
            y: 32
            path: "M 6 -30 L 9 -48 C 10 -50 12 -50 13 -48 L 20 -32 Z"
            fill: Colors.fur
        }
        Blob {
            x: -12
            y: 32
            path: "M 8.5 -33 L 10.5 -44.5 L 16 -33.5 Z"
            fill: Colors.earInner
        }
    }

    // Eyes
    Item {
        x: head.lookX * 1.4
        y: head.lookY * 1.2
        visible: head.eyes === "open"
        Blob {
            path: "M 19.3 -22 A 2.7 3.5 0 1 0 24.7 -22 A 2.7 3.5 0 1 0 19.3 -22 Z"
            fill: Colors.ink
        }
        Blob {
            path: "M 22 -23.4 A 1 1 0 1 0 24 -23.4 A 1 1 0 1 0 22 -23.4 Z"
            fill: "white"
        }
    }
    Item {
        x: head.lookX * 1.2
        y: head.lookY * 1.0
        visible: head.eyes === "big"
        Blob {
            path: "M 18.6 -22 A 3.4 4.4 0 1 0 25.4 -22 A 3.4 4.4 0 1 0 18.6 -22 Z"
            fill: Colors.ink
        }
        Blob {
            path: "M 21.8 -23.8 A 1.4 1.4 0 1 0 24.6 -23.8 A 1.4 1.4 0 1 0 21.8 -23.8 Z"
            fill: "white"
        }
    }
    Stroke {
        visible: head.eyes === "happy"
        path: "M 19 -21 Q 22 -25.5 25 -21"
        color: Colors.ink
        lineWidth: 1.8
    }
    Stroke {
        visible: head.eyes === "closed"
        path: "M 19 -21 Q 22 -18 25 -21"
        color: Colors.ink
        lineWidth: 1.6
    }
    Stroke {
        visible: head.eyes === "dizzy"
        path: "M 19.5 -24.5 L 24.5 -19.5 M 24.5 -24.5 L 19.5 -19.5"
        color: Colors.ink
        lineWidth: 1.6
    }

    Blob {
        path: "M 21.6 -13 A 3.4 2 0 1 0 28.4 -13 A 3.4 2 0 1 0 21.6 -13 Z"
        fill: Colors.blush
        opacity: head.blush
    }
    Blob {
        path: "M 37.8 -14 A 2.4 2 0 1 0 42.6 -14 A 2.4 2 0 1 0 37.8 -14 Z"
        fill: Colors.ink
    }
}
