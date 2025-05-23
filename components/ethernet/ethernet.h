/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize Ethernet driver based on Espressif IoT Development Framework Configuration
     *
     * @param[out] eth_handles_out array of initialized Ethernet driver handles
     * @param[out] eth_cnt_out number of initialized Ethernets
     * @return
     *          - ESP_OK on success
     *          - ESP_ERR_INVALID_ARG when passed invalid pointers
     *          - ESP_ERR_NO_MEM when there is no memory to allocate for Ethernet driver handles array
     *          - ESP_FAIL on any other failure
     */
    esp_err_t ethernet_init(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out);

    // static void eth_event_handler(void *arg, esp_event_base_t event_base,
    //                               int32_t event_id, void *event_data);

    // static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
    //                                  int32_t event_id, void *event_data);

    void ethernet_task(void *pvParameters);

    void ethernet_start();

    esp_err_t init_fs(void);

#ifdef __cplusplus
}
#endif
