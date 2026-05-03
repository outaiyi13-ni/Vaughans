"""Emotion-support robot agent.

该模块提供一个可扩展的 Agent，用于：
1) 识别用户指令文本、动作事件、表情与声音特征；
2) 决策机器人身体动作；
3) 生成安抚式回复与基础科普建议。
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Callable, Dict, List, Optional
import re


class EmotionLevel(str, Enum):
    CALM = "calm"
    SAD = "sad"
    ANGRY = "angry"
    ANXIOUS = "anxious"
    PANIC = "panic"
    PAIN = "pain"


class PainType(str, Enum):
    GENERAL = "general"
    MENSTRUAL = "menstrual"


@dataclass
class UserSignal:
    text: str = ""
    action: str = ""
    facial_expression: str = ""
    voice_tone: str = ""


@dataclass
class RobotPlan:
    body_actions: List[str]
    reply: str
    emotion_level: EmotionLevel
    care_tips: List[str]


Recognizer = Callable[[UserSignal], Optional[EmotionLevel]]


class EmotionSoothingAgent:
    def __init__(self) -> None:
        self.text_rules: Dict[EmotionLevel, List[str]] = {
            EmotionLevel.PANIC: ["崩溃", "活不下去", "救命", "绝望", "不想活", "suicide"],
            EmotionLevel.PAIN: ["疼", "好痛", "痛死了", "胃疼", "头疼", "胸口痛", "肚子痛", "腹痛"],
            EmotionLevel.ANGRY: ["生气", "烦死", "讨厌", "愤怒", "火大"],
            EmotionLevel.SAD: ["难过", "想哭", "伤心", "孤独", "委屈", "低落"],
            EmotionLevel.ANXIOUS: ["焦虑", "害怕", "紧张", "不安", "担心"],
        }

        self.menstrual_keywords = ["经期", "姨妈", "月经", "生理期", "痛经"]

        self.action_rules: Dict[str, EmotionLevel] = {
            "crying": EmotionLevel.SAD,
            "shaking": EmotionLevel.ANXIOUS,
            "shouting": EmotionLevel.ANGRY,
            "hyperventilating": EmotionLevel.PANIC,
            "silence": EmotionLevel.SAD,
            "holding_stomach": EmotionLevel.PAIN,
            "holding_head": EmotionLevel.PAIN,
        }

        self.facial_rules: Dict[str, EmotionLevel] = {
            "pain_face": EmotionLevel.PAIN,
            "grimace": EmotionLevel.PAIN,
            "tearful": EmotionLevel.SAD,
            "fear_face": EmotionLevel.ANXIOUS,
        }

        self.voice_rules: Dict[str, EmotionLevel] = {
            "pain_voice": EmotionLevel.PAIN,
            "groaning": EmotionLevel.PAIN,
            "low_voice": EmotionLevel.SAD,
            "flat_voice": EmotionLevel.SAD,
            "trembling_voice": EmotionLevel.ANXIOUS,
        }

        self.priority: Dict[EmotionLevel, int] = {
            EmotionLevel.CALM: 0,
            EmotionLevel.SAD: 1,
            EmotionLevel.ANXIOUS: 2,
            EmotionLevel.ANGRY: 2,
            EmotionLevel.PAIN: 3,
            EmotionLevel.PANIC: 4,
        }

        self.recognizers: List[Recognizer] = [
            self._recognize_from_text,
            self._recognize_from_action,
            self._recognize_from_face,
            self._recognize_from_voice,
            self._recognize_from_punctuation,
        ]

    def register_recognizer(self, recognizer: Recognizer) -> None:
        self.recognizers.append(recognizer)

    def _pick_higher_priority(self, current: Optional[EmotionLevel], candidate: Optional[EmotionLevel]) -> Optional[EmotionLevel]:
        if candidate is None:
            return current
        if current is None:
            return candidate
        return candidate if self.priority[candidate] > self.priority[current] else current

    def _recognize_from_text(self, signal: UserSignal) -> Optional[EmotionLevel]:
        text = signal.text.lower().strip()
        for emotion in [EmotionLevel.PANIC, EmotionLevel.PAIN, EmotionLevel.ANGRY, EmotionLevel.SAD, EmotionLevel.ANXIOUS]:
            if any(kw in text for kw in self.text_rules.get(emotion, [])):
                return emotion
        return None

    def _recognize_from_action(self, signal: UserSignal) -> Optional[EmotionLevel]:
        return self.action_rules.get(signal.action)

    def _recognize_from_face(self, signal: UserSignal) -> Optional[EmotionLevel]:
        return self.facial_rules.get(signal.facial_expression)

    def _recognize_from_voice(self, signal: UserSignal) -> Optional[EmotionLevel]:
        return self.voice_rules.get(signal.voice_tone)

    def _recognize_from_punctuation(self, signal: UserSignal) -> Optional[EmotionLevel]:
        return EmotionLevel.ANXIOUS if re.search(r"!{2,}|！{2,}", signal.text) else None

    def recognize_emotion(self, signal: UserSignal) -> EmotionLevel:
        result: Optional[EmotionLevel] = None
        for recognizer in self.recognizers:
            result = self._pick_higher_priority(result, recognizer(signal))
        return result or EmotionLevel.CALM

    def detect_pain_type(self, signal: UserSignal) -> PainType:
        text = signal.text.lower()
        if any(kw in text for kw in self.menstrual_keywords):
            return PainType.MENSTRUAL
        return PainType.GENERAL

    def plan_body_actions(self, emotion: EmotionLevel, signal: UserSignal) -> List[str]:
        actions = ["turn_towards_user", "soft_eye_contact"]

        if signal.action == "hug_request":
            actions.extend(["ask_consent_for_hug", "gentle_open_arms"])

        if emotion == EmotionLevel.CALM:
            actions.extend(["nod", "relaxed_posture"])
        elif emotion == EmotionLevel.SAD:
            actions.extend(["slow_nod", "lower_voice", "offer_warm_water"])
        elif emotion == EmotionLevel.ANGRY:
            actions.extend(["step_back_half_meter", "open_palms", "steady_breath_led"])
        elif emotion == EmotionLevel.ANXIOUS:
            actions.extend(["guided_breathing", "slow_head_nod", "warm_tone"])
        elif emotion == EmotionLevel.PAIN:
            actions.extend(["reduce_environment_noise", "supportive_posture", "ask_pain_scale_0_to_10"])
            if self.detect_pain_type(signal) == PainType.MENSTRUAL:
                actions.extend(["offer_heat_pad", "suggest_rest_position", "remind_hydration"])
            else:
                actions.append("suggest_medical_help_if_needed")
        else:
            actions.extend(["grounding_breath_demo", "counting_with_fingers", "call_human_support_if_needed"])

        return actions

    def generate_care_tips(self, emotion: EmotionLevel, signal: UserSignal) -> List[str]:
        if emotion != EmotionLevel.PAIN:
            return []

        if self.detect_pain_type(signal) == PainType.MENSTRUAL:
            return [
                "可用热水袋或暖贴热敷下腹部 15-20 分钟，帮助缓解痉挛。",
                "轻柔活动（如慢走、拉伸）通常比完全卧床更有助于减轻痛经。",
                "保证饮水与规律进食，减少高咖啡因和高盐食物摄入。",
                "若医生已建议可使用布洛芬等非甾体抗炎药，请按医嘱或说明书规范使用。",
                "如果疼痛突然明显加重、伴发热/晕厥/异常大量出血，应尽快就医。",
            ]

        return [
            "先评估疼痛等级与持续时间，必要时尽快联系专业医疗支持。",
            "保持环境安静与舒适，帮助身体放松。",
        ]

    def generate_reply(self, emotion: EmotionLevel, signal: UserSignal) -> str:
        if emotion == EmotionLevel.CALM:
            return "我在这儿陪着你。你愿意和我说说现在最需要我做什么吗？"
        if emotion == EmotionLevel.SAD:
            return "我听到你现在很低落。我们先慢一点，我会一直在这儿陪你。"
        if emotion == EmotionLevel.ANGRY:
            return "我听到你现在很愤怒，这很重要。我会先保持安全距离，陪你把情绪放下来。"
        if emotion == EmotionLevel.ANXIOUS:
            return "你已经很努力了。跟着我吸气四秒、呼气六秒，我们先让身体慢下来。"
        if emotion == EmotionLevel.PAIN:
            if self.detect_pain_type(signal) == PainType.MENSTRUAL:
                return "我注意到你可能正在经历经期疼痛。你不是一个人，我会陪你一起缓解，并提醒你科学的应对方式。"
            return "我注意到你可能很痛苦。请告诉我疼痛程度（0到10），我会协助你联系帮助。"

        return "你现在的痛苦我听到了。请先看着我，跟我一起呼吸；如存在自伤或他伤风险，我会立即联系现场支持。"

    def handle_signal(self, signal: UserSignal) -> RobotPlan:
        emotion = self.recognize_emotion(signal)
        return RobotPlan(
            body_actions=self.plan_body_actions(emotion, signal),
            reply=self.generate_reply(emotion, signal),
            emotion_level=emotion,
            care_tips=self.generate_care_tips(emotion, signal),
        )


if __name__ == "__main__":
    agent = EmotionSoothingAgent()

    demo_signals = [
        UserSignal(text="我今天真的好焦虑，心跳很快", action="shaking"),
        UserSignal(text="我经期肚子痛得厉害", action="holding_stomach", facial_expression="pain_face", voice_tone="pain_voice"),
        UserSignal(text="我感觉很没力气", voice_tone="low_voice", facial_expression="tearful"),
    ]

    for i, s in enumerate(demo_signals, start=1):
        plan = agent.handle_signal(s)
        print(f"\nCase {i}")
        print("Emotion:", plan.emotion_level.value)
        print("Actions:", plan.body_actions)
        print("Reply:", plan.reply)
        if plan.care_tips:
            print("Care Tips:")
            for tip in plan.care_tips:
                print("-", tip)
