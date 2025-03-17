

#include <time.h>

#include "BPCSender.hpp"
#include "BPCTimeSender.hpp"

#include <HardwareSerial.h>

#include "esp_sntp.h"
#include "esp_netif_sntp.h"

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "WiFiProvision.h"

BPCTimeSender bpcstation;

void setup()
{
	bpcstation.init();
	Serial.begin(115200);

	WiFiManager wm;


	wm.autoConnect();

	esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
	esp_netif_sntp_init(&config);

	sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

	esp_netif_sntp_start();

	if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
	{
		printf("Failed to update system time within 10s timeout");

		// wm.resetSettings();
		wm.reboot();
	}

	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

}

void loop()
{
	bpcstation.loop();
}
