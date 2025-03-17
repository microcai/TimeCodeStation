
#pragma once

#include "driver/rmt_tx.h"
#include "esp_log.h"

static const rmt_transmit_config_t transmit_config = {
    .loop_count = 0, // no loop
    .flags = {
        .eot_level = 1,
        .queue_nonblocking = 1
    },
};

class BPCSender
{
public:
    void init()
    {
        static const rmt_tx_channel_config_t tx_chan_config = {
            .gpio_num = GPIO_NUM_12,                    // GPIO 编号
            .clk_src = RMT_CLK_SRC_APB,   // 选择时钟源
            .resolution_hz = 68500*5,              // 2.9197us 滴答分辨率
            .mem_block_symbols = 64,          // 内存块大小，即 64 * 4 = 256 字节
            .trans_queue_depth = 4,           // 设置后台等待处理的事务数量
            .flags = {
                .invert_out = false,        // 反转输出信号
                .with_dma = false,           // 不需要 DMA 后端
            },
        };
        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));

        ESP_LOGI("BPC_RMT", "setup 68.5khz carrier");
        rmt_carrier_config_t carrier_cfg = {
            .frequency_hz = 68450, // 68.5KHz
            .duty_cycle = 0.50,
        };

        ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

        static const rmt_copy_encoder_config_t encoder_config = {
        };

        ESP_LOGW("BPC_RMT", "install encoder to TX channel");
        ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &bpc_encoder));

        static const rmt_tx_event_callbacks_t cb_cfg = {
            .on_trans_done = &BPCSender::on_trans_done
        };

        ESP_LOGW("BPC_RMT", "enable RMT TX channels");
        ESP_ERROR_CHECK(rmt_enable(tx_channel));
        rmt_tx_register_event_callbacks(tx_channel, &cb_cfg,this);

        static const rmt_symbol_word_t starting = {
            .duration0 = 340/2,
            .level0 = 1,
            .duration1 = 340/2,
            .level1 = 1,
        };
        auto result = rmt_transmit(tx_channel, bpc_encoder, &starting, sizeof(starting), &transmit_config);
        rmt_tx_wait_all_done(tx_channel, 500);
    }
    
    ~BPCSender()
    {
        rmt_disable(tx_channel);
        rmt_del_encoder(bpc_encoder);
        rmt_del_channel(tx_channel);
    }

    // 调用本 api, 发送新编码的时间.
    void start(uint64_t _sending_code)
    {
        static const char symbolstr[] = {'0', '1', '2', '3'};

        static const rmt_symbol_word_t low_out_symbol = {
            .duration0 = 34250/2,
            .level0 = 0,
            .duration1 = 34250/2,
            .level1 = 0,
        };

        static const rmt_symbol_word_t high_out_symbol = {
            .duration0 = 34250/2,
            .level0 = 1,
            .duration1 = 34250/2,
            .level1 = 1,
        };

        char encoded_str[20] = { 0 };
        for (int i=0; i < 19; i++)
        {
            // 每 10 个 symbol 表示 1s， 每 34250 tick 表示 0.1s
            // 0 = 0.1s, 1 = 0.2s , 2 = 0.3s 3 = 0.4s

            uint32_t symbol =  (_sending_code >> ( 36 - i*2 ) )& 0x3;

            uint32_t ticks_low = (symbol+1);
            uint32_t ticks_high = (9-symbol);

            // 将 ticks_low 和 ticks_high 分配到 10个 symbol 里
            for (int j=0; j < 10; j++)
            {
                if (j < ticks_low)
                    sending_code[i*10+j] = low_out_symbol;
                else
                 sending_code[i*10+j] = high_out_symbol;
            }
            // 0 = 0.1s, 1 = 0.2s , 2 = 0.3s 3 = 0.4s

            encoded_str[i] = symbolstr[symbol];
        }

        printf("encoding BPC symbol %s\n", encoded_str);

        auto result = rmt_transmit(tx_channel, bpc_encoder, sending_code, sizeof(sending_code), &transmit_config);
        printf("rmt_transmit result with %d\n", result);

        if (ESP_OK != result)
        {
            rmt_disable(tx_channel);
            rmt_enable(tx_channel);

            ESP_ERROR_CHECK(rmt_transmit(tx_channel, bpc_encoder, this, sizeof(*this), &transmit_config));
        }
    }

private:

    inline bool on_trans_done(rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t *edata)
    {
        return false;
        printf("BPC frame send ok\n");
    }

    static bool  on_trans_done(rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t *edata, void *user_ctx)
    {
        return reinterpret_cast<BPCSender*>(user_ctx)->on_trans_done(tx_chan, edata);
    }

    rmt_channel_handle_t tx_channel;
    rmt_encoder_handle_t bpc_encoder;

    // 待发送的代码
    rmt_symbol_word_t sending_code[19*10];
};

