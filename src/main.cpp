

#include <time.h>

#include "BPCSender.hpp"

static int parity_bit(uint64_t x) {
    x ^= x >> 32;  // 折叠高32位与低32位
    x ^= x >> 16;  // 折叠剩余高16位与低16位
    x ^= x >> 8;   // 继续折叠到8位
    x ^= x >> 4;    // 折叠到4位
    x ^= x >> 2;    // 折叠到2位
    x ^= x >> 1;    // 折叠到1位
    return x & 1;   // 取最低位作为奇偶标志
}

// 一共  38bit, 外加 1秒 帧空格，需要20s发送
uint64_t bpc_encode(struct tm tm_time)
{
    // 基础时间参数转换
    const int year = tm_time.tm_year + 1900 - 2000;  // 基于2000年的偏移
    const int month = tm_time.tm_mon + 1;
    const int hour_24 = tm_time.tm_hour;
    const int is_pm = (hour_24 >= 12) ? 1 : 0;
    const int hour_12 = (hour_24 > 12) ? hour_24 - 12 : hour_24;
    const int sec = tm_time.tm_sec;

    // 四进制编码组件（每个元素代表一个四进制位）
    uint64_t encoded = 0;

    // 帧结构组装（参考专利附图布局）
    //encoded <<=2;
    encoded |= sec < 21 ? 0 : ( sec < 41 ? 1 : 2 );  // P0 起始位
    encoded <<= 6; //


    encoded |= hour_12; // 时

    encoded <<= 6; //
    encoded |= tm_time.tm_min; // 分 6 位

    encoded <<= 4; //
    encoded |= (tm_time.tm_wday + 1); // 星期, 有 4 位

    encoded <<= 1;
    encoded |= is_pm; // 上下午标志, 1 位

    encoded <<=1;
    encoded |= parity_bit(encoded); // 偶校验位

    encoded <<= 6;
    encoded |= tm_time.tm_mday; // 日，6位

    encoded <<=4;
    encoded |= month; // 月， 4位

    encoded <<=6;
    encoded |= year & 0x3F; // 年， 低 6 位
    encoded <<=1;
    encoded |= year >> 6; // 年的第7位
    encoded <<=1;
    encoded |= parity_bit(encoded); // 偶校验位

    return  encoded;
}

// 这个类依赖系统时间运行，因此需确保启用 SNTP 服务
class BPCTimeSender
{
public:
    BPCTimeSender()
    {
        // 设定定时器，在每个时间的 00/20/40 秒唤醒.
        

    }

private:

    BPCSender bpc_sender;
};

BPCTimeSender bpc_sation;

extern "C" void app_main()
{


}