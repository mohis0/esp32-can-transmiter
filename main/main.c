/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/can.h"
#include "driver/gpio.h"

#define CAN_GPIO_RX GPIO_NUM_21
#define CAN_GPIO_TX GPIO_NUM_22

TaskHandle_t can_handle;

void can_function(void *pvParameters);

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    unsigned major_rev = chip_info.full_revision / 100;
    unsigned minor_rev = chip_info.full_revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    xTaskCreatePinnedToCore(can_function, "can task", 4096, NULL, 12, &can_handle, 1);

    vTaskDelete(0);

    // for (int i = 10; i >= 0; i--) {
    //     printf("Restarting in %d seconds...\n", i);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

void can_function(void *pvParameters)
{
    can_general_config_t can_config_g = CAN_GENERAL_CONFIG_DEFAULT(CAN_GPIO_TX, CAN_GPIO_RX, CAN_MODE_NORMAL);
    can_timing_config_t can_config_t = CAN_TIMING_CONFIG_125KBITS();
    can_filter_config_t can_config_f = CAN_FILTER_CONFIG_ACCEPT_ALL();
    if (can_driver_install(&can_config_g, &can_config_t, &can_config_f) != ESP_OK)
    {
        printf("Failed to install can driver\n");
        return;
    }
    if (can_start() != ESP_OK)
    {
        printf("Failed to start can driver\n");
        return;
    }
    printf("can driver installed and started successfully\n");
    while (1)
    {
        // can_start();
        can_message_t message;
        message.identifier = 0xAAAA;
        message.flags = CAN_MSG_FLAG_EXTD;
        message.data_length_code = 4;

        for (int i = 0; i < 4; i++)
        {
            message.data[i] = i + 24;
        }
        if (can_transmit(&message, pdMS_TO_TICKS(100)) != ESP_OK)
        {
            while (1)
            {
                can_status_info_t can_status;
                can_get_status_info(&can_status);
                printf("can state: %d |tx queue: %d\n", can_status.state, can_status.msgs_to_tx);
                if (can_status.state == CAN_STATE_BUS_OFF)
                    can_initiate_recovery();
                else if (can_status.state == CAN_STATE_RECOVERING || can_status.state == CAN_STATE_STOPPED)
                    can_start();
                if (can_status.state == CAN_STATE_RUNNING)
                    break;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            // printf("Failed to trasmit data, try to clear queue\n");
            can_clear_transmit_queue();
        }
        else
        {
            printf("message send successfully\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}