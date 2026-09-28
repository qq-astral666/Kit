import QtQuick
import QtQuick.Shapes

// A stroked, unfilled SVG path (closed eyes, dizzy eyes).
Shape {
    id: root

    property alias path: svg.path
    property color color: "black"
    property real lineWidth: 1.6

    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        fillColor: "transparent"
        strokeColor: root.color
        strokeWidth: root.lineWidth
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathSvg { id: svg }
    }
}
