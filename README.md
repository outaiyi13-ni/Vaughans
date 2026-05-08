# Vaughans

一个用于情感机器人安抚场景的示例项目。

## 文件

- `soothing_agent.py`：识别用户文本、动作、表情、声音信号，协调机器人身体动作并生成安抚回复。

## 新增能力

- 痛苦表情识别（如 `pain_face`、`grimace`）
- 痛苦声音识别（如 `pain_voice`、`groaning`）
- 低落声音识别（如 `low_voice`、`flat_voice`）
- 女性经期疼痛识别（如关键词 `经期`、`姨妈`、`生理期`、`痛经`）
- 经期疼痛安抚动作与科学缓解建议（热敷、补水、轻运动、合理用药与就医提醒）
- 可扩展识别器管线（`register_recognizer`）用于未来新增更多模态识别代码

## 快速运行

```bash
python3 soothing_agent.py
```

你可以把规则替换成自己的模型或硬件接口（语音识别、视觉动作识别、机械臂动作控制等）。

## WALL-E 履带机器人 Arduino 示例

- `wall_e_trash_collector/wall_e_trash_collector.ino`：可直接在 Arduino IDE 中打开并烧录到 Arduino Uno 的 WALL-E “捡垃圾”履带机器人示例，包含 L298N 履带控制、HC-SR04 测距、双舵机手臂动作与无源蜂鸣器音效。
