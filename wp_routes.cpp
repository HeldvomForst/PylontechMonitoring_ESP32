#include "wp_routes.h"
#include <esp_http_server.h>
#include <SPIFFS.h>
#include <esp_log.h>

// === API-Module einbinden ===
#include "web/api_core.h"
#include "web/console_api.h"
#include "web/api_combined.h"
#include "web/pwr_api.h"
#include "web/bat_api.h"
#include "web/stat_api.h"
#include "web/wp_health_api.h"
#include "web/framedump_api.h"
#include "web/wp_connect_api.h"
#include "web/service_api.h"
#include "web/filemanager_api.h"

static const char *TAG = "HTTPD";
httpd_handle_t server = NULL;

// ------------------------------------------------------------
// MIME-Type bestimmen
// ------------------------------------------------------------
static const char* getContentType(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".json")) return "application/json";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg"))  return "image/jpeg";
    return "application/octet-stream";
}

// ------------------------------------------------------------
// STATIC FILE HANDLER (Catch-all)
// ------------------------------------------------------------
esp_err_t static_file_handler(httpd_req_t *req)
{
    // API-Routen NICHT vom static handler bedienen
    if (strncmp(req->uri, "/api/", 5) == 0) {
        return ESP_FAIL;   // API soll weiter zum API-Handler gehen
    }

    String path = req->uri;

    int q = path.indexOf('?');
    if (q >= 0) path = path.substring(0, q);

    // Root → Layout
    if (path == "/") {
        path = "/layout.html";
    }

    // Dashboard-Link aus der Sidebar → ebenfalls Layout
    if (path == "/dashboard") {
        path = "/layout.html";
    }

    // Sidebar-Seiten → Layout laden
    if (path == "/dashboard" ||
        path == "/basevalue" ||
        path == "/celldata" ||
        path == "/statistic" ||
        path == "/console" ||
        path == "/connection" ||
        path == "/log" ||
        path == "/health" ||
        path == "/framedump") {

        path = "/layout.html";
    }

    File f = SPIFFS.open(path, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // MIME-Type
    if (path.endsWith(".html")) httpd_resp_set_type(req, "text/html");
    else if (path.endsWith(".js")) httpd_resp_set_type(req, "application/javascript");
    else if (path.endsWith(".css")) httpd_resp_set_type(req, "text/css");
    else if (path.endsWith(".json")) httpd_resp_set_type(req, "application/json");
    else if (path.endsWith(".png")) httpd_resp_set_type(req, "image/png");
    else httpd_resp_set_type(req, "text/plain");

    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");

    // Chunked-Response
    uint8_t buf[128];
    while (true) {
        size_t n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        httpd_resp_send_chunk(req, (const char*)buf, n);
    }

    httpd_resp_send_chunk(req, NULL, 0);
    f.close();
    return ESP_OK;
}


// ------------------------------------------------------------
// HTTPD starten
// ------------------------------------------------------------
void startHttpd() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port       = 80;
    config.max_uri_handlers  = 64;
    config.stack_size        = 6144;

    // FIX: mehrere gleichzeitige Verbindungen erlauben
    config.max_open_sockets  = 8;

    config.keep_alive_enable = false;
    config.lru_purge_enable  = true;
    config.recv_wait_timeout = 30;
    config.send_wait_timeout = 30;
    config.uri_match_fn      = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server");

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        server = NULL;
        return;
    }

    registerConsoleAPI();
    registerCombinedAPI();
    registerPwrAPI();
    registerBatAPI();
    registerStatAPI();
    registerHealthAPI();
    registerFramedumpApi();
    registerConnectAPI();
    registerServiceAPI();
    registerFilemanagerAPI();

    httpd_uri_t all = {
        .uri      = "/*",
        .method   = HTTP_GET,
        .handler  = static_file_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &all);

    ESP_LOGI(TAG, "HTTP server started");
}



