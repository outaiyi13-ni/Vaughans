#include <Servo.h>                                      // 引入 Arduino 官方 Servo 库，用于控制两只手臂舵机

const int LEFT_IN1_PIN = 5;                             // 左侧履带电机 L298N IN1 接 Arduino Uno 数字引脚 5
const int LEFT_IN2_PIN = 6;                             // 左侧履带电机 L298N IN2 接 Arduino Uno 数字引脚 6
const int RIGHT_IN3_PIN = 9;                            // 右侧履带电机 L298N IN3 接 Arduino Uno 数字引脚 9
const int RIGHT_IN4_PIN = 10;                           // 右侧履带电机 L298N IN4 接 Arduino Uno 数字引脚 10
const int TRIG_PIN = 11;                                // HC-SR04 超声波传感器 Trig 接 Arduino Uno 数字引脚 11
const int ECHO_PIN = 12;                                // HC-SR04 超声波传感器 Echo 接 Arduino Uno 数字引脚 12
const int LEFT_ARM_PIN = 3;                             // 左臂舵机信号线接 Arduino Uno 数字引脚 3
const int RIGHT_ARM_PIN = 4;                            // 右臂舵机信号线接 Arduino Uno 数字引脚 4
const int BUZZER_PIN = 8;                               // 无源蜂鸣器正极接 Arduino Uno 数字引脚 8

const int CRUISE_SPEED_PWM = 150;                       // 巡视前进速度，范围 0-255，数值越大履带越快
const int ACTION_SPEED_PWM = 180;                       // 后退与转向动作速度，范围 0-255，动作时略快一些更容易避障
const int DISTANCE_THRESHOLD_CM = 15;                   // 发现垃圾或障碍物的距离阈值，单位为厘米
const int ARM_HOME_ANGLE = 20;                          // 两只手臂的初始角度，可根据机械安装微调
const int ARM_PICK_ANGLE = 90;                          // 两只手臂抱起垃圾时转到的角度
const unsigned long ARM_HOLD_TIME_MS = 1500;            // 手臂抱起垃圾后停留时间，单位为毫秒
const unsigned long BACKWARD_TIME_MS = 1000;            // 收集完成后后退时间，单位为毫秒
const unsigned long TURN_RIGHT_TIME_MS = 1000;          // 后退完成后右转时间，单位为毫秒
const unsigned long SONAR_TIMEOUT_US = 30000;           // 超声波 echo 等待超时时间，30000 微秒约等于 5 米距离
const unsigned long PWM_PERIOD_US = 2000;               // 软件 PWM 周期，2000 微秒约等于 500Hz，适合直流电机调速
const unsigned long SENSOR_INTERVAL_MS = 80;            // 巡视时两次超声波测距之间的间隔，单位为毫秒
const int SERVO_STEP_DELAY_MS = 12;                     // 舵机平滑动作时每移动 1 度的延时，数值越大动作越慢

int currentLeftSpeed = 0;                               // 保存左侧履带当前目标速度，正数前进、负数后退、0 停止
int currentRightSpeed = 0;                              // 保存右侧履带当前目标速度，正数前进、负数后退、0 停止
unsigned long lastSensorReadMs = 0;                     // 保存上一次超声波测距的时间，用于控制测距频率

Servo leftArmServo;                                     // 创建左臂舵机对象
Servo rightArmServo;                                    // 创建右臂舵机对象

void setup() {                                          // Arduino 上电或复位后只执行一次的初始化函数
  pinMode(LEFT_IN1_PIN, OUTPUT);                        // 将左电机 IN1 引脚设置为输出模式
  pinMode(LEFT_IN2_PIN, OUTPUT);                        // 将左电机 IN2 引脚设置为输出模式
  pinMode(RIGHT_IN3_PIN, OUTPUT);                       // 将右电机 IN3 引脚设置为输出模式
  pinMode(RIGHT_IN4_PIN, OUTPUT);                       // 将右电机 IN4 引脚设置为输出模式
  pinMode(TRIG_PIN, OUTPUT);                            // 将超声波 Trig 引脚设置为输出模式
  pinMode(ECHO_PIN, INPUT);                             // 将超声波 Echo 引脚设置为输入模式
  pinMode(BUZZER_PIN, OUTPUT);                          // 将蜂鸣器引脚设置为输出模式
  digitalWrite(TRIG_PIN, LOW);                          // 让 Trig 引脚保持低电平，避免上电时误触发测距
  stopMoving();                                         // 初始化时立即停止两侧履带，防止机器人突然移动
  leftArmServo.attach(LEFT_ARM_PIN);                    // 将左臂舵机对象绑定到左臂舵机信号引脚
  rightArmServo.attach(RIGHT_ARM_PIN);                  // 将右臂舵机对象绑定到右臂舵机信号引脚
  leftArmServo.write(ARM_HOME_ANGLE);                   // 将左臂舵机转到初始角度
  rightArmServo.write(ARM_HOME_ANGLE);                  // 将右臂舵机转到初始角度
  delay(500);                                           // 等待舵机稳定到初始位置
}

void loop() {                                           // Arduino 主循环函数，会不断重复执行
  moveForward();                                        // 进入巡视模式，让瓦力持续向前缓慢移动
  refreshMotorPwm();                                    // 高频刷新软件 PWM，确保两侧履带持续获得调速脉冲
  if (millis() - lastSensorReadMs >= SENSOR_INTERVAL_MS) { // 如果距离上次测距已经超过设定间隔，就进行一次新测距
    lastSensorReadMs = millis();                        // 更新上一次测距时间，避免超声波触发过于频繁
    long distanceCm = getDistanceCm();                  // 读取前方距离，单位为厘米
    if (distanceCm > 0 && distanceCm < DISTANCE_THRESHOLD_CM) { // 如果测距有效且小于阈值，说明发现垃圾或障碍物
      stopMoving();                                     // 动作 A：立刻停止移动
      playHappyBeep();                                  // 动作 B：播放开心的短促“滴滴”声
      pickUpTrash();                                    // 动作 C：双臂抱起垃圾并放回初始位置
      timedBackward(BACKWARD_TIME_MS);                  // 动作 D 第一段：双轮反转后退指定时间
      timedTurnRight(TURN_RIGHT_TIME_MS);               // 动作 D 第二段：单侧轮转动右转指定时间
      stopMoving();                                     // 动作完成后再次停止，保证切回巡视前状态稳定
    }
  }
}

void moveForward() {                                    // 控制瓦力向前慢速行驶的函数
  setMotorSpeed(CRUISE_SPEED_PWM, CRUISE_SPEED_PWM);    // 左右履带都设置为正速度，实现直线前进
}

void moveBackward() {                                   // 控制瓦力向后倒车的函数
  setMotorSpeed(-ACTION_SPEED_PWM, -ACTION_SPEED_PWM);  // 左右履带都设置为负速度，实现直线后退
}

void turnRight() {                                      // 控制瓦力向右转弯的函数
  setMotorSpeed(ACTION_SPEED_PWM, 0);                   // 只让左履带前进、右履带停止，实现向右原地偏转
}

void stopMoving() {                                     // 控制瓦力停止移动的函数
  setMotorSpeed(0, 0);                                  // 左右履带速度都设置为 0，停止所有电机输出
}

void timedBackward(unsigned long durationMs) {          // 按指定毫秒数执行后退动作的函数
  unsigned long startMs = millis();                     // 记录后退动作开始时的系统时间
  moveBackward();                                       // 设置两侧履带为后退目标速度
  while (millis() - startMs < durationMs) {             // 在未达到指定时长前持续执行后退
    refreshMotorPwm();                                  // 持续刷新软件 PWM，让两侧履带保持后退
  }
  stopMoving();                                         // 到达指定时长后停止履带
}

void timedTurnRight(unsigned long durationMs) {         // 按指定毫秒数执行右转动作的函数
  unsigned long startMs = millis();                     // 记录右转动作开始时的系统时间
  turnRight();                                          // 设置履带为右转目标速度
  while (millis() - startMs < durationMs) {             // 在未达到指定时长前持续执行右转
    refreshMotorPwm();                                  // 持续刷新软件 PWM，让瓦力保持右转
  }
  stopMoving();                                         // 到达指定时长后停止履带
}

void setMotorSpeed(int leftSpeed, int rightSpeed) {     // 根据左右速度值保存 L298N 目标速度的总函数
  currentLeftSpeed = constrain(leftSpeed, -255, 255);   // 保存左侧履带速度，并限制到 -255 到 255 的安全范围
  currentRightSpeed = constrain(rightSpeed, -255, 255); // 保存右侧履带速度，并限制到 -255 到 255 的安全范围
  refreshMotorPwm();                                    // 立即刷新一次输出，让新速度尽快生效
}

void refreshMotorPwm() {                                // 按当前目标速度刷新两侧履带软件 PWM 的函数
  unsigned long phaseUs = micros() % PWM_PERIOD_US;     // 计算当前处在 PWM 周期中的哪个时间点
  driveOneMotor(LEFT_IN1_PIN, LEFT_IN2_PIN, currentLeftSpeed, phaseUs); // 根据当前相位刷新左侧履带输出
  driveOneMotor(RIGHT_IN3_PIN, RIGHT_IN4_PIN, currentRightSpeed, phaseUs); // 根据当前相位刷新右侧履带输出
}

void driveOneMotor(int forwardPin, int backwardPin, int speedValue, unsigned long phaseUs) { // 使用软件 PWM 控制单个直流电机的方向和速度
  int pwmValue = constrain(abs(speedValue), 0, 255);    // 取速度绝对值并限制到 0-255 的 PWM 范围
  if (pwmValue == 0) {                                  // 如果速度为 0，表示需要停止该电机
    digitalWrite(forwardPin, LOW);                      // 关闭该电机正转方向输入
    digitalWrite(backwardPin, LOW);                     // 关闭该电机反转方向输入
    return;                                             // 停止模式已设置完成，直接返回函数
  }
  unsigned long onTimeUs = PWM_PERIOD_US * pwmValue / 255; // 根据 PWM 数值计算本周期高电平持续时间
  int activePin = speedValue > 0 ? forwardPin : backwardPin; // 正速度选正转引脚，负速度选反转引脚
  int inactivePin = speedValue > 0 ? backwardPin : forwardPin; // 与运动方向相反的引脚必须保持低电平
  digitalWrite(inactivePin, LOW);                       // 关闭相反方向输入，避免 L298N 同一路桥臂冲突
  if (phaseUs < onTimeUs) {                             // 如果当前仍处在 PWM 通电阶段
    digitalWrite(activePin, HIGH);                      // 打开当前运动方向输入，为电机供电
  } else {                                              // 如果当前已经处在 PWM 断电阶段
    digitalWrite(activePin, LOW);                       // 关闭当前运动方向输入，降低电机平均速度
  }
}

long getDistanceCm() {                                  // 读取 HC-SR04 距离并返回厘米数的函数
  digitalWrite(TRIG_PIN, LOW);                          // 先拉低 Trig，确保触发脉冲从干净的低电平开始
  delayMicroseconds(2);                                 // 等待 2 微秒，让 Trig 低电平稳定
  digitalWrite(TRIG_PIN, HIGH);                         // 拉高 Trig，开始发送超声波触发脉冲
  delayMicroseconds(10);                                // 保持 10 微秒高电平，这是 HC-SR04 要求的触发宽度
  digitalWrite(TRIG_PIN, LOW);                          // 拉低 Trig，结束触发脉冲
  unsigned long durationUs = pulseIn(ECHO_PIN, HIGH, SONAR_TIMEOUT_US); // 读取 Echo 高电平持续时间，超时则返回 0
  if (durationUs == 0) {                                // 如果返回 0，说明本次测距超时或没有有效回波
    return -1;                                          // 返回 -1 表示无效距离，主循环会忽略这次读数
  }
  long distanceCm = durationUs / 58;                    // 将声波往返时间换算为厘米，常用公式为 微秒/58
  return distanceCm;                                    // 返回计算得到的前方距离
}

void pickUpTrash() {                                    // 控制两只手臂模拟抱起垃圾的完整动作函数
  moveArmsSmoothly(ARM_HOME_ANGLE, ARM_PICK_ANGLE);     // 两只手臂从初始角度平滑转到抱取角度
  delay(ARM_HOLD_TIME_MS);                              // 在抱取角度停留 1.5 秒，模拟把垃圾放进肚子
  moveArmsSmoothly(ARM_PICK_ANGLE, ARM_HOME_ANGLE);     // 两只手臂从抱取角度平滑回到初始角度
}

void moveArmsSmoothly(int fromAngle, int toAngle) {     // 让两只舵机同步平滑移动的函数
  int stepValue = fromAngle <= toAngle ? 1 : -1;         // 根据目标角度决定每次增加 1 度还是减少 1 度
  for (int angle = fromAngle; angle != toAngle + stepValue; angle += stepValue) { // 从起始角度逐步移动到目标角度
    leftArmServo.write(angle);                          // 将左臂舵机写入当前角度
    rightArmServo.write(angle);                         // 将右臂舵机写入当前角度，实现双臂同步动作
    delay(SERVO_STEP_DELAY_MS);                         // 每移动 1 度稍作等待，让动作更像机械臂而不是瞬间跳转
  }
}

void playHappyBeep() {                                  // 播放瓦力发现物品时开心短促音效的函数
  tone(BUZZER_PIN, 880, 120);                           // 播放 880Hz 音调 120 毫秒，第一声偏高
  delay(150);                                           // 等待第一声播放完成并留出短暂停顿
  tone(BUZZER_PIN, 660, 120);                           // 播放 660Hz 音调 120 毫秒，第二声偏低
  delay(150);                                           // 等待第二声播放完成并留出短暂停顿
  tone(BUZZER_PIN, 988, 150);                           // 播放 988Hz 音调 150 毫秒，第三声更高表现开心
  delay(180);                                           // 等待第三声播放完成
  noTone(BUZZER_PIN);                                   // 关闭蜂鸣器输出，避免持续发声
}
