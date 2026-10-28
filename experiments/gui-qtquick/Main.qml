import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1672
    height: 941
    minimumWidth: 1000
    minimumHeight: 640
    // The C++ entry point sets the requested size before mapping this window.
    visible: false
    title: "Linux 全生态软件版本管理中心 · Qt Quick PoC"
    color: "#eaf0ff"

    property bool compactNav: width < 1320
    property bool reducedMotion: appReducedMotion
    property bool compareMode: appCompareMode
    property bool stressEnabled: appStressMode
    property bool stressScrollRunning: stressScrollAnimation.running
    property int selectedIndex: 0
    property int selectedInstanceIndex: 0
    property var selectedRecord: softwareModel.recordAt(0)
    property bool sceneRunning: visible
                               && visibility !== Window.Minimized
                               && Qt.application.state === Qt.ApplicationActive
                               && !reducedMotion

    readonly property color ink: "#17265f"
    readonly property color mutedInk: "#65749b"
    readonly property color indigo: "#5852ef"
    readonly property color violet: "#8c5bff"
    readonly property color cyan: "#72ddf8"
    readonly property color mint: "#16b77e"
    readonly property color line: "#cad5f1"
    readonly property int navWidth: compactNav ? 76 : 260
    readonly property int tableHeight: height < 800 ? 220 : 286

    function setSelected(index) {
        selectedIndex = index
        selectedRecord = softwareModel.recordAt(index)
        selectedInstanceIndex = 0
    }

    Rectangle {
        anchors.fill: parent
        color: "#dfeaff"
    }

    Image {
        id: city
        anchors.fill: parent
        anchors.leftMargin: root.navWidth
        source: assetRoot + "09_secondary_concepts/scene_floating_city_wide.png"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        opacity: 0.98
        x: root.sceneRunning ? drift : 0
        property real drift: -8
        SequentialAnimation on drift {
            running: root.sceneRunning
            loops: Animation.Infinite
            NumberAnimation { from: -8; to: 8; duration: 12000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 8; to: -8; duration: 12000; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        anchors.fill: city
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#12f7f9ff" }
            GradientStop { position: 0.55; color: "#16eef4ff" }
            GradientStop { position: 1.0; color: "#20ebf2ff" }
        }
    }

    Image {
        id: heroine
        source: assetRoot + "09_secondary_concepts/character_anime_halfbody.png"
        x: root.navWidth + (root.width - root.navWidth) * 0.26
        y: root.height * 0.15 + bob
        width: root.width < 1320 ? 300 : 378
        height: root.height < 800 ? 210 : 258
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: true
        opacity: 0.76
        sourceClipRect: Qt.rect(12, 4, 270, 173)
        property real bob: 0
        SequentialAnimation on bob {
            running: root.sceneRunning
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: -5; duration: 5200; easing.type: Easing.InOutSine }
            NumberAnimation { from: -5; to: 5; duration: 10400; easing.type: Easing.InOutSine }
            NumberAnimation { from: 5; to: 0; duration: 5200; easing.type: Easing.InOutSine }
        }
    }

    Repeater {
        model: 18
        delegate: Item {
            required property int index
            width: 14
            height: 18
            x: root.navWidth + 30 + ((index * 137) % Math.max(160, root.width - root.navWidth - 70))
            y: -20 - (index % 5) * 34
            z: 2
            Rectangle {
                anchors.centerIn: parent
                width: index % 3 === 0 ? 10 : 7
                height: index % 4 === 0 ? 14 : 10
                radius: 8
                rotation: index * 31
                color: index % 2 === 0 ? "#f7b7e7" : "#ffe9f4"
                opacity: 0.68
            }
            NumberAnimation on y {
                running: root.sceneRunning
                loops: Animation.Infinite
                from: -24 - (index % 5) * 34
                to: root.height + 30
                duration: 9000 + (index % 7) * 1100
                easing.type: Easing.Linear
            }
            NumberAnimation on x {
                running: root.sceneRunning
                loops: Animation.Infinite
                from: root.navWidth + 30 + ((index * 137) % Math.max(160, root.width - root.navWidth - 70))
                to: root.navWidth + 54 + ((index * 137) % Math.max(160, root.width - root.navWidth - 70))
                duration: 7000 + (index % 4) * 900
                easing.type: Easing.InOutSine
            }
        }
    }

    Rectangle {
        id: sidebar
        x: 0
        y: 0
        z: 8
        width: root.navWidth
        height: root.height
        color: "#eaf5fbf8"
        border.color: "#aabfe5"
        border.width: 1
        Behavior on width { NumberAnimation { duration: root.reducedMotion ? 0 : 220; easing.type: Easing.OutCubic } }

        Column {
            anchors.fill: parent
            anchors.margins: root.compactNav ? 8 : 16
            spacing: 10

            Row {
                width: parent.width
                height: 78
                spacing: 12
                Rectangle {
                    width: 46; height: 46; radius: 15
                    anchors.verticalCenter: parent.verticalCenter
                    gradient: Gradient {
                        GradientStop { position: 0; color: root.indigo }
                        GradientStop { position: 1; color: root.violet }
                    }
                    Text { anchors.centerIn: parent; text: "✦"; color: "white"; font.pixelSize: 25; font.bold: true }
                }
                Column {
                    visible: !root.compactNav
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: "Linux 全生态"; color: root.ink; font.pixelSize: 17; font.bold: true }
                    Text { text: "软件版本管理中心"; color: root.ink; font.pixelSize: 14; font.bold: true }
                }
            }

            Rectangle { width: parent.width; height: 1; color: "#9eafd2"; opacity: 0.5 }

            Repeater {
                model: [
                    { label: "工作台", icon: "⌂", count: "" },
                    { label: "软件清单", icon: "▦", count: "10k" },
                    { label: "AI Agent / CLI", icon: "◇", count: "86" },
                    { label: "更新任务", icon: "◷", count: "12" },
                    { label: "来源与适配器", icon: "⬡", count: "9" },
                    { label: "系统洞察", icon: "♧", count: "" },
                    { label: "故障恢复", icon: "⛨", count: "" },
                    { label: "设置与偏好", icon: "⚙", count: "" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: root.height < 800 ? 44 : 53
                    radius: 16
                    color: index === 0 ? "#dce4ffff" : "transparent"
                    border.color: index === 0 ? "#a3a6f3ff" : "transparent"
                    Behavior on color { ColorAnimation { duration: root.reducedMotion ? 0 : 200 } }
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: root.compactNav ? 0 : 14
                        anchors.rightMargin: 10
                        spacing: 12
                        Text { width: root.compactNav ? parent.width : 30; anchors.verticalCenter: parent.verticalCenter; horizontalAlignment: Text.AlignHCenter; text: modelData.icon; color: index === 0 ? root.indigo : root.ink; font.pixelSize: 23 }
                        Text { visible: !root.compactNav; anchors.verticalCenter: parent.verticalCenter; width: Math.max(80, parent.width - 72); text: modelData.label; color: root.ink; font.pixelSize: 14; elide: Text.ElideRight }
                        Text { visible: !root.compactNav && modelData.count !== ""; anchors.verticalCenter: parent.verticalCenter; text: modelData.count; color: root.mutedInk; font.pixelSize: 12 }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.setSelected(Math.min(index, softwareModel.recordCount - 1)) }
                }
            }

            Item { height: Math.max(0, root.height - (root.height < 800 ? 675 : 800)) }

            Rectangle {
                width: parent.width
                height: root.compactNav ? 98 : 154
                radius: 19
                color: "#dff3f8ff"
                border.color: root.line
                Column {
                    anchors.fill: parent
                    anchors.margins: root.compactNav ? 8 : 14
                    spacing: 7
                    Row {
                        width: parent.width
                        Text { text: "♥"; color: root.mint; font.pixelSize: 14 }
                        Text { visible: !root.compactNav; text: "系统健康"; color: root.ink; font.pixelSize: 13; font.bold: true; leftPadding: 5 }
                        Item { width: root.compactNav ? 0 : parent.width - 115; height: 1 }
                        Text { text: "92%"; color: root.ink; font.pixelSize: 12; font.bold: true }
                    }
                    Rectangle { width: parent.width; height: 5; radius: 3; color: "#c5d3ed"; Rectangle { width: parent.width * 0.92; height: parent.height; radius: 3; color: root.mint } }
                    Text { visible: !root.compactNav; text: "● 同期连接　　 正常\n● 存储空间　　 正常\n● 最后检查　　 2 分钟前"; color: root.mutedInk; font.pixelSize: 11; lineHeight: 1.45 }
                    Row {
                        visible: !root.compactNav
                        spacing: 6
                        CheckBox { id: motionToggle; checked: root.reducedMotion; text: "减少动态"; font.pixelSize: 11; onToggled: root.reducedMotion = checked }
                    }
                }
            }
        }
    }

    Row {
        id: topBar
        x: root.navWidth + 22
        y: 14
        z: 7
        width: root.width - root.navWidth - 44
        height: 56
        spacing: 14

        Rectangle {
            width: Math.min(440, parent.width * 0.34)
            height: parent.height
            radius: 18
            color: "#e7f9fbfb"
            border.color: "#a9bee5"
            Row {
                anchors.fill: parent
                anchors.leftMargin: 15
                spacing: 10
                Text { text: "⌕"; anchors.verticalCenter: parent.verticalCenter; color: root.mutedInk; font.pixelSize: 26 }
                TextField {
                    width: parent.width - 65
                    anchors.verticalCenter: parent.verticalCenter
                    placeholderText: "搜索软件、包名、命令、描述…"
                    color: root.ink
                    placeholderTextColor: root.mutedInk
                    background: Item {}
                    font.pixelSize: 13
                }
            }
        }

        Item { width: Math.max(0, parent.width - Math.min(440, parent.width * 0.34) - 620); height: 1 }

        Rectangle {
            width: Math.min(600, parent.width - Math.min(440, parent.width * 0.34) - 20)
            height: parent.height
            radius: 18
            color: "#dcf9fbff"
            border.color: "#afc3e7"
            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0
                Repeater {
                    model: [
                        { icon: "⬡", title: "系统健康", value: "良好", color: "#16b77e" },
                        { icon: "⇩", title: "待更新", value: "286", color: "#5852ef" },
                        { icon: "⚠", title: "风险项", value: "2", color: "#f05153" },
                        { icon: "✿", title: "运行中", value: "3", color: "#3684d8" }
                    ]
                    delegate: Row {
                        required property var modelData
                        width: parent.width / 4
                        height: parent.height
                        spacing: 7
                        Text { text: modelData.icon; anchors.verticalCenter: parent.verticalCenter; color: modelData.color; font.pixelSize: 19 }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                    Text { text: modelData.title; color: root.mutedInk; font.pixelSize: 12 }
                    Text { text: modelData.value; color: modelData.color; font.pixelSize: 15; font.bold: true }
                        }
                        Rectangle { visible: index < 3; x: parent.width - 1; y: 6; width: 1; height: parent.height - 12; color: root.line }
                    }
                }
            }
        }
    }

    Text {
        x: root.navWidth + 28
        y: 91
        z: 4
        text: "版本中枢"
        color: root.ink
        font.pixelSize: root.height < 800 ? 36 : 43
        font.bold: true
    }
    Text {
        x: root.navWidth + 30
        y: 145
        z: 4
        text: "连接 Linux 全生态 · 掌控每一个软件的版本轨迹"
        color: root.ink
        font.pixelSize: 14
    }

    Row {
        x: root.navWidth + 28
        y: 182
        z: 4
        spacing: 0
        Repeater {
            model: [
                { value: "10,000", label: "模拟软件记录" },
                { value: "286", label: "可更新" },
                { value: "12", label: "待处理任务" },
                { value: "9", label: "来源适配器" }
            ]
            delegate: Row {
                required property var modelData
                width: 116
                height: 52
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: modelData.value; color: root.ink; font.pixelSize: 21; font.bold: true }
                    Text { text: modelData.label; color: root.mutedInk; font.pixelSize: 12 }
                }
                Rectangle { x: parent.width - 1; y: 4; width: 1; height: 40; color: root.line }
            }
        }
    }

    Item {
        id: orbitalStage
        x: root.navWidth + 24
        y: 245
        z: 3
        width: Math.max(340, root.width - root.navWidth - 48)
        height: root.height - root.tableHeight - y - 23

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.68, 720)
            height: Math.min(parent.height * 0.78, 340)
            radius: height / 2
            color: "transparent"
            border.color: "#78a8eaf3"
            border.width: 2
            rotation: -13
            opacity: 0.75
        }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.55, 590)
            height: Math.min(parent.height * 0.55, 230)
            radius: height / 2
            color: "transparent"
            border.color: "#7dafa2ff"
            border.width: 1
            rotation: 12
            opacity: 0.7
        }

        Repeater {
            model: [
                { label: "pacman", sub: "356 个包", icon: "A", color: "#3089ff", fx: 0.37, fy: 0.54 },
                { label: "AI Agent / CLI", sub: "86 个软件", icon: ">_", color: "#5f67ff", fx: 0.68, fy: 0.34 },
                { label: "Flatpak", sub: "124 个应用", icon: "⬡", color: "#e3a93c", fx: 0.22, fy: 0.76 },
                { label: "mise", sub: "52 个工具", icon: "m", color: "#9657ff", fx: 0.83, fy: 0.68 },
                { label: "系统工具", sub: "210 个组件", icon: "⚙", color: "#2995ff", fx: 0.66, fy: 0.85 },
                { label: "uv / pipx", sub: "86 个包", icon: "Py", color: "#5599e8", fx: 0.47, fy: 0.9 }
            ]
            delegate: Item {
                required property var modelData
                required property int index
                x: Math.max(0, orbitalStage.width * modelData.fx - width / 2)
                y: orbitalStage.height * modelData.fy - height / 2
                width: 172
                height: 68
                Rectangle {
                    x: -11; y: -7; width: 80; height: 80; radius: 40
                    color: modelData.color
                    opacity: 0.09
                    scale: halo.scale
                }
                Rectangle {
                    id: halo
                    x: 0; y: 4; width: 58; height: 58; radius: 29
                    color: modelData.color
                    opacity: 0.13
                    scale: 1.45
                    SequentialAnimation on scale {
                        running: root.sceneRunning
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.6; duration: 2300; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.35; duration: 2300; easing.type: Easing.InOutSine }
                    }
                }
                Rectangle {
                    x: 5; y: 9; width: 48; height: 48; radius: 24
                    border.color: "#d8faff"
                    border.width: 2
                    gradient: Gradient {
                        GradientStop { position: 0; color: modelData.color }
                        GradientStop { position: 1; color: Qt.darker(modelData.color, 1.25) }
                    }
                    Text { anchors.centerIn: parent; text: modelData.icon; color: "white"; font.pixelSize: modelData.icon.length > 2 ? 13 : 19; font.bold: true }
                }
                Rectangle {
                    x: 50; y: 15; width: 118; height: 44; radius: 14
                    color: "#dff8fbff"
                    border.color: "#bdd0ef"
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        Text { text: modelData.label; color: root.ink; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; width: 96 }
                        Text { text: modelData.sub; color: root.mutedInk; font.pixelSize: 11 }
                    }
                }
                NumberAnimation on y {
                    running: root.sceneRunning
                    loops: Animation.Infinite
                    from: orbitalStage.height * modelData.fy - height / 2 - 4
                    to: orbitalStage.height * modelData.fy - height / 2 + 4
                    duration: 2800 + index * 310
                    easing.type: Easing.InOutSine
                }
            }
        }
    }

    Rectangle {
        id: detailPanel
        z: 9
        x: root.width - width - 22
        y: 91
        width: Math.min(490, root.width * 0.38)
        height: Math.min(510, root.height - root.tableHeight - 132)
        radius: 25
        color: "#fff9fbff"
        border.color: "#b5c5e8"
        border.width: 1
        Behavior on x { NumberAnimation { duration: root.reducedMotion ? 0 : 320; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: root.reducedMotion ? 0 : 250 } }

        Column {
            anchors.fill: parent
            anchors.margins: root.height < 800 ? 12 : 18
            spacing: root.height < 800 ? 6 : 8

            Row {
                width: parent.width
                height: root.height < 800 ? 58 : 68
                spacing: 13
                Rectangle {
                    width: 60; height: 60; radius: 18
                    color: "#ff713c"
                    Text { anchors.centerIn: parent; text: root.selectedRecord.glyph || "✳"; color: "white"; font.pixelSize: 27; font.bold: true }
                }
                Column {
                    width: parent.width - 110
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: root.selectedRecord.displayName || "Claude Code"; color: root.ink; font.pixelSize: 19; font.bold: true; elide: Text.ElideRight; width: parent.width }
                    Text { text: root.selectedRecord.category || "AI Agent / CLI"; color: root.mutedInk; font.pixelSize: 11 }
                }
                Text { text: "⋯"; color: root.mutedInk; font.pixelSize: 22 }
            }

            Row {
                width: parent.width
                height: root.height < 800 ? 28 : 34
                spacing: 2
                Repeater {
                    model: ["概览", "安装实例", "来源与信任", "更新计划", "历史与诊断"]
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        width: parent.width / 5
                        height: parent.height
                        color: "transparent"
                        Text { anchors.centerIn: parent; text: modelData; color: index === 1 ? root.indigo : root.mutedInk; font.pixelSize: 12; font.bold: index === 1 }
                        Rectangle { visible: index === 1; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: 40; height: 3; radius: 2; color: root.indigo }
                    }
                }
            }
            Rectangle { width: parent.width; height: 1; color: root.line }

            Row {
                width: parent.width
                Text { width: Math.max(120, parent.width - 132); text: root.compareMode ? "DET3 / CMP3 · 实例比较" : "安装实例 · 多来源"; color: root.ink; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight }
                Rectangle {
                    width: 120; height: 28; radius: 14
                    color: root.compareMode ? "#dad8ff" : "#eef1ff"
                    Text { anchors.centerIn: parent; text: root.compareMode ? "返回实例列表" : "比较所选实例"; color: root.indigo; font.pixelSize: 12; font.bold: true }
                    MouseArea { anchors.fill: parent; onClicked: root.compareMode = !root.compareMode }
                }
            }

            Column {
                width: parent.width
                spacing: 8
                Repeater {
                    model: root.compareMode
                           ? [
                               { source: "npm 全局", version: root.selectedRecord.currentVersion || "1.0.28", scope: "用户级 · PATH 命中", color: "#ed4b4b" },
                               { source: "mise 项目", version: root.selectedRecord.latestVersion || "1.0.21", scope: "项目级 · 需手动确认", color: "#8853eb" },
                               { source: "官方包（模拟）", version: root.selectedRecord.latestVersion || "1.0.32", scope: "系统级 · 不执行更新", color: "#2994bd" }
                           ]
                           : [
                               { source: "npm 全局", version: root.selectedRecord.currentVersion || "1.0.28", scope: "用户级 · PATH 命中", color: "#ed4b4b" },
                               { source: "mise（复用）", version: root.selectedRecord.latestVersion || "1.0.32", scope: "项目级 · 需手动确认", color: "#8853eb" },
                               { source: "官方包（手动）", version: root.selectedRecord.currentVersion || "1.0.32", scope: "系统级 · 只读演示", color: "#2994bd" }
                           ]
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: root.height < 800 ? 48 : 60
                        radius: 14
                        color: index === root.selectedInstanceIndex ? "#edf1ff" : "#fffdfdff"
                        border.color: "#d4def4"
                        Row {
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 11
                            Rectangle {
                                width: 32; height: 32; radius: 10
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.color
                                Text { anchors.centerIn: parent; text: modelData.source.substring(0, 1); color: "white"; font.pixelSize: 15; font.bold: true }
                            }
                            Column {
                                width: parent.width - 155
                                anchors.verticalCenter: parent.verticalCenter
                                Text { text: modelData.source; color: root.ink; font.pixelSize: 12; font.bold: true }
                                Text { text: modelData.scope; color: root.mutedInk; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                Text { text: modelData.version; color: root.ink; font.pixelSize: 12; font.bold: true }
                                Text { text: "当前实例"; color: root.mutedInk; font.pixelSize: 11 }
                            }
                            Text { text: index === root.selectedInstanceIndex ? "●" : "○"; anchors.verticalCenter: parent.verticalCenter; color: index === root.selectedInstanceIndex ? root.indigo : root.mutedInk; font.pixelSize: 17 }
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.selectedInstanceIndex = index }
                    }
                }
            }

            Rectangle {
                width: parent.width; height: root.height < 800 ? 28 : 35; radius: 12
                color: "#e8eaff"
                Text { anchors.centerIn: parent; text: root.compareMode ? "差异优先 · 版本 / 来源 / 作用域" : "查看完整比较矩阵（12 项字段）　›"; color: root.indigo; font.pixelSize: 11; font.bold: true }
            }
            Item { width: 1; height: 1 }
            Text { visible: root.height >= 800; text: "演示数据 · 不连接包管理器，不执行更新"; color: root.mutedInk; font.pixelSize: 11 }
        }
    }

    Rectangle {
        id: workbenchCard
        x: orbitalStage.x + 8
        y: orbitalStage.y + Math.max(8, orbitalStage.height * 0.28)
        z: 5
        width: 164
        height: 44
        radius: 14
        color: "#eaf9fbff"
        border.color: "#b9c8ec"
        Row {
            anchors.centerIn: parent
            spacing: 8
            Text { text: "⠿"; color: root.indigo; font.pixelSize: 18 }
            Text { text: "WB3 · 拖动布局"; color: root.ink; font.pixelSize: 12; font.bold: true }
        }
        DragHandler {
            target: workbenchCard
            xAxis.minimum: root.navWidth
            xAxis.maximum: root.width - workbenchCard.width
            yAxis.minimum: 75
            yAxis.maximum: root.height - root.tableHeight - workbenchCard.height
            onActiveChanged: {
                if (!active) {
                    workbenchCard.x = Math.round(workbenchCard.x / 16) * 16
                    workbenchCard.y = Math.round(workbenchCard.y / 16) * 16
                }
            }
        }
        Behavior on x { NumberAnimation { duration: root.reducedMotion ? 0 : 180; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: root.reducedMotion ? 0 : 180; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: softwareTable
        x: root.navWidth + 16
        y: root.height - height - 14
        z: 7
        width: root.width - root.navWidth - 32
        height: root.tableHeight
        radius: 22
        color: "#fff9fbff"
        border.color: "#bac9e9"

        Column {
            anchors.fill: parent
            anchors.margins: 13
            spacing: 6
            Row {
                width: parent.width
                height: 34
                spacing: 7
                Repeater {
                    model: ["全部软件　10k", "可更新　286", "AI Agent　86", "开发工具　320", "运行时　152"]
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        width: index === 0 ? 115 : 98
                        height: 28
                        radius: 14
                        color: index === 0 ? "#e3e3ff" : "#f5f7ff"
                        border.color: index === 0 ? "#b9b7ff" : "#d7dff3"
                        Text { anchors.centerIn: parent; text: modelData; color: index === 0 ? root.indigo : root.mutedInk; font.pixelSize: 12; font.bold: index === 0 }
                    }
                }
                Item { width: Math.max(0, parent.width - 610); height: 1 }
                Text { text: "10,000 条模拟记录"; color: root.mutedInk; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 11 }
                Rectangle { width: 32; height: 28; radius: 8; color: "#e6eaff"; Text { anchors.centerIn: parent; text: "☷"; color: root.indigo; font.pixelSize: 17 } }
                Rectangle { width: 74; height: 28; radius: 9; color: "#e6eaff"; Text { anchors.centerIn: parent; text: "⚙ 列设置"; color: root.indigo; font.pixelSize: 11 } }
            }

            Row {
                width: parent.width
                height: 25
                spacing: 0
                Repeater {
                    model: [
                        { label: "软件名称", factor: 0.19 },
                        { label: "当前版本", factor: 0.12 },
                        { label: "最新版本", factor: 0.12 },
                        { label: "来源 / 管理器", factor: 0.15 },
                        { label: "安装实例", factor: 0.12 },
                        { label: "更新状态", factor: 0.12 },
                        { label: "风险", factor: 0.08 },
                        { label: "操作", factor: 0.10 }
                    ]
                    delegate: Text {
                        required property var modelData
                        width: parent.width * modelData.factor
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.label
                        color: root.mutedInk
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            ListView {
                id: softwareList
                width: parent.width
                height: Math.max(80, parent.height - 65)
                clip: true
                model: softwareModel
                reuseItems: true
                cacheBuffer: height * 2
                spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    required property int index
                    required property string displayName
                    required property string category
                    required property string currentVersion
                    required property string latestVersion
                    required property string source
                    required property string instances
                    required property string status
                    required property string risk
                    required property string glyph
                    width: softwareList.width
                    height: root.height < 800 ? 31 : 37
                    radius: 10
                    color: index === root.selectedIndex ? "#e3eaff" : "#f9fbffff"
                    border.color: index === root.selectedIndex ? "#b1c0fa" : "#e4e9f7"

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 8
                        spacing: 0
                        Row {
                            width: parent.width * 0.19
                            height: parent.height
                            spacing: 7
                            Rectangle {
                                width: 24; height: 24; radius: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: ["#ff713c", "#252b35", "#278f62", "#568ee8", "#f34c3d", "#418ed6", "#5866e9", "#e4b23e"][index % 8]
                                Text { anchors.centerIn: parent; text: glyph; color: "white"; font.pixelSize: glyph.length > 2 ? 10 : 13; font.bold: true }
                            }
                            Column {
                                width: parent.width - 37
                                anchors.verticalCenter: parent.verticalCenter
                                Text { text: displayName; color: root.ink; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; width: parent.width }
                                Text { text: category; color: root.mutedInk; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                            }
                        }
                        Text { width: parent.width * 0.12; height: parent.height; verticalAlignment: Text.AlignVCenter; text: currentVersion; color: root.ink; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { width: parent.width * 0.12; height: parent.height; verticalAlignment: Text.AlignVCenter; text: latestVersion; color: index % 5 === 0 ? root.mint : "#f16b47"; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight }
                        Text { width: parent.width * 0.15; height: parent.height; verticalAlignment: Text.AlignVCenter; text: source; color: root.ink; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { width: parent.width * 0.12; height: parent.height; verticalAlignment: Text.AlignVCenter; text: instances; color: root.ink; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { width: parent.width * 0.12; height: parent.height; verticalAlignment: Text.AlignVCenter; text: status; color: root.mint; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight }
                        Text { width: parent.width * 0.08; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "✦ " + risk; color: risk === "中" ? "#f19130" : root.mint; font.pixelSize: 12 }
                        Text { width: parent.width * 0.10; height: parent.height; verticalAlignment: Text.AlignVCenter; text: "模拟"; color: root.indigo; font.pixelSize: 12; font.bold: true }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.setSelected(index) }
                }
            }
        }
    }

    Timer {
        id: stressStartTimer
        interval: 2200
        running: root.stressEnabled && root.sceneRunning
        onTriggered: stressScrollAnimation.start()
    }

    NumberAnimation {
        id: stressScrollAnimation
        target: softwareList
        property: "contentY"
        from: 0
        to: Math.max(0, softwareList.contentHeight - softwareList.height)
        duration: 10000
        loops: Animation.Infinite
        running: false
        easing.type: Easing.Linear
    }

}
