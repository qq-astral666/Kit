import QtQuick

// Bushy tail attached at (0,0) (the rump). `puff` fluffs it up.
Item {
    id: tail

    property real puff: 1

    transform: Scale { xScale: tail.puff; yScale: tail.puff }

    Blob {
        path: "M 0 0 C -8 4 -22 6 -34 -2 C -44 -9 -48 -22 -42 -30 C -38 -35 -30 -32 -26 -24 C -21 -15 -13 -8 0 -8 Z"
        fill: Colors.fur
    }
    Blob {
        path: "M -42 -30 C -38 -35 -30 -32 -27 -25 C -32 -24 -38 -22 -45 -18 C -47 -23 -45 -27 -42 -30 Z"
        fill: Colors.cream
    }
}
