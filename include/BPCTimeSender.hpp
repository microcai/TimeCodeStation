
#pragma once

#include <Arduino.h>

#include "BPCSender.hpp"

#include <time.h>

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"

static inline int parity_bit(uint64_t x) {
    x ^= x >> 32;  // 折叠高32位与低32位
    x ^= x >> 16;  // 折叠剩余高16位与低16位
    x ^= x >> 8;   // 继续折叠到8位
    x ^= x >> 4;    // 折叠到4位
    x ^= x >> 2;    // 折叠到2位
    x ^= x >> 1;    // 折叠到1位
    return x & 1;   // 取最低位作为奇偶标志
}

// 一共  38bit, 外加 1秒 帧空格，需要20s发送
static void bpc_encode(struct tm tm_time, uint64_t& outencode)
{
    // 基础时间参数转换
    const int year = tm_time.tm_year + 1900 - 2000;  // 基于2000年的偏移
    const int month = tm_time.tm_mon + 1;
    const int hour_24 = tm_time.tm_hour;
    const int is_pm = hour_24/12;
    const int hour_12 = (hour_24 > 12) ? hour_24 - 12 : hour_24;
    const int sec = tm_time.tm_sec;

    // 四进制编码组件（每个元素代表一个四进制位）
    uint64_t encoded = 0;

    // 帧结构组装（参考专利附图布局）
    encoded <<=2;
    encoded |= sec < 21 ? 0 : ( sec < 41 ? 1 : 2 );  // P1 帧序号
    encoded <<= 2; 
    encoded |= 0; // P2 总是 0

    encoded <<= 4; // 时
    encoded |= hour_12;

    encoded <<= 6;
    encoded |= tm_time.tm_min; // 分 6 位

    encoded <<= 4; //
    encoded |= (tm_time.tm_wday == 0 ? 7 : tm_time.tm_wday); // 星期, 有 4 位

    encoded <<= 1;
    encoded |= is_pm; // 上下午标志, 1 位

    encoded <<=1;
    encoded |= parity_bit(encoded & 0x3FFFE); // 偶校验位

    encoded <<= 6;
    encoded |= tm_time.tm_mday; // 日，6位

    encoded <<=4;
    encoded |= month; // 月， 4位

    encoded <<=6;
    encoded |= year & 0x3F; // 年， 低 6 位
    encoded <<=1;
    encoded |= year >> 6; // 年的第7位
    encoded <<=1;
    encoded |= parity_bit(encoded & 0x3FFFE); // 偶校验位
    outencode = encoded;
}

// 这个类依赖系统时间运行，因此需确保启用 SNTP 服务
class BPCTimeSender
{
public:
    void init()
    {
        bpc_sender.init();

        // 设定定时器，在每个时间的 00/20/40 秒唤醒.
        const esp_timer_create_args_t wake_up_timer_cfg = {
            .callback = &BPCTimeSender::timer_fireup,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "BPC timer",
            .skip_unhandled_events = false,
        };

        auto result = esp_timer_create(&wake_up_timer_cfg, &wake_up_timer);
    }

    ~BPCTimeSender()
    {
        esp_timer_stop(wake_up_timer);
        esp_timer_delete(wake_up_timer);
    }

    void loop()
    {
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        if (tv_now.tv_usec > 500000)
        {
            tv_now.tv_sec += 3600*8;

            struct tm the_time;
            the_time = *localtime(&tv_now.tv_sec);
            printf("current time is 星期%d  %d:%d:%d\n", the_time.tm_wday, the_time.tm_hour, the_time.tm_min, the_time.tm_sec);

            switch(the_time.tm_sec)
            {
                case 0:
                case 20:
                case 40:
                {
                    time_t broad_cast_timepoint = tv_now.tv_sec;
                    // 在 59s 的时候，开始设置
                    the_time = *localtime(&broad_cast_timepoint);
                    bpc_encode(the_time, this->encoded_bpc_data);
                    printf(" %d:%d:%d bpc encoded as %x %x\n", the_time.tm_hour, the_time.tm_min, the_time.tm_sec, encoded_bpc_data, encoded_bpc_data >> 32);

                    gettimeofday(&tv_now, NULL);
                    // 计算 剩余微秒数, 到 999980 微秒的时候，触发定时器.
                    // 999980 - tv_now.tv_usec
                    auto RESULT = esp_timer_start_once(wake_up_timer, 999980 - tv_now.tv_usec);
                    printf("esp_timer_start_once delay %d, result = %d\n", 999980 - tv_now.tv_usec, RESULT);

                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    break;
                }
                default:
                    vTaskDelay(500);
            }
            return;
        }
        else
        {
            vTaskDelay(10);
        }
    }
private:
    inline void timer_fireup()
    {
        printf("start sending BPC code %x %x\n", encoded_bpc_data, encoded_bpc_data >> 32);
        bpc_sender.start(encoded_bpc_data);
    }
    static void timer_fireup(void* arg)
    {
        reinterpret_cast<BPCTimeSender*>(arg)->timer_fireup();
    }

    uint64_t encoded_bpc_data;
    esp_timer_handle_t wake_up_timer;
    BPCSender bpc_sender;
};
