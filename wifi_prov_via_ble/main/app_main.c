/* SoftAP based Provisioning Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "app_prov.h"
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_http_server.h>

#define EXAMPLE_AP_RECONN_ATTEMPTS  CONFIG_EXAMPLE_AP_RECONN_ATTEMPTS
#define PROV_QR_VERSION            "v1"
#define PROV_TRANSPORT_SOFTAP      "softap"
#define QRCODE_BASE_URL            "https://espressif.github.io/esp-jumpstart/qrcode.html"

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64

static const char *TAG = "app";

static esp_err_t save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", password);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "WiFi credentials saved");
    return ESP_OK;
}

static esp_err_t load_wifi_credentials(char *ssid, char *password) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_storage", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No saved credentials found");
        return err;
    }

    size_t ssid_len = WIFI_SSID_MAX_LEN, pass_len = WIFI_PASS_MAX_LEN;
    nvs_get_str(nvs, "ssid", ssid, &ssid_len);
    nvs_get_str(nvs, "password", password, &pass_len);
    nvs_close(nvs);
    return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_AP_RECONN_ATTEMPTS) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

static void wifi_init_sta(const char *ssid, const char *password) {
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* HTTP Server Handlers */
esp_err_t get_handler(httpd_req_t *req) {
    const char *html = "<html>\
        <head><title>ESP32 WiFi Provisioning</title></head>\
        <body>\
        <h2>Enter WiFi Credentials</h2>\
        <form action='/set' method='POST'>\
            SSID: <input type='text' name='ssid'><br>\
            Password: <input type='password' name='password'><br>\
            <input type='submit' value='Submit'>\
        </form>\
        </body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t post_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    /* Extract SSID and Password */
    char ssid[WIFI_SSID_MAX_LEN] = {0}, password[WIFI_PASS_MAX_LEN] = {0};
    sscanf(buf, "ssid=%[^&]&password=%s", ssid, password);

    if (strlen(ssid) > 0 && strlen(password) > 0) {
        save_wifi_credentials(ssid, password);
        ESP_LOGI(TAG, "Received SSID: %s, Password: %s", ssid, password);
        httpd_resp_send(req, "WiFi credentials received. Rebooting...", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        httpd_resp_send(req, "Invalid input. Try again.", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

/* Start HTTP Web Server */
static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = get_handler };
        httpd_uri_t uri_post = { .uri = "/set", .method = HTTP_POST, .handler = post_handler };
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }
    else{
        ESP_LOGI(TAG, "ERRORRRRRR HEREEEEEEEEE");
    }
    return server;
}

/* Start Wi-Fi in SoftAP Mode */
static void start_softap_provisioning(void)
{
    uint8_t eth_mac[6];
    char ssid[32];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(ssid, sizeof(ssid), "eSpark_%02X%02X%02X%02X%02X%02X", eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    strcpy((char *)wifi_config.ap.password, CONFIG_EXAMPLE_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP mode started. Go to http://192.168.4.1/");
    start_webserver();
}

void app_main(void)
{
    /* Initialize networking stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    char ssid[WIFI_SSID_MAX_LEN] = {0}, password[WIFI_PASS_MAX_LEN] = {0};
    if (load_wifi_credentials(ssid, password) == ESP_OK) {
        ESP_LOGI(TAG, "Connecting to saved WiFi: %s\tPassword: %s", ssid, password);
        wifi_init_sta(ssid, password);
    } else {
        ESP_LOGI(TAG, "Starting SoftAP for provisioning");
        start_softap_provisioning();
    }
}