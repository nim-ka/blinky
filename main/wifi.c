#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "sdkconfig.h"

#include "wifi.h"

void wifi_init(void) {
	nvs_flash_init();

	esp_event_loop_create_default();

	esp_netif_init();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t wifiInitCfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&wifiInitCfg);

	wifi_config_t wifiCfg = {
		.ap = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASS,
			.channel = CONFIG_WIFI_CHANNEL,
			.max_connection = 5,
			.authmode = WIFI_AUTH_WPA2_PSK
		}
	};
	esp_wifi_set_mode(WIFI_MODE_AP);
	esp_wifi_set_config(WIFI_IF_AP, &wifiCfg);

	esp_wifi_start();
}
