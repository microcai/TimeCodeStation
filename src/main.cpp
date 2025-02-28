

#include <time.h>

#include "BPCSender.hpp"
#include "BPCTimeSender.hpp"

#include <HardwareSerial.h>

#include "esp_sntp.h"
#include "esp_netif_sntp.h"

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

BPCTimeSender bpcstation;

void setup()
{
	Serial.begin(115200);

	WiFiManager wm;

	wm.autoConnect();

	esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
	esp_netif_sntp_init(&config);

	esp_netif_sntp_start();

	if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
	{
		printf("Failed to update system time within 10s timeout");

		// wm.resetSettings();
		wm.reboot();
	}
}

void loop()
{
	bpcstation.loop();
}
