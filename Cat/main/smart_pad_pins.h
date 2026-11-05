#pragma once

// HX711 로드셀 4채널 핀 정의 (회로도 Sheet 1, 2 기준)
#define PIN_HX711_SCK       23   // L_SCK (공통 클록)
#define PIN_HX711_DATA1     21   // L_DATA1
#define PIN_HX711_DATA2     20   // L_DATA2
#define PIN_HX711_DATA3     19   // L_DATA3
#define PIN_HX711_DATA4     18   // L_DATA4

// 전면 UI 핀 정의
#define PIN_KEY_IN           2   // 전면 스위치 입력 (Active Low)
#define PIN_FLED_DOUT        3   // 전면 상태 LED 데이터 라인