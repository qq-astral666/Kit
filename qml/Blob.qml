import QtQuick
import QtQuick.Shapes

// A filled SVG path. All fox parts are drawn with these: no image files.
Shape {
    id: root

    property alias path: svg.path
    property color fill: "black"

    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        fillColor: root.fill
        strokeWidth: -1
        strokeColor: "transparent"
        PathSvg { id: svg }
    }
}
