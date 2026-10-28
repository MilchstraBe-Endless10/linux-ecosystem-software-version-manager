import QtQuick
import QtQuick.Effects

// Qt 6.8 visual-only energy core. No shaders/downloads/host actions.
// running=false freezes all motion for reduced-motion, idle and screenshot modes.
Item {
    id: root
    property bool running: false
    property bool compact: false
    property color accent: "#a05bff"

    implicitWidth: 200
    implicitHeight: 190

    Rectangle {
        id: auraOuter
        anchors.centerIn: parent
        width: parent.width * 0.89
        height: parent.height * 0.89
        radius: width / 2
        color: "#196bd5ff"
        border.color: "#9fe1fff6"
        border.width: 1
        opacity: 0.18
        SequentialAnimation on opacity {
            running: root.running
            loops: Animation.Infinite
            NumberAnimation { from: 0.12; to: 0.28; duration: 2600; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.28; to: 0.12; duration: 2600; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        id: auraInner
        anchors.centerIn: parent
        width: parent.width * 0.68
        height: parent.height * 0.68
        radius: width / 2
        color: "#3251edff"
        border.color: "#cda6ff"
        opacity: 0.24
    }

    // The orbit is a flat ellipse and intentionally has no mouse handlers.
    Rectangle {
        id: orbitA
        anchors.centerIn: parent
        width: parent.width * 1.02
        height: parent.height * 0.39
        radius: height / 2
        rotation: -24
        color: "transparent"
        border.color: "#b9edffff"
        border.width: 2
        opacity: 0.69
        SequentialAnimation on rotation {
            running: root.running
            loops: Animation.Infinite
            NumberAnimation { from: -24; to: 336; duration: 30000; easing.type: Easing.Linear }
        }
    }
    Rectangle {
        id: orbitB
        anchors.centerIn: parent
        width: parent.width * 0.38
        height: parent.height * 0.98
        radius: width / 2
        rotation: 18
        color: "transparent"
        border.color: "#b696ff"
        border.width: 1
        opacity: 0.57
        SequentialAnimation on rotation {
            running: root.running
            loops: Animation.Infinite
            NumberAnimation { from: 18; to: -342; duration: 37000; easing.type: Easing.Linear }
        }
    }

    Rectangle {
        id: prism
        anchors.centerIn: parent
        width: root.compact ? 80 : 108
        height: width
        radius: 17
        rotation: 45
        z: 2
        gradient: Gradient {
            GradientStop { position: 0; color: "#dc9bffff" }
            GradientStop { position: 0.48; color: "#9651f6" }
            GradientStop { position: 1; color: "#25319c" }
        }
        border.color: "#effbff"
        border.width: 2
        SequentialAnimation on scale {
            running: root.running
            loops: Animation.Infinite
            NumberAnimation { from: 0.98; to: 1.045; duration: 2700; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.045; to: 0.98; duration: 2700; easing.type: Easing.InOutSine }
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.70
            height: width
            radius: 9
            color: "#5d55d8"
            border.color: "#d9deff"
            border.width: 1
        }
    }

    // MultiEffect belongs to Qt 6 QtQuick.Effects, not the obsolete Qt 5
    // QtGraphicalEffects module. Limit the effect region to the jewel.
    MultiEffect {
        anchors.fill: prism
        source: prism
        z: 1
        shadowEnabled: true
        shadowColor: "#bd51faff"
        shadowBlur: 1.0
        blurMax: 32
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        opacity: 0.90
    }

    Text {
        anchors.centerIn: parent
        z: 4
        text: "⌘"
        font.pixelSize: root.compact ? 31 : 43
        font.bold: true
        color: "#f8ffff"
        style: Text.Outline
        styleColor: "#7060ee"
    }
}
