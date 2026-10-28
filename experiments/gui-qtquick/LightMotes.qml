import QtQuick
import QtQuick.Particles

// GPU-rendered light motes. No network, no external petal images.
// The built-in Qt particle resource renders as a small glow point.
// For final V3.0 floral fidelity, replace the texture with a LICENSED
// isolated petal sprite; never reuse a crop of the flattened UI image.
Item {
    id: root
    property bool running: false
    property int density: 7

    ParticleSystem {
        id: particles
        anchors.fill: parent
        running: root.running
    }

    ImageParticle {
        system: particles
        source: "qrc:///particleresources/glowdot.png"
        color: "#ffd7f4"
        colorVariation: 0.15
        alpha: 0.55
        rotationVariation: 180
        entryEffect: ImageParticle.Fade
    }

    Emitter {
        system: particles
        x: 0
        y: 0
        width: root.width
        height: 3
        emitRate: root.running ? root.density : 0
        lifeSpan: 7600
        lifeSpanVariation: 1600
        size: 10
        sizeVariation: 4
        velocity: AngleDirection {
            angle: 75
            angleVariation: 34
            magnitude: 41
            magnitudeVariation: 18
        }
        acceleration: PointDirection { x: -3; y: 8 }
    }
}
