import QtQuick
import QtQuick.Shapes
import Kit

// The fox rig. (0,0) is the feet; the drawing faces right in design units and
// is flipped/scaled by the transform. Every pose is a blend of three base
// poses (stand / sit / sleep) plus per-activity offsets and oscillations, so
// switching activities never pops.
Item {
    id: fox

    required property Pet pet

    readonly property int act: pet.activity
    readonly property real t: pet.clock
    readonly property bool rising: pet.velY < 0
    readonly property real progress: pet.actDuration > 0 ? Math.min(1, pet.actTime / pet.actDuration) : 0
    readonly property bool leaping: act === Pet.Jump || act === Pet.Pounce || act === Pet.Happy
                                    || act === Pet.Startled || act === Pet.Flip

    // ------------------------------------------------------------ blends
    property real sitBlend: act === Pet.Sit || act === Pet.Wag ? 1 : 0
    Behavior on sitBlend { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
    property real sleepBlend: act === Pet.Sleep ? 1 : 0
    Behavior on sleepBlend { NumberAnimation { duration: 480; easing.type: Easing.InOutCubic } }
    readonly property real standBlend: 1 - Math.max(sitBlend, sleepBlend)

    function mix3(stand, sit, sleep) {
        return stand + (sit - stand) * sitBlend + (sleep - stand) * sleepBlend
    }

    // Facing flips through zero, which reads as turning around.
    property real facingAnim: pet.facing
    Behavior on facingAnim { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }

    // -------------------------------------------------- activity targets
    readonly property var legTarget: {
        switch (act) {
        case Pet.Pounce:
            return rising ? [45, -75, 50, -70] : [30, -35, 40, -40]
        case Pet.Jump:
        case Pet.Happy:
        case Pet.Startled:
        case Pet.Flip:
            return rising ? [50, -60, 55, -55] : [30, -30, 40, -40]
        case Pet.Dragged:
            return [-70, -80, -75, -85]
        case Pet.Crouch:
            return [14, -12, 16, -14]
        case Pet.Wake:
            return pet.actTime < 0.9 ? [0, -38, 0, -42] : [0, 0, 0, 0]
        default:
            return [0, 0, 0, 0]
        }
    }
    property real leg0: legTarget[0]
    property real leg1: legTarget[1]
    property real leg2: legTarget[2]
    property real leg3: legTarget[3]
    Behavior on leg0 { NumberAnimation { duration: 140 } }
    Behavior on leg1 { NumberAnimation { duration: 140 } }
    Behavior on leg2 { NumberAnimation { duration: 140 } }
    Behavior on leg3 { NumberAnimation { duration: 140 } }

    property real walkAmp: act === Pet.Walk ? 28 : act === Pet.Run ? 44 : act === Pet.ChaseTail ? 32 : 0
    Behavior on walkAmp { NumberAnimation { duration: 160 } }
    readonly property real phase: act === Pet.ChaseTail ? t * 16 : pet.walkPhase
    readonly property real swingA: Math.sin(phase) * walkAmp
    readonly property real swingB: Math.sin(phase + Math.PI) * walkAmp
    readonly property real flail: act === Pet.Fall ? 38 : act === Pet.Dragged ? 9 : 0

    function legAngle(i, base) {
        const walk = (i === 0 || i === 3) ? swingB : swingA
        const kick = flail > 0 ? Math.sin(t * (act === Pet.Fall ? 26 : 9) + i * 1.7) * flail : 0
        return base + walk + kick
    }

    property real bodyTilt: {
        switch (act) {
        case Pet.Run: return -4
        case Pet.Crouch: return 7
        case Pet.Jump:
        case Pet.Happy:
        case Pet.Startled:
        case Pet.Flip: return rising ? -10 : 8
        case Pet.Pounce: return rising ? -16 : 10
        case Pet.Fall: return Math.max(-20, Math.min(25, pet.velY * 0.012))
        case Pet.Wake: return pet.actTime < 0.9 ? 12 : 0
        default: return 0
        }
    }
    Behavior on bodyTilt { NumberAnimation { duration: 160 } }

    readonly property real bodyWobble: {
        switch (act) {
        case Pet.Crouch: return Math.sin(t * 22) * 2
        case Pet.Wake: return pet.actTime > 0.9 && pet.actTime < 1.4 ? Math.sin(t * 42) * 5 : 0
        case Pet.Dizzy: return Math.sin(t * 5) * 4
        case Pet.ChaseTail: return Math.sin(t * 12) * 6
        default: return 0
        }
    }

    readonly property bool attentive: act === Pet.Idle || act === Pet.Sit || act === Pet.Walk
                                      || act === Pet.Crouch || act === Pet.Wag
    property real headTilt: {
        switch (act) {
        case Pet.Dragged: return -60
        case Pet.Petted: return -10
        case Pet.Happy: return -8
        case Pet.Wake: return pet.actTime < 0.9 ? 10 : 0
        case Pet.Pounce: return rising ? -8 : 6
        default: return 0
        }
    }
    Behavior on headTilt { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    readonly property real headMotion: {
        switch (act) {
        case Pet.LookAround: return Math.sin(t * 1.8) * 18
        case Pet.Petted: return Math.sin(t * 6) * 4
        case Pet.Dizzy: return Math.sin(t * 6) * 10
        case Pet.Sleep: return Math.sin(t * 1.6) * 1.5
        default: return attentive ? Math.max(-14, Math.min(12, pet.lookY * 14)) : 0
        }
    }

    property real tailTilt: {
        switch (act) {
        case Pet.Run: return -12
        case Pet.Walk: return -6
        case Pet.Crouch: return -18
        case Pet.Jump:
        case Pet.Pounce:
        case Pet.Happy:
        case Pet.Flip: return rising ? -25 : -35
        case Pet.Startled: return -30
        case Pet.Fall: return -35
        case Pet.Dragged: return -40 + pet.swing * 0.6 * pet.facing
        case Pet.Wake: return pet.actTime < 0.9 ? -30 : 0
        default: return 0
        }
    }
    Behavior on tailTilt { NumberAnimation { duration: 180 } }
    readonly property real tailMotion: {
        switch (act) {
        case Pet.Wag:
        case Pet.Petted: return Math.sin(t * 14) * 22
        case Pet.ChaseTail: return Math.sin(t * 16) * 25
        case Pet.Crouch: return Math.sin(t * 18) * 14
        case Pet.Walk: return Math.sin(phase) * 8
        case Pet.Run: return Math.sin(phase * 0.5) * 6
        case Pet.Sleep: return Math.sin(t * 0.9) * 2
        case Pet.Idle:
        case Pet.Sit:
        case Pet.LookAround: return Math.sin(t * 1.7) * 6
        default: return 0
        }
    }

    property real earTilt: {
        switch (act) {
        case Pet.Run: return -22
        case Pet.Jump:
        case Pet.Pounce:
        case Pet.Happy:
        case Pet.Flip: return -20
        case Pet.Fall: return -25
        case Pet.Dragged: return -15
        case Pet.Petted: return -12
        case Pet.Startled: return 12
        case Pet.Crouch: return 10
        default: return 0
        }
    }
    Behavior on earTilt { NumberAnimation { duration: 140 } }

    property bool blinking: false
    readonly property string eyeMode: {
        switch (act) {
        case Pet.Sleep: return "closed"
        case Pet.Wake: return pet.actTime < 0.9 ? "closed" : "open"
        case Pet.Happy:
        case Pet.Flip:
        case Pet.Wag:
        case Pet.Petted: return "happy"
        case Pet.Startled:
        case Pet.Fall:
        case Pet.Crouch:
        case Pet.Pounce: return "big"
        case Pet.Dragged: return pet.actTime < 0.8 ? "big" : "open"
        case Pet.Dizzy: return "dizzy"
        default: return "open"
        }
    }

    property real blushLevel: act === Pet.Happy || act === Pet.Wag || act === Pet.Petted
                              || act === Pet.Flip ? 0.95 : 0.45
    Behavior on blushLevel { NumberAnimation { duration: 250 } }

    property real puff: act === Pet.Startled ? 1.18 : 1
    Behavior on puff { SpringAnimation { spring: 5; damping: 0.25 } }

    property real dragRot: act === Pet.Dragged ? 75 - pet.swing * pet.facing : 0
    Behavior on dragRot { SpringAnimation { spring: 6; damping: 0.35 } }
    readonly property real flipRot: act === Pet.Flip ? -360 * progress : 0

    readonly property real breathe: act === Pet.Sleep ? Math.sin(t * 1.6) * 0.03
                                   : (act === Pet.Idle || act === Pet.Sit) ? Math.sin(t * 2.4) * 0.012 : 0
    readonly property real bob: act === Pet.Walk ? -Math.abs(Math.sin(phase)) * 1.5
                              : act === Pet.Run ? -Math.abs(Math.sin(phase)) * 3 : 0

    Timer {
        interval: 2600 + Math.random() * 3000
        running: true
        repeat: true
        onTriggered: {
            interval = 2600 + Math.random() * 3000
            fox.blinking = true
            blinkOff.restart()
        }
    }
    Timer {
        id: blinkOff
        interval: 120
        onTriggered: fox.blinking = false
    }

    // --------------------------------------------------------------- rig
    transform: Scale { xScale: fox.pet.size * fox.facingAnim; yScale: fox.pet.size }

    // Soft contact shadow, only when standing on something.
    Item {
        opacity: fox.pet.grounded ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
        transform: Scale { xScale: 1 / Math.sqrt(fox.pet.squash) * (fox.sleepBlend > 0.5 ? 1.15 : 1); yScale: 0.16 }
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: -1
                strokeColor: "transparent"
                fillGradient: RadialGradient {
                    centerX: 0; centerY: 0; centerRadius: 30
                    focalX: 0; focalY: 0
                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.26) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                PathSvg { path: "M -30 0 A 30 30 0 1 0 30 0 A 30 30 0 1 0 -30 0 Z" }
            }
        }
    }

    Item {
        id: squashed
        transform: Scale { xScale: 1 / Math.sqrt(fox.pet.squash); yScale: fox.pet.squash }

        // Backflip pivots around the middle of the body.
        Item {
            x: 0
            y: -32
            rotation: fox.flipRot
            transformOrigin: Item.TopLeft
            Item {
                x: 0
                y: 32

                // Dangling from the scruff.
                Item {
                    x: 20
                    y: -40
                    rotation: fox.dragRot
                    transformOrigin: Item.TopLeft
                    Item {
                        x: -20
                        y: 40

                        // Standing legs: far pair first, all behind the body.
                        Item {
                            opacity: fox.standBlend
                            visible: opacity > 0.01
                            Leg { x: -11; y: -14; far: true; angle: fox.legAngle(0, fox.leg0) }
                            Leg { x: 18; y: -14; far: true; angle: fox.legAngle(1, fox.leg1) }
                            Leg { x: -16; y: -14; angle: fox.legAngle(2, fox.leg2) }
                            Leg { x: 13; y: -14; angle: fox.legAngle(3, fox.leg3) }
                        }

                        Item {
                            id: bodyDrop
                            y: 7 * Math.max(fox.sitBlend, fox.sleepBlend) + fox.bob

                            Item {
                                x: -20
                                y: -9
                                rotation: fox.mix3(0, -35, 0) + fox.bodyTilt + fox.bodyWobble
                                transformOrigin: Item.TopLeft
                                Item {
                                    x: 20
                                    y: 9

                                    Item {
                                        x: fox.mix3(-20, -20, -26)
                                        y: fox.mix3(-24, -24, -9)
                                        z: fox.sleepBlend > 0.5 ? 3 : 0
                                        rotation: fox.mix3(0, 60, 105) + fox.tailTilt + fox.tailMotion
                                        transformOrigin: Item.TopLeft
                                        Tail { puff: fox.puff }
                                    }

                                    // Front legs while sitting: kept vertical, longer.
                                    Item {
                                        z: 1
                                        opacity: fox.sitBlend
                                        visible: opacity > 0.01
                                        Leg { x: 19; y: -14; far: true; stretch: 1.55; angle: 35 * fox.sitBlend - fox.bodyWobble }
                                        Leg { x: 14; y: -14; stretch: 1.55; angle: 35 * fox.sitBlend - fox.bodyWobble }
                                    }

                                    Item {
                                        z: 1
                                        transform: Scale { yScale: fox.mix3(1, 1, 0.78) * (1 + fox.breathe) }
                                        Blob {
                                            path: "M -24 -22 C -24 -33 -12 -37 2 -36 C 16 -35 25 -30 25 -21 C 25 -12 15 -8 0 -8 C -14 -8 -24 -12 -24 -22 Z"
                                            fill: Colors.fur
                                        }
                                        Blob {
                                            opacity: 1 - fox.sleepBlend
                                            path: "M -12 -10 C -4 -7 8 -7 16 -11 C 12 -15 2 -16 -4 -15 C -8 -14 -11 -12 -12 -10 Z"
                                            fill: Colors.cream
                                        }
                                        Blob {
                                            opacity: 1 - fox.sleepBlend
                                            path: "M 16 -30 C 22 -28 26 -22 24 -15 C 20 -14 17 -18 15 -24 Z"
                                            fill: Colors.cream
                                        }
                                    }

                                    Item {
                                        z: 2
                                        x: fox.mix3(16, 16, 12)
                                        y: fox.mix3(-30, -30, -7)
                                        rotation: fox.mix3(0, 32, 8) + fox.headTilt + fox.headMotion
                                        transformOrigin: Item.TopLeft
                                        Head {
                                            eyes: fox.blinking && (fox.eyeMode === "open" || fox.eyeMode === "big") ? "closed" : fox.eyeMode
                                            earRot: fox.earTilt
                                            lookX: fox.attentive ? fox.pet.lookX : 0
                                            lookY: fox.attentive ? fox.pet.lookY : 0
                                            blush: fox.blushLevel
                                        }
                                    }
                                }
                            }

                            // Haunch and hind paw while sitting (not rotated with the body).
                            Item {
                                opacity: fox.sitBlend
                                visible: opacity > 0.01
                                Blob {
                                    x: -2
                                    y: -7
                                    path: "M -14 -2 C -22 -4 -24 -14 -18 -20 C -12 -25 -2 -22 0 -14 C 2 -6 -4 -1 -14 -2 Z"
                                    fill: Colors.furDark
                                }
                                Blob {
                                    x: -4
                                    y: -7
                                    path: "M -8 0 L 6 0 C 8 0 8 -5 6 -5 L -6 -5 C -9 -5 -10 0 -8 0 Z"
                                    fill: Colors.sock
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
