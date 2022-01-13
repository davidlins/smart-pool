#include "esp_log.h"
#include "esp_http_server.h"

static httpd_handle_t server = NULL;

static esp_err_t health_get_handler(httpd_req_t *req);

static const httpd_uri_t health = {
    .uri = "/health",
    .method = HTTP_GET,
    .handler = health_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "OK!"};

static httpd_handle_t start_webserver(void);


static const char *TAG_WEBSERVER = "APP WEBSERVER";

/* An HTTP GET handler */
static esp_err_t health_get_handler(httpd_req_t *req)
{
    const char *resp_str = "OK";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG_WEBSERVER, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG_WEBSERVER, "Registering URI handlers");
        httpd_register_uri_handler(server, &health);
        return server;
    }

    ESP_LOGI(TAG_WEBSERVER, "Error starting server!");
    return NULL;
}

void app_webserver_start(void)
{
    ESP_LOGI(TAG_WEBSERVER, "Starting webserver");
    server = start_webserver();
}