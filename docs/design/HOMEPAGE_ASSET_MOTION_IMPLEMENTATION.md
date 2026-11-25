# V3.0 工作台首页：美术资产制作、图形渲染与原生 QML 实施指令

状态：开发执行指令，2026-11-25。**本文件是当前唯一首页视觉开发入口，优先于旧交接中的 Agent/CLI 优先顺序。** 暂停其他七页视觉改造，保留已有生产功能。以下技术方案、时长、尺寸是实现建议与起始参数，不是新增 PRD 产品决策。实际以本地 PRD V3.5、AGENTS.md、批准的 V3.0 首页及当前生产代码为准。批准首页原图 1672×941，SHA-256 5361c9da2dbeb159fb935979f7e5ba280dedb2f60d1a8c008f9505556e952ba9，不得修改。原图及未审查许可的资产不在公开仓库分发。

## 0. 任务与真实边界

你是运行在项目本机的 Codex。直接制作素材、编写制作脚本、修改正式 QML、构建渲染并修复实际差距，不只列软件名单或再次输出计划。先读 UI_ASSET_BASELINE.md、退役清单、ACTIVE_WORK.md、已有资源目录与 CMake/QRC。不要假定文件存在或远端旧代码等于本地最新版本。远端 feature/gui-fx-native-001 的 MagicCore.qml 实际是旋转圆角矩形加 MultiEffect；LightMotes.qml 使用 glowdot 粒子，并非最终立体核心和花瓣。旧 PoC 只作技术参考，不能覆盖生产代码。

## 1. 工具链：先探测，再选实现

| 软件/库 | 制作职责 | 可交付源文件和运行资源 |
|---|---|---|
| Krita | 原画分层、人物与猫咪蒙版、发丝边缘、手工修补被遮挡背景、局部调色 | .kra；RGBA PNG |
| Blender | 中央能量立方体、玻璃球、透视、灯光、发光材质、透明背景渲染 | .blend + bpy 制作脚本；透明 PNG 或局部帧序列 |
| Inkscape | 导航图标、轨道和标志的矢量制作、统一 viewBox 与描边 | SVG 源文件；Qt 兼容 SVG/PNG |
| Python + Pillow（可选 OpenCV） | alpha 审计、裁切边界、SHA、合成预览、帧序列图集和差异热图 | 可重跑脚本、清单、接触表 |
| Qt Quick / QtQuick.Effects | 真实布局、玻璃面板、局部阴影和高光、交互 | QML |
| QtQuick.Particles / Qt Shader Tools | 花瓣粒子、能量轨迹、高光扫描等局部 GPU 效果 | QML、GLSL、.qsb |
| Qt Creator / QML Profiler（若已有） | 定位绑定、绘制、帧时间与事件问题 | 本地性能记录 |

Figma/Penpot 可做布局标注，不是本轮必需；不要用网页、Electron 或全屏视频替代 Qt 应用。推荐优先使用 Blender 离线渲染 + Qt Quick 原生动画，避免为了一个核心引入整套 3D 运行库。若需要真正实时三维旋转，再评估 Qt Quick 3D 的依赖、许可与性能，并取得项目决定，不能自动加入。Blender、Krita、Inkscape 不作为终端用户运行依赖。

实际只读探测：command -v blender krita inkscape python3 cmake qtpaths6 qsb qmllint；检查已存在 Qt 安装中的 bin/libexec、模块及 Qt 版本（命令未在 PATH 不等于未安装）；检查 Python Pillow、OpenCV 是否可导入。Arch 上用本机 pacman -Q 查询已装包即可，不联网。可能涉及 blender、krita、inkscape、qt6-declarative、qt6-shadertools、qt6-svg、qt6-tools，实际包名及版本以本机证据为准。不要执行 sudo、pacman -Sy、部分升级、自动下载模型权重或依赖。缺工具时列出具体缺项、可替代流程和安装请求；不能用假素材冒充完成，也不能因此停止其他可做工作。

## 2. 美术资产：先做真实分层，不是整屏截图

沿现有目录管理源文件和运行导出物；如尚无等价结构，可在本地资产目录内划分 sources/、scene/、fx/、icons/、manifests/。退役资产不恢复生产引用。不移动或覆盖批准原图。以下文件名为建议，已有等价素材直接复用。

| 层 | 建议独立输出 | 制作方法 |
|---|---|---|
| 远景 | sky-far.png | Krita 保留天空、远城、远云；去除前景和 UI |
| 中景 | city-mid.png | 城市、桥梁、瀑布，必要时补绘被人物遮挡区域 |
| 前景 | garden-front.png | 花卉、栏杆及近景；独立遮挡关系 |
| 人物 | heroine.png | 原有高质量人物原画，Krita 透明蒙版；不得重塑原批准角色或姿态 |
| 猫咪 | cat.png | 只从正确且许可允许的已有独立原画修复蒙版；退役错误猫图禁用 |
| 粒子 | petal-01.png 等 / glow.png | 原创或许可清楚的粉色花瓣、微光 sprite |
| 核心 | core-body.png、core-emission.png | Blender 透明背景的局部高精渲染 |
| 环绕节点 | orb-base.png、orb-glow.png、SVG 图标 | Blender 渲染通用球体/平台，Qt 动态叠加对应图标和文字 |
| 遮罩/光照 | edge-highlight.png、局部光晕 | 单独纹理，不烘焙数据或按钮 |

Krita 操作流程：打开已有原始可编辑资产 → 保留原层 → 建立透明蒙版 → 手工修正发丝、毛发边缘和反光 → 在黑、白、蓝紫三种底色检查 Alpha → 输出 sRGB RGBA PNG。若只有扁平原图，被 UI 或其他物体遮挡的像素无法无损恢复：优先找原始生成素材或分层源文件，再做受控补绘；必要时单独请求图像编辑/美术补绘，不把虚构补绘称为原画像素复原。自动抠图工具只产生初始蒙版；模型与权重许可独立审查，不默认联网。人物、猫咪、背景不能从整屏图裁出带 UI 的矩形使用。工作台节点、文字、表格、详情必须真正可交互。

用 Pillow 记录尺寸、实际 alpha min/max、透明像素比例、非透明包围盒、边缘半透明分布、SHA-256。RGBA 并不等于真的透明。检查像素边缘是否有白底、黑边、粉色溢出、重复角色或比例错误。导出尺寸按实际显示范围与 DPR 决定，不盲目放大低分辨率。Alpha 合成避免重复预乘。保存源文件、导出脚本、相对路径、锚点、裁切与缩放参数、来源及许可；以黑白底预览和分层合成预览验收。具体场景坐标从批准图量取，先对齐再改色。

## 3. Blender：实际制作发光三维核心，不再用旋转矩形

在已有 Blender 可用时，用 bpy 编写可重跑的 make_energy_core.py，在临时输出目录生成 .blend 和 RGBA 渲染。可以通过 blender --background --python <脚本> -- <输出目录> 执行；脚本自行解析参数、保存工程与执行 render(write_still=True)。只运行本地审查过的制作脚本，不执行下载的任意脚本。

制作工序：
1. 创建真正的三维立方体网格，添加 Bevel（起始宽度约边长 5–8%，4–8 segments）及平滑法线；采用三面可见的固定透视/正交相机，以批准图实际角度为准，不是二维矩形旋转 45°。
2. 外层为蓝紫半透明玻璃/晶体材质，内层为深蓝能量面；建立独立青色、紫色发光边缘（曲线或网格）和内层光源。终端符号用自制矢量或曲线作为面上图案，保证实际朝向和可读性，不使用旧 PoC 的 ⌘ 替代批准图终端符号。
3. 布置青、紫、白区域光，调整材质粗糙度、反射与发光，避免变成纯白饱和块。根据已装 Blender 版本选择 Eevee/Cycles；开启 Film Transparent，输出 PNG RGBA，保留不透明主体和独立光晕。可在 compositor 使用 Glare/Fog Glow；检查透明边缘，单独输出光效以便 QML 合成。以 512 或 768 方形渲染为初值，根据目标实际大小与 DPR 验证，不固定为 PRD。
4. 先输出一帧匹配批准构图；主体与发光层独立。需要小幅视角变化时可按固定相机生成例如 24–48 帧局部透明循环，先测试内存、压缩和解码成本。不要为整页制作视频/GIF；可优先固定主体 + QML 动态轨道，避免不必要的贴图序列。
5. 用相同光照制作生态球形节点的通用底座和光晕，图标由 Qt 动态覆盖，不能将软件名称、版本和数据烘焙进底图。

若 Blender 不可用且已有合格透明三维渲染，先接入它；若无合格素材，明确该层阻塞并提交其余真实修复，不回退扁平矩形后宣称完成。

## 4. Qt：从分层素材到正式交互界面

沿现有 Main.qml、工作台组件和 WorkbenchLayoutStore 实现，按实际架构抽取场景层、核心、轨道节点、玻璃面板，不另建平行产品。以下名称仅建议：WorkbenchScene.qml、EnergyCore.qml、OrbitNode.qml、PetalField.qml、GlassPanel.qml。

层序：远景 → 中景 → 人物与近景（按批准图遮挡关系）→ 局部核心与轨道/粒子 → 实际导航、搜索、统计、软件表、详情。使用统一场景坐标变换、锚点和局部 clip，场景装饰层不处理鼠标。透明遮罩只服务局部可读性，不漂白整个背景；先实现稳定静态构图，再加动态。现有布局编辑的拖动、缩放、显隐、保存规则不能丢失。

玻璃面板用半透明填充、细边框、渐变高光、局部 MultiEffect 阴影。若需要真实背景模糊，明确采样的是独立场景源而不是面板自身，不产生递归采样；限制渲染区域/分辨率。MultiEffect 不是自动背景毛玻璃，也不是三维渲染器。软件后端用已有半透明静态外观/预渲染阴影回退，不能称作 GPU 效果验收。

Inkscape 制作统一 SVG viewBox 和线宽，检查 Qt SVG 实际渲染（不依赖未经验证的滤镜、CSS 或字体）。已有品牌图标或系统主题图标按稳定软件身份匹配，缺失用统一合法通用图标，不裁截图。导航选中渐变使用真正 QML 状态，不烘焙文字。所有文字、版本、数量、状态、按钮由当前 SoftwareModel 和 CoreSnapshot 驱动，未知不伪造；匹配候选构图可用独立测试夹具，但生产模式绝不使用夹具值。

## 5. 动画如何写（参数是调试初值）

| 对象 | Qt 实现 | 初始建议 |
|---|---|---|
| 能量核心 | 主体 Image + 独立 emission Image；SequentialAnimation 改 scale/opacity，Easing.InOutSine | scale 0.985→1.02→0.985，5–6 秒一周期 |
| 轨道 | QtQuick.Shapes 的 Path/椭圆路径，或局部 ShaderEffect；轨迹参数 angle 驱动光点 x=cx+rx*cos(angle)、y=cy+ry*sin(angle) | 20–36 秒一圈；必要时按倾角旋转坐标并用前后层遮挡 |
| 生态节点 | 固定用户布局锚点 + 小幅视觉偏移，不修改保存的布局坐标 | 浮动 ±2–4 逻辑像素，4–6 秒；拖动时停动效 |
| 花瓣 | QtQuick.Particles ParticleSystem + Emitter + ImageParticle，使用实际透明花瓣 sprite；光点单独 emitter | 初始每秒 2–5，寿命 6–10 秒；限制活跃粒子数量 |
| 视差 | 对远/中/近景使用有限坐标偏移，只响应实际交互状态 | 各层约 1/3/6 像素，禁止破坏原图构图 |
| 按钮与导航 | HoverHandler/现有 Control 状态、Behavior/ColorAnimation/NumberAnimation | 120–180ms，高亮过渡；press 0.98；导航 180–220ms |
| 详情 | Transition，位置/透明度过渡，保留焦点和实际布局 | 200–260ms |
| 减少动态 | 统一 sceneRunning 条件，停止计时器、Emitter 和重复动画 | 隐藏、最小化、非工作台或 reducedMotion 时静止；截屏固定相位 |

不要使用动画不断修改用户拖动位置；不要给每个软件列表项创建永久计时器。真实任务状态只随模型变化，未知进度不可伪造百分比。装饰图层不能拦截鼠标、焦点或键盘导航。

如需能量流动/边缘高光 Shader：先验证已装 Qt 6 ShaderTools；编写 GLSL（Qt 6 的 uniform block 布局与采样规则、qt_Matrix/qt_Opacity 按实际阶段与官方示例配置），CMake find_package(Qt6 COMPONENTS ShaderTools ...) 和 qt_add_shaders 将源文件编译为 .qsb 并打包，ShaderEffect 引用对应 qrc URL。不能用 Qt5 内联 GLSL。只在核心/轨道局部使用 shader；先有无 shader 的正确静态回退。Qt 6 软件渲染后端不支持 ShaderEffect 或粒子渲染，offscreen/software CTest 通过不能证明 GPU 动效已完成。真实后端要记录实际 QSG 图形 API、GPU/驱动和帧时间；GPU 不可用时标注未验收，不自动切换为软件后端后冒充成功。原生 Wayland 与物理指针测试仍需明确授权。

## 6. 工程接线、迭代和验收

在现有 CMake/Qt resource 模式下登记生产资产，使用实际资源相对路径或 qrc URL，构建产物和临时 DESTDIR 验证资源可读、无开发机绝对路径依赖。不安装到真实系统，不部署 Broker。大型可编辑美术源文件单独管理，不把生成缓存全部打入程序。公开仓库只提交许可已确认、明确批准公开的内容；真实主机截图/路径/私有源不公开。

执行顺序（直接做，不写另一份计划）：
A. 读取原图与当前正式首页并实际渲染，形成区域级差距；锁定原始校验值，盘点已有独立素材，不重复全量治理。
B. 先制作/修复缺失的场景层和核心资产、图标，输出源文件及可重复脚本、各层预览和实际资源清单。
C. 在生产 QML 恢复首页构图、真实数据表格、右侧实例详情、交互、工作台编辑。实际接线后再增加核心与轨道、花瓣及局部动效。
D. 循环构建→渲染→同尺寸对照→修正直至可处理差异解决。每次检查背景、角色/猫、核心、节点、导航、搜索与状态区、底部表格、详情。相同数据夹具对照布局；不同数据不要求文字像素相同。禁止整图背景、透明点击覆盖、预录视频和单一相似率冒充成品。
E. 三尺寸 1672×941 / 1280×720 / 900×600（PRD 如另有尺寸也按原文），隔离 XDG、私有 session D-Bus、不可达 system D-Bus，Release、聚焦、完整 CTest；实际渲染截图和交互测试。GPU 与原生验收独立记录，测试动画运行和停用、拖动命中、可读性、帧时间 P95/P99 与最小化空闲行为。若无 GPU 或原生窗口授权，报告该独立验收阻塞；其他静态、交互与资源工作仍继续完成。
F. 报告已生成/修改的实际 .kra/.blend/.svg/PNG、脚本、QML、shader 文件及实际引用；实际运行命令、构建与测试证据；原图保护、许可和剩余不可处理阻塞。不要仅交说明。首页未经视觉确认不继续改其他七页。

禁止真实更新、未经批准的远程来源查询、提权、系统安装、部署、自动更新；D-195、Omarchy fail-closed、C2/B3 保持。保护整个本地工作树（包含未跟踪成果），不清理、重置、切换、覆盖、提交或推送生产源码；不得自动创建 Issue。通过只读 Git 对照设计分支与 GUI 参考分支，不合并旧 PoC。

## 技术参考（按实际安装版本核对）

- Blender Film Transparent: https://docs.blender.org/manual/en/latest/render/cycles/render_settings/film.html
- Krita 文档: https://docs.krita.org/
- Inkscape: https://inkscape.org/
- Qt MultiEffect: https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html
- Qt ImageParticle: https://doc.qt.io/qt-6/qml-qtquick-particles-imageparticle.html
- Qt ShaderEffect: https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html
- Qt qt_add_shaders: https://doc.qt.io/qt-6/qt-add-shaders.html
- Qt software adaptation 限制: https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations-software.html
- Qt licensing: https://doc.qt.io/qt-6/licensing.html

**立即在本地实际执行 A→F。不要只回报方案、软件列表或测试通过。**
