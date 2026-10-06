#ifndef IRDecoder_h
#define IRDecoder_h

#include <Arduino.h>

// 红外接收器引脚（GPIO25，按你要求）
#define IR_RECEIVER_PIN 5
// Power键码值（固定为你需要的0xFFA25D）
#define IR_POWER_CODE 0xFFA25D
// 重复码（过滤长按重复触发）
#define IR_REPEAT_CODE 0xFFFFFFFF

/**
 * @brief 初始化红外接收器（设置引脚+串口）
 */
void IRDecoder_Init();

/**
 * @brief 解码NEC协议红外信号
 * @return 32位键码（成功）/ 0（无信号/解码失败）
 */
unsigned long IRDecoder_Decode();

#endif