#include <stdio.h>
#include <string.h>
#include "esp_netif.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#if CONFIG_ETH_USE_SPI_ETHERNET
#include "driver/spi_master.h"

#include "../../main/includes/events.h"

#endif // CONFIG_ETH_USE_SPI_ETHERNET

#include "esp_vfs_fat.h"

#if SOC_SDMMC_HOST_SUPPORTED
#include "driver/sdmmc_host.h"
#endif
#include "sdmmc_cmd.h"

#include "ethernet.h"

static const char *TAG = "eth_init";

TaskHandle_t ethernet_handle = NULL;

static bool success = false; // Success status from init etherner task

#if CONFIG_UMNI_SPI_ETHERNETS_NUM
#define SPI_ETHERNETS_NUM CONFIG_UMNI_SPI_ETHERNETS_NUM
#else
#define SPI_ETHERNETS_NUM 0
#endif

#if CONFIG_UMNI_USE_INTERNAL_ETHERNET
#define INTERNAL_ETHERNETS_NUM 1
#else
#define INTERNAL_ETHERNETS_NUM 0
#endif

#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num)                               \
    do                                                                                   \
    {                                                                                    \
        eth_module_config[num].spi_cs_gpio = CONFIG_UMNI_ETH_SPI_CS##num##_GPIO;         \
        eth_module_config[num].int_gpio = CONFIG_UMNI_ETH_SPI_INT##num##_GPIO;           \
        eth_module_config[num].phy_reset_gpio = CONFIG_UMNI_ETH_SPI_PHY_RST##num##_GPIO; \
        eth_module_config[num].phy_addr = CONFIG_UMNI_ETH_SPI_PHY_ADDR##num;             \
    } while (0)

typedef struct
{
    uint8_t spi_cs_gpio;
    uint8_t int_gpio;
    int8_t phy_reset_gpio;
    uint8_t phy_addr;
    uint8_t *mac_addr;
} spi_eth_module_config_t;

#if CONFIG_UMNI_USE_INTERNAL_ETHERNET
/**
 * @brief Internal ESP32 Ethernet initialization
 *
 * @param[out] mac_out optionally returns Ethernet MAC object
 * @param[out] phy_out optionally returns Ethernet PHY object
 * @return
 *          - esp_eth_handle_t if init succeeded
 *          - NULL if init failed
 */
static esp_eth_handle_t eth_init_internal(esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out)
{
    esp_eth_handle_t ret = NULL;

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = CONFIG_UMNI_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_UMNI_ETH_PHY_RST_GPIO;
    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
    esp32_emac_config.smi_mdc_gpio_num = CONFIG_UMNI_ETH_MDC_GPIO;
    esp32_emac_config.smi_mdio_gpio_num = CONFIG_UMNI_ETH_MDIO_GPIO;
    // Create new ESP32 Ethernet MAC instance
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    // Create new PHY instance based on board configuration
#if CONFIG_UMNI_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_UMNI_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_UMNI_ETH_PHY_LAN87XX
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
#elif CONFIG_UMNI_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#elif CONFIG_UMNI_ETH_PHY_KSZ80XX
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz80xx(&phy_config);
#endif
    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&config, &eth_handle) == ESP_OK, NULL,
                      err, TAG, "Ethernet driver install failed");

    if (mac_out != NULL)
    {
        *mac_out = mac;
    }
    if (phy_out != NULL)
    {
        *phy_out = phy;
    }
    return eth_handle;
err:
    if (eth_handle != NULL)
    {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL)
    {
        mac->del(mac);
    }
    if (phy != NULL)
    {
        phy->del(phy);
    }
    return ret;
}
#endif // CONFIG_UMNI_USE_INTERNAL_ETHERNET

#if CONFIG_UMNI_USE_SPI_ETHERNET
/**
 * @brief SPI bus initialization (to be used by Ethernet SPI modules)
 *
 * @return
 *          - ESP_OK on success
 */
static esp_err_t spi_bus_init(void)
{
    esp_err_t ret = ESP_OK;

    gpio_pullup_dis(CONFIG_UMNI_ETH_SPI_CS0_GPIO); // add this to tests

    // Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_UMNI_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_UMNI_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_UMNI_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(CONFIG_UMNI_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                      err, TAG, "SPI host #%d init failed", CONFIG_UMNI_ETH_SPI_HOST);

    return ret;

err:
    return ESP_FAIL;
}

/**
 * @brief Ethernet SPI modules initialization
 *
 * @param[in] spi_eth_module_config specific SPI Ethernet module configuration
 * @param[out] mac_out optionally returns Ethernet MAC object
 * @param[out] phy_out optionally returns Ethernet PHY object
 * @return
 *          - esp_eth_handle_t if init succeeded
 *          - NULL if init failed
 */
static esp_eth_handle_t eth_init_spi(spi_eth_module_config_t *spi_eth_module_config, esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out)
{
    esp_eth_handle_t ret = NULL;

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = spi_eth_module_config->phy_addr;
    phy_config.reset_gpio_num = spi_eth_module_config->phy_reset_gpio;

    // Configure SPI interface for specific SPI module
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_UMNI_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = spi_eth_module_config->spi_cs_gpio};
    // Init vendor specific MAC config to default, and create new SPI Ethernet MAC instance
    // and new PHY instance based on board configuration
#if CONFIG_UMNI_USE_KSZ8851SNL
    eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_UMNI_ETH_SPI_HOST, &spi_devcfg);
    ksz8851snl_config.int_gpio_num = spi_eth_module_config->int_gpio;
    esp_eth_mac_t *mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8851snl(&phy_config);
#elif CONFIG_UMNI_USE_DM9051
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_UMNI_ETH_SPI_HOST, &spi_devcfg);
    dm9051_config.int_gpio_num = spi_eth_module_config->int_gpio;
    esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#elif CONFIG_UMNI_USE_W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_UMNI_ETH_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = spi_eth_module_config->int_gpio;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
#endif // CONFIG_UMNI_USE_W5500
    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config_spi, &eth_handle) == ESP_OK, NULL, err, TAG, "SPI Ethernet driver install failed");

    // The SPI Ethernet module might not have a burned factory MAC address, we can set it manually.
    if (spi_eth_module_config->mac_addr != NULL)
    {
        ESP_GOTO_ON_FALSE(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, spi_eth_module_config->mac_addr) == ESP_OK,
                          NULL, err, TAG, "SPI Ethernet MAC address config failed");
    }

    if (mac_out != NULL)
    {
        *mac_out = mac;
    }
    if (phy_out != NULL)
    {
        *phy_out = phy;
    }
    return eth_handle;
err:
    if (eth_handle != NULL)
    {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL)
    {
        mac->del(mac);
    }
    if (phy != NULL)
    {
        phy->del(phy);
    }
    return ret;
}
#endif // CONFIG_UMNI_USE_SPI_ETHERNET

esp_err_t ethernet_init(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out)
{
    esp_err_t ret = ESP_OK;
    esp_eth_handle_t *eth_handles = NULL;
    uint8_t eth_cnt = 0;

#if CONFIG_UMNI_USE_INTERNAL_ETHERNET || CONFIG_UMNI_USE_SPI_ETHERNET
    ESP_GOTO_ON_FALSE(eth_handles_out != NULL && eth_cnt_out != NULL, ESP_ERR_INVALID_ARG,
                      err, TAG, "invalid arguments: initialized handles array or number of interfaces");
    eth_handles = calloc(SPI_ETHERNETS_NUM + INTERNAL_ETHERNETS_NUM, sizeof(esp_eth_handle_t));
    ESP_GOTO_ON_FALSE(eth_handles != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory");

#if CONFIG_UMNI_USE_INTERNAL_ETHERNET
    eth_handles[eth_cnt] = eth_init_internal(NULL, NULL);
    ESP_GOTO_ON_FALSE(eth_handles[eth_cnt], ESP_FAIL, err, TAG, "internal Ethernet init failed");
    eth_cnt++;
#endif // CONFIG_UMNI_USE_INTERNAL_ETHERNET

#if CONFIG_UMNI_USE_SPI_ETHERNET
    ESP_GOTO_ON_ERROR(spi_bus_init(), err, TAG, "SPI bus init failed");
    ESP_GOTO_ON_ERROR(init_fs(), err, TAG, "Init fs failed");
    // Init specific SPI Ethernet module configuration from Kconfig (CS GPIO, Interrupt GPIO, etc.)
    spi_eth_module_config_t spi_eth_module_config[CONFIG_UMNI_SPI_ETHERNETS_NUM] = {0};
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);
    // The SPI Ethernet module(s) might not have a burned factory MAC address, hence use manually configured address(es).
    // In this example, Locally Administered MAC address derived from ESP32x base MAC address is used.
    // Note that Locally Administered OUI range should be used only when testing on a LAN under your control!
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    ESP_GOTO_ON_ERROR(esp_efuse_mac_get_default(base_mac_addr), err, TAG, "get EFUSE MAC failed");
    uint8_t local_mac_1[ETH_ADDR_LEN];
    esp_derive_local_mac(local_mac_1, base_mac_addr);
    spi_eth_module_config[0].mac_addr = local_mac_1;

#if CONFIG_UMNI_SPI_ETHERNETS_NUM > 1
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 1);
    uint8_t local_mac_2[ETH_ADDR_LEN];
    base_mac_addr[ETH_ADDR_LEN - 1] += 1;
    esp_derive_local_mac(local_mac_2, base_mac_addr);
    spi_eth_module_config[1].mac_addr = local_mac_2;
#endif
#if CONFIG_UMNI_SPI_ETHERNETS_NUM > 2
#error Maximum number of supported SPI Ethernet devices is currently limited to 2 by this example.
#endif
    for (int i = 0; i < CONFIG_UMNI_SPI_ETHERNETS_NUM; i++)
    {
        eth_handles[eth_cnt] = eth_init_spi(&spi_eth_module_config[i], NULL, NULL);
        ESP_GOTO_ON_FALSE(eth_handles[eth_cnt], ESP_FAIL, err, TAG, "SPI Ethernet init failed");
        eth_cnt++;
    }
#endif // CONFIG_ETH_USE_SPI_ETHERNET
#else
    ESP_LOGD(TAG, "no Ethernet device selected to init");
#endif // CONFIG_UMNI_USE_INTERNAL_ETHERNET || CONFIG_UMNI_USE_SPI_ETHERNET
    *eth_handles_out = eth_handles;
    *eth_cnt_out = eth_cnt;

    return ret;
#if CONFIG_UMNI_USE_INTERNAL_ETHERNET || CONFIG_UMNI_USE_SPI_ETHERNET
err:
    free(eth_handles);
    return ret;
#endif
}

/** Event handler for Ethernet events */

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        char mac_addr_str[18];
        sprintf(
            mac_addr_str,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        esp_event_post(APP_EVENTS, EV_ETH_MAC, mac_addr_str, sizeof(mac_addr_str), portMAX_DELAY);

        break;
    case ETHERNET_EVENT_DISCONNECTED:
        // тут утечка памяти, нужно где-то что-то почистить
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

void ethernet_task(void *pvParameters)
{
    while (!success)
    {
        esp_err_t res = ESP_FAIL;
        // Initialize Ethernet driver
        uint8_t eth_port_cnt = 0;
        esp_eth_handle_t *eth_handles;
        res = ethernet_init(&eth_handles, &eth_port_cnt);

        // Create default event loop that running in background

        // Create instance(s) of esp-netif for Ethernet(s)
        if (res == ESP_OK && eth_port_cnt == 1)
        {
            // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
            // default esp-netif configuration parameters.
            esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
            esp_netif_t *eth_netif = esp_netif_new(&cfg);
            // Attach Ethernet driver to TCP/IP stack
            res = esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0]));
        }
        else
        {
            // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
            // esp-netif configuration parameters for each interface (name, priority, etc.).
            esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
            esp_netif_config_t cfg_spi = {
                .base = &esp_netif_config,
                .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
            char if_key_str[10];
            char if_desc_str[10];
            char num_str[3];
            for (int i = 0; i < eth_port_cnt; i++)
            {
                itoa(i, num_str, 10);
                strcat(strcpy(if_key_str, "ETH_"), num_str);
                strcat(strcpy(if_desc_str, "eth"), num_str);
                esp_netif_config.if_key = if_key_str;
                esp_netif_config.if_desc = if_desc_str;
                esp_netif_config.route_prio -= i * 5;
                esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

                // Attach Ethernet driver to TCP/IP stack
                res = esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i]));
            }
        }

        if (res == ESP_OK)
        {
            // Register user defined event handers
            res = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);

            // Start Ethernet driver state machine
            for (int i = 0; i < eth_port_cnt; i++)
            {
                res = esp_eth_start(eth_handles[i]);
            }

            success = true;
        }
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void ethernet_start()
{
    xTaskCreatePinnedToCore(ethernet_task, "init_ethernet", 4095, NULL, 13, &ethernet_handle, 0);
}

#if CONFIG_UMNI_WEB_DEPLOY_SEMIHOST
esp_err_t init_fs(void)
{
    esp_err_t ret = esp_vfs_semihost_register(CONFIG_UMNI_WEB_MOUNT_POINT);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WEBSERVER_TAG, "Failed to register semihost driver (%s)!", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(WEBSERVER_TAG, "Semihost register success");
    }
    return ESP_OK;
}
#endif

#if CONFIG_UMNI_WEB_DEPLOY_SD

esp_err_t init_fs(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

    slot_config.gpio_cs = CONFIG_UMNI_SD_CS;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 12 * 1000; // пониженная частота для общения с SD SPI
    slot_config.host_id = CONFIG_UMNI_ETH_SPI_HOST;
    esp_err_t res = esp_vfs_fat_sdspi_mount(CONFIG_UMNI_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (res == ESP_OK)
    {
        sdmmc_card_print_info(stdout, card);
        esp_event_post(APP_EVENTS, EV_SDCARD_MOUNTED, NULL, sizeof(NULL), portMAX_DELAY);
    }
    else
    {
        ESP_LOGE(TAG, "Faled mount SD card %s", esp_err_to_name(res));
    }
    return res;
}

#endif
