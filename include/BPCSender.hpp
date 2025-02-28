
#pragma once

#include "driver/rmt_tx.h"
#include "esp_log.h"

class BPCSender
{
public:
    BPCSender()
    {
        static const rmt_tx_channel_config_t tx_chan_config = {
            .gpio_num = GPIO_NUM_10,                    // GPIO 编号
            .clk_src = RMT_CLK_SRC_DEFAULT,   // 选择时钟源
            .resolution_hz = 10,              // 0.1s 滴答分辨率
            .mem_block_symbols = 19,          // 内存块大小，即 64 * 4 = 256 字节
            .trans_queue_depth = 1,           // 设置后台等待处理的事务数量
            .flags = {
                .invert_out = false,        // 不反转输出信号
                .with_dma = false,           // 不需要 DMA 后端
            },
        };
        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));

        ESP_LOGI("BPC_RMT", "setup 68.5khz carrier");
        rmt_carrier_config_t carrier_cfg = {
            .frequency_hz = 68500, // 68.5KHz
            .duty_cycle = 0.50,
        };
        ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

        const rmt_simple_encoder_config_t encoder_config = {
            .callback = &BPCSender::rmt_encoder,
            .arg = this,
            .min_chunk_size = 32
        };

        ESP_LOGI("BPC_RMT", "install encoder to TX channel");
        ESP_ERROR_CHECK(rmt_new_simple_encoder(&encoder_config, &bpc_encoder));

        ESP_LOGI("BPC_RMT", "enable RMT TX channels");
        ESP_ERROR_CHECK(rmt_enable(tx_channel));
    }

    ~BPCSender()
    {
        rmt_disable(tx_channel);
        rmt_del_encoder(bpc_encoder);
        rmt_del_channel(tx_channel);
    }

    // 调用本 api, 发送新编码的时间.
    void start(uint64_t sending_code)
    {
        this->sending_code =sending_code;

        static const rmt_transmit_config_t transmit_config = {
            .loop_count = 0, // no loop
            .flags = {
                .eot_level = 1,
                .queue_nonblocking = 1
            },
        };

        if (ESP_FAIL == rmt_transmit(tx_channel, bpc_encoder, this, sizeof(*this), &transmit_config))
        {
            rmt_disable(tx_channel);
            rmt_enable(tx_channel);

            ESP_ERROR_CHECK(rmt_transmit(tx_channel, bpc_encoder, this, sizeof(*this), &transmit_config));
        }
    }

private:
    inline size_t rmt_encoder(const void *data, size_t data_size, size_t symbols_written, size_t symbols_free, rmt_symbol_word_t *symbols, bool *done)
    {
        for (int i=0; i < 19; i++)
        {
            auto symbol =  (sending_code >> ( 36 - i*2 ) )& 0x3;

            symbols[i].duration0 = symbol + 1; // 0 = 0.1s, 1 = 0.2s , 2 = 0.3s 3 = 0.4s
            symbols[i].level0 = 0; // level low

            symbols[i].duration1 = 9 - symbol; // 0 = 0.9s, 1 = 0.8s , 2 = 0.7s 3 = 0.6s
            symbols[i].level1 = 1; // level high
        }

        symbols[19].duration0 = symbols[19].duration1 = 15;
        symbols[19].level0 = symbols[19].level1 = 1;

        *done = true;

        return 20;
    }

    static size_t rmt_encoder(const void *data, size_t data_size, size_t symbols_written, size_t symbols_free, rmt_symbol_word_t *symbols, bool *done, void *arg)
    {
        return reinterpret_cast<BPCSender*>(arg)->rmt_encoder(data, data_size, symbols_written, symbols_free, symbols, done);
    }

    rmt_channel_handle_t tx_channel;
    rmt_encoder_handle_t bpc_encoder;

    // 待发送的代码
    uint64_t  sending_code;
    // 发送指针。19 - 0 递减。为 0 的时候发送低电平. // 此时有 1s 的时间用于计算新的发送代码.
    int send_ptr;
};

