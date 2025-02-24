
#include "driver/ledc.h"
#include "esp_timer.h"

static const ledc_timer_config_t pwm_timer_cfg = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_1_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 68500, // 68.5khz
    .clk_cfg = LEDC_AUTO_CLK,
};

static const ledc_channel_config_t pwm_channel_cfg = {
    .gpio_num = 16,
    .speed_mode =LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0,
    .flags = {},
};

class BPCSender
{
public:
    BPCSender()
    {
        ledc_timer_config(&pwm_timer_cfg);
        ledc_channel_config(&pwm_channel_cfg);

        static const esp_timer_create_args_t one_sec_timer_cfg = {
            .callback = BPCSender::one_sec_timer_cb,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "BPC 1s timer",
            .skip_unhandled_events = false,
        };

        static const esp_timer_create_args_t modulation_tiemr_cfg = {
            .callback = BPCSender::modulation_tiemr_cb,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "BPC 1s timer",
            .skip_unhandled_events = false,
        };

        esp_timer_create(&one_sec_timer_cfg, &one_sec_timer);
        esp_timer_create(&modulation_tiemr_cfg, &modulation_tiemr);

        esp_timer_start_periodic(one_sec_timer, 1000);
    }

    ~BPCSender()
    {
        esp_timer_delete(one_sec_timer);
        esp_timer_delete(modulation_tiemr);
    }
private:
    static void one_sec_timer_cb(void* arg)
    {
        reinterpret_cast<BPCSender*>(arg)->on_one_sec_interval();
    }

    static void modulation_tiemr_cb(void* arg)
    {
        reinterpret_cast<BPCSender*>(arg)->on_modulation_timer();
    }

    void on_one_sec_interval()
    {
        if (send_ptr --)
        {
            // 打开 PWM 输出。发射  68.5khz 电磁波.
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1, 0);

            // 根据当前码片，设置关闭定时.

            // 首先获得当前时间片应该发送的4进制数据
            auto c = (sending_code >> (send_ptr*2))&0x3;

            // 发送时间为 0 = 0.1s 1= 0.2s 2=0.3s 3=0.4s
            esp_timer_start_once(modulation_tiemr, c*100+100);
        }
        else
        {
            // 发送完毕，关闭 68.5khz 电磁波.
            send_ptr = 0;
            // 到这里，其实从 start 开始算起，一共过了 19s.
            ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 0);
        }
    }

    void on_modulation_timer()
    {
        // 时间到，关闭 68.5khz 电磁波.
        ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 0);
    }

    // 调用本 api, 发送新编码的时间.
    // 本代码，必须在 秒数为 00, 20, 40 的时候调用方可实现最大化性能.
    // 如果非要在 中间时间调用，则需要设置 send_ptr，跳过前面的秒数。
    // 比如在 10 秒的时候发送，设置 send_ptr = 9. 只发送剩余 9 个4进制数。
    // 这样在 20 秒的时候又可以按正确的分片发送了.
    void start(uint64_t sending_code, int send_ptr = 19)
    {
        // 立即调用回调，先拉高 pwm
        this->send_ptr = send_ptr;
        this->sending_code = sending_code;
        on_one_sec_interval();
        esp_timer_restart(one_sec_timer, 1000);
    }

    esp_timer_handle_t one_sec_timer; // every 1s
    esp_timer_handle_t modulation_tiemr; // once, for 0.1s or 0.2s or 0.3s or 0.4s

    // 待发送的代码
    uint64_t  sending_code;
    // 发送指针。19 - 0 递减。为 0 的时候发送低电平. // 此时有 1s 的时间用于计算新的发送代码.
    int send_ptr;
};

