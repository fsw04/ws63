#include "wifi_provision.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "securec.h"
#include "soc_osal.h"
#include "../model/watch_model.h"
#include "wifi_device.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "wifi_task.h"

#define WIFI_PROV_AP_CHANNEL 6
#define WIFI_PROV_TASK_STACK_SIZE 0x2800
#define WIFI_PROV_TASK_PRIORITY 24
#define WIFI_PROV_HTTP_BUF_SIZE 1536
#define WIFI_PROV_HTTP_BACKLOG 2
#define WIFI_PROV_SOCKET_TIMEOUT_SEC 3
#define WIFI_PROV_RSSI_DEFAULT (-42)

static volatile uint8_t g_prov_task_running = 0;
static volatile uint8_t g_prov_stop_requested = 0;
static volatile uint8_t g_prov_ap_started = 0;
static int g_prov_listen_fd = -1;
static int g_prov_client_fd = -1;

static const char g_wifi_prov_index_html[] =
    "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
    "<title>WS63 WiFi Setup</title>"
    "<style>"
    ":root{--bg:#f7fbf8;--ink:#17201b;--muted:#65736b;--line:#dce8e1;"
    "--panel:#fff;--accent:#0f8f7a;--accent2:#ff7b54;--soft:#eef8f4;}"
    "*{box-sizing:border-box}body{margin:0;min-height:100vh;font-family:Arial,"
    "Helvetica,sans-serif;background:linear-gradient(135deg,#f7fbf8 0%,#eaf6f0 54%,"
    "#fff3ed 100%);color:var(--ink);display:flex;align-items:center;"
    "justify-content:center;padding:24px}.panel{width:min(100%,420px);background:"
    "var(--panel);border:1px solid var(--line);border-radius:8px;box-shadow:0 18px "
    "45px rgba(31,55,43,.14);padding:24px}.top{display:flex;align-items:center;"
    "gap:12px;margin-bottom:18px}.mark{width:44px;height:44px;border-radius:8px;"
    "background:linear-gradient(135deg,var(--accent),#18b596);position:relative;"
    "box-shadow:0 10px 22px rgba(15,143,122,.26)}.mark:before{content:\"\";"
    "position:absolute;inset:12px;border:3px solid #fff;border-top-color:transparent;"
    "border-left-color:transparent;border-radius:50%;transform:rotate(-45deg)}"
    "h1{font-size:24px;line-height:1.15;margin:0}.sub{margin:4px 0 0;color:var(--muted);"
    "font-size:13px}.status{display:grid;grid-template-columns:1fr 1fr;gap:8px;"
    "margin:18px 0}.pill{background:var(--soft);border:1px solid #d8eee5;"
    "border-radius:8px;padding:10px}.pill b{display:block;font-size:11px;"
    "letter-spacing:.04em;color:var(--muted);text-transform:uppercase;margin-bottom:5px}"
    ".pill span{font-size:14px;font-weight:700;word-break:break-all}.field{margin:14px 0}"
    "label{display:block;font-size:13px;font-weight:700;margin:0 0 7px}input{width:100%;"
    "height:48px;border:1px solid var(--line);border-radius:8px;padding:0 14px;"
    "font-size:16px;color:var(--ink);background:#fbfdfc;outline:none}input:focus{border-color:"
    "var(--accent);box-shadow:0 0 0 3px rgba(15,143,122,.14)}button{width:100%;height:48px;"
    "border:0;border-radius:8px;background:var(--accent);color:#fff;font-size:16px;"
    "font-weight:800;margin-top:8px;box-shadow:0 10px 22px rgba(15,143,122,.22)}"
    "button:disabled{opacity:.72}.hint{font-size:12px;color:var(--muted);line-height:1.5;"
    "margin:14px 0 0}.msg{min-height:22px;margin-top:14px;font-size:13px;font-weight:700}"
    ".ok{color:var(--accent)}.error{color:var(--accent2)}"
    "@media(max-width:360px){body{padding:14px}.panel{padding:18px}h1{font-size:22px}"
    ".status{grid-template-columns:1fr}}"
    "</style></head><body><main class=\"panel\"><div class=\"top\"><div class=\"mark\"></div>"
    "<div><h1>WS63 WiFi Setup</h1><p class=\"sub\">SoftAP provisioning portal</p></div></div>"
    "<div class=\"status\"><div class=\"pill\"><b>AP SSID</b><span>" WIFI_PROV_AP_SSID
    "</span></div><div class=\"pill\"><b>Device IP</b><span>" WIFI_PROV_AP_IP "</span></div></div>"
    "<form method=\"post\" action=\"" WIFI_PROV_HTTP_PATH "\"><div class=\"field\">"
    "<label for=\"ssid\">Router SSID</label><input id=\"ssid\" name=\"ssid\" maxlength=\"32\" "
    "autocomplete=\"off\" required></div><div class=\"field\"><label for=\"password\">"
    "Router Password</label><input id=\"password\" name=\"password\" maxlength=\"64\" "
    "type=\"password\" autocomplete=\"current-password\"></div><button type=\"submit\">"
    "Save and Connect</button><p class=\"msg\" role=\"status\"></p></form>"
    "<p class=\"hint\">After saving, WS63 will stop this setup AP and connect to the router.</p>"
    "</main><script>"
    "const f=document.querySelector('form'),m=document.querySelector('.msg'),b=document.querySelector('button');"
    "f.addEventListener('submit',async e=>{e.preventDefault();m.className='msg';"
    "m.textContent='Sending...';b.disabled=true;try{const r=await fetch('" WIFI_PROV_HTTP_PATH "',"
    "{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:"
    "f.ssid.value.trim(),password:f.password.value})});const j=await r.json();if(j.ok){"
    "m.className='msg ok';m.textContent='Saved. WS63 is connecting to your router.';}else{"
    "throw new Error(j.error||'Provision failed');}}catch(err){m.className='msg error';"
    "m.textContent=err.message||'Request failed';b.disabled=false;}});"
    "</script></body></html>";

const char *wifi_provision_get_ap_ssid(void)
{
    return WIFI_PROV_AP_SSID;
}

const char *wifi_provision_get_ap_password(void)
{
    return WIFI_PROV_AP_PASSWORD;
}

const char *wifi_provision_get_ap_ip(void)
{
    return WIFI_PROV_AP_IP;
}

bool wifi_provision_is_running(void)
{
    return (g_prov_task_running != 0) && (g_prov_stop_requested == 0);
}

static void wifi_provision_close_fd(int *fd)
{
    if ((fd != NULL) && (*fd >= 0)) {
        (void)lwip_close(*fd);
        *fd = -1;
    }
}

static void wifi_provision_softap_stop(void)
{
    struct netif *netif_p = netifapi_netif_find("ap0");

    if (netif_p != NULL) {
        (void)netifapi_dhcps_stop(netif_p);
    }
    if ((g_prov_ap_started != 0) || (wifi_is_softap_enabled() != 0)) {
        (void)wifi_softap_disable();
    }
    g_prov_ap_started = 0;
}

void wifi_provision_stop(void)
{
    g_prov_stop_requested = 1;
    wifi_provision_close_fd(&g_prov_client_fd);
    wifi_provision_close_fd(&g_prov_listen_fd);
    wifi_provision_softap_stop();
}

static errcode_t wifi_provision_softap_start(void)
{
    softap_config_stru hapd_conf = {0};
    struct netif *netif_p = NULL;
    ip4_addr_t st_gw = {0};
    ip4_addr_t st_ipaddr = {0};
    ip4_addr_t st_netmask = {0};

    watch_model_set_wifi(WATCH_LINK_CONNECTING, WIFI_PROV_AP_SSID, "--", WIFI_PROV_RSSI_DEFAULT);
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "starting provision AP");

    while ((wifi_is_wifi_inited() == 0) && (g_prov_stop_requested == 0)) {
        osal_msleep(100);
    }
    if (g_prov_stop_requested != 0) {
        return ERRCODE_FAIL;
    }

    (void)wifi_sta_disconnect();
    (void)wifi_sta_disable();
    wifi_provision_softap_stop();

    if (memcpy_s(hapd_conf.ssid, sizeof(hapd_conf.ssid), WIFI_PROV_AP_SSID,
                 strlen(WIFI_PROV_AP_SSID) + 1) != EOK) {
        return ERRCODE_FAIL;
    }
    if (memcpy_s(hapd_conf.pre_shared_key, sizeof(hapd_conf.pre_shared_key), WIFI_PROV_AP_PASSWORD,
                 strlen(WIFI_PROV_AP_PASSWORD) + 1) != EOK) {
        return ERRCODE_FAIL;
    }
    hapd_conf.security_type = WIFI_SEC_TYPE_WPA2_WPA_PSK_MIX;
    hapd_conf.channel_num = WIFI_PROV_AP_CHANNEL;

    if (wifi_softap_enable(&hapd_conf) != ERRCODE_SUCC) {
        watch_model_set_wifi(WATCH_LINK_ERROR, WIFI_PROV_AP_SSID, "--", WIFI_PROV_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision AP enable failed");
        return ERRCODE_FAIL;
    }

    IP4_ADDR(&st_ipaddr, 192, 168, 63, 1);
    IP4_ADDR(&st_netmask, 255, 255, 255, 0);
    IP4_ADDR(&st_gw, 192, 168, 63, 1);

    netif_p = netifapi_netif_find("ap0");
    if (netif_p == NULL) {
        (void)wifi_softap_disable();
        watch_model_set_wifi(WATCH_LINK_ERROR, WIFI_PROV_AP_SSID, "--", WIFI_PROV_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision AP netif missing");
        return ERRCODE_FAIL;
    }
    if (netifapi_netif_set_addr(netif_p, &st_ipaddr, &st_netmask, &st_gw) != ERR_OK) {
        (void)wifi_softap_disable();
        watch_model_set_wifi(WATCH_LINK_ERROR, WIFI_PROV_AP_SSID, "--", WIFI_PROV_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision AP address failed");
        return ERRCODE_FAIL;
    }
    if (netifapi_dhcps_start(netif_p, NULL, 0) != ERR_OK) {
        (void)wifi_softap_disable();
        watch_model_set_wifi(WATCH_LINK_ERROR, WIFI_PROV_AP_SSID, "--", WIFI_PROV_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision DHCP failed");
        return ERRCODE_FAIL;
    }

    g_prov_ap_started = 1;
    watch_model_set_wifi(WATCH_LINK_BROADCASTING, WIFI_PROV_AP_SSID, WIFI_PROV_AP_IP, WIFI_PROV_RSSI_DEFAULT);
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "provision AP ready");
    osal_printk("[WIFI_PROV] AP %s ready at %s\r\n", WIFI_PROV_AP_SSID, WIFI_PROV_AP_IP);
    return ERRCODE_SUCC;
}

static void wifi_provision_send_response(int fd, int code, const char *status,
                                         const char *content_type, const char *body)
{
    char header[320] = {0};
    const char *payload = (body == NULL) ? "" : body;
    int header_len;

    header_len = snprintf_s(header, sizeof(header), sizeof(header) - 1,
                            "HTTP/1.1 %d %s\r\n"
                            "Content-Type: %s\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Access-Control-Allow-Headers: Content-Type\r\n"
                            "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
                            "Connection: close\r\n"
                            "Content-Length: %u\r\n"
                            "\r\n",
                            code, status, content_type, (unsigned int)strlen(payload));
    if (header_len > 0) {
        (void)send(fd, header, (size_t)header_len, 0);
    }
    if (payload[0] != '\0') {
        (void)send(fd, payload, strlen(payload), 0);
    }
}

static void wifi_provision_send_json(int fd, int code, const char *status, const char *body)
{
    wifi_provision_send_response(fd, code, status, "application/json", body);
}

static int wifi_provision_hex_value(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    if ((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }
    return -1;
}

static bool wifi_provision_url_decode(const char *src, char *dst, size_t dst_len)
{
    size_t out = 0;

    if ((src == NULL) || (dst == NULL) || (dst_len == 0)) {
        return false;
    }

    while ((*src != '\0') && (*src != '&')) {
        char c = *src++;

        if (out >= (dst_len - 1)) {
            return false;
        }
        if (c == '+') {
            dst[out++] = ' ';
        } else if ((c == '%') && (src[0] != '\0') && (src[1] != '\0')) {
            int high = wifi_provision_hex_value(src[0]);
            int low = wifi_provision_hex_value(src[1]);
            if ((high < 0) || (low < 0)) {
                return false;
            }
            dst[out++] = (char)((high << 4) | low);
            src += 2;
        } else {
            dst[out++] = c;
        }
    }
    dst[out] = '\0';
    return true;
}

static bool wifi_provision_extract_form_value(const char *body, const char *key, char *out, size_t out_len)
{
    size_t key_len;
    const char *p = body;

    if ((body == NULL) || (key == NULL) || (out == NULL) || (out_len == 0)) {
        return false;
    }

    key_len = strlen(key);
    while ((p != NULL) && (*p != '\0')) {
        if ((strncmp(p, key, key_len) == 0) && (p[key_len] == '=')) {
            return wifi_provision_url_decode(p + key_len + 1, out, out_len);
        }
        p = strchr(p, '&');
        if (p != NULL) {
            p++;
        }
    }
    return false;
}

static bool wifi_provision_extract_json_string(const char *body, const char *key, char *out, size_t out_len)
{
    char pattern[40] = {0};
    const char *p;
    size_t out_pos = 0;

    if ((body == NULL) || (key == NULL) || (out == NULL) || (out_len == 0)) {
        return false;
    }
    if (snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "\"%s\"", key) <= 0) {
        return false;
    }

    p = strstr(body, pattern);
    if (p == NULL) {
        return false;
    }
    p = strchr(p + strlen(pattern), ':');
    if (p == NULL) {
        return false;
    }
    p++;
    while ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++;

    while ((*p != '\0') && (*p != '"')) {
        char c = *p++;

        if (out_pos >= (out_len - 1)) {
            return false;
        }
        if ((c == '\\') && (*p != '\0')) {
            c = *p++;
        }
        out[out_pos++] = c;
    }
    if (*p != '"') {
        return false;
    }
    out[out_pos] = '\0';
    return true;
}

static bool wifi_provision_extract_credentials(const char *request, const char *body,
                                               char *ssid, size_t ssid_len,
                                               char *password, size_t password_len)
{
    bool has_ssid = false;
    bool has_password = false;

    (void)request;
    has_ssid = wifi_provision_extract_json_string(body, "ssid", ssid, ssid_len);
    has_password = wifi_provision_extract_json_string(body, "password", password, password_len);
    if (!has_password) {
        has_password = wifi_provision_extract_json_string(body, "pwd", password, password_len);
    }

    if (!has_ssid) {
        has_ssid = wifi_provision_extract_form_value(body, "ssid", ssid, ssid_len);
    }
    if (!has_password) {
        has_password = wifi_provision_extract_form_value(body, "password", password, password_len);
    }
    if (!has_password) {
        has_password = wifi_provision_extract_form_value(body, "pwd", password, password_len);
    }

    if (!has_password) {
        password[0] = '\0';
        has_password = true;
    }
    return has_ssid && has_password && (ssid[0] != '\0');
}

static int wifi_provision_content_length(const char *request)
{
    const char *p = strstr(request, "Content-Length:");

    if (p == NULL) {
        p = strstr(request, "content-length:");
    }
    if (p == NULL) {
        return 0;
    }
    p += strlen("Content-Length:");
    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }
    return (int)strtol(p, NULL, 10);
}

static int wifi_provision_read_request(int fd, char *buf, size_t buf_len)
{
    int total = 0;

    if ((buf == NULL) || (buf_len == 0)) {
        return -1;
    }

    while (total < (int)(buf_len - 1)) {
        int received = recv(fd, buf + total, (size_t)((int)(buf_len - 1) - total), 0);
        char *body;

        if (received <= 0) {
            break;
        }
        total += received;
        buf[total] = '\0';

        body = strstr(buf, "\r\n\r\n");
        if (body != NULL) {
            int header_len = (int)(body + 4 - buf);
            int content_len = wifi_provision_content_length(buf);
            if ((content_len <= 0) || (total >= (header_len + content_len))) {
                break;
            }
        }
    }

    if (total <= 0) {
        return -1;
    }
    buf[total] = '\0';
    return total;
}

static bool wifi_provision_handle_post(int fd, const char *request, const char *body)
{
    char ssid[WIFI_MAX_SSID_LEN] = {0};
    char password[WIFI_MAX_KEY_LEN] = {0};
    errcode_t ret;

    if (!wifi_provision_extract_credentials(request, body, ssid, sizeof(ssid), password, sizeof(password))) {
        wifi_provision_send_json(fd, 400, "Bad Request", "{\"ok\":false,\"error\":\"missing ssid\"}");
        return false;
    }

    wifi_provision_send_json(fd, 200, "OK", "{\"ok\":true,\"state\":\"connecting\"}");
    wifi_provision_close_fd(&g_prov_client_fd);

    ret = wifi_task_request_connect(ssid, password);
    if (ret == ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "provision credentials received");
        g_prov_stop_requested = 1;
        wifi_provision_close_fd(&g_prov_listen_fd);
    } else {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision connect request failed");
    }
    return true;
}

static void wifi_provision_handle_client(int fd)
{
    char request[WIFI_PROV_HTTP_BUF_SIZE] = {0};
    char *body = NULL;
    struct timeval timeout = {WIFI_PROV_SOCKET_TIMEOUT_SEC, 0};

    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (wifi_provision_read_request(fd, request, sizeof(request)) <= 0) {
        return;
    }

    body = strstr(request, "\r\n\r\n");
    if (body != NULL) {
        body += 4;
    } else {
        body = "";
    }

    if (strncmp(request, "OPTIONS ", strlen("OPTIONS ")) == 0) {
        wifi_provision_send_response(fd, 204, "No Content", "text/plain", "");
    } else if (strncmp(request, "GET /status", strlen("GET /status")) == 0) {
        wifi_provision_send_json(fd, 200, "OK",
                                 "{\"ok\":true,\"state\":\"ap\",\"ssid\":\"" WIFI_PROV_AP_SSID
                                 "\",\"password\":\"" WIFI_PROV_AP_PASSWORD
                                 "\",\"ip\":\"" WIFI_PROV_AP_IP "\",\"provision\":\"" WIFI_PROV_HTTP_PATH "\"}");
    } else if ((strncmp(request, "GET / ", strlen("GET / ")) == 0) ||
               (strncmp(request, "GET / HTTP", strlen("GET / HTTP")) == 0)) {
        wifi_provision_send_response(fd, 200, "OK", "text/html; charset=utf-8",
                                     g_wifi_prov_index_html);
    } else if (strncmp(request, "POST " WIFI_PROV_HTTP_PATH, strlen("POST " WIFI_PROV_HTTP_PATH)) == 0) {
        if (wifi_provision_handle_post(fd, request, body)) {
            return;
        }
    } else {
        wifi_provision_send_json(fd, 404, "Not Found", "{\"ok\":false,\"error\":\"not found\"}");
    }
}

static errcode_t wifi_provision_http_server(void)
{
    int listen_fd;
    int reuse = 1;
    struct sockaddr_in addr = {0};
    struct timeval timeout = {WIFI_PROV_SOCKET_TIMEOUT_SEC, 0};

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision socket failed");
        return ERRCODE_FAIL;
    }

    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    (void)setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WIFI_PROV_HTTP_PORT);
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        (void)lwip_close(listen_fd);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision bind failed");
        return ERRCODE_FAIL;
    }
    if (listen(listen_fd, WIFI_PROV_HTTP_BACKLOG) != 0) {
        (void)lwip_close(listen_fd);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision listen failed");
        return ERRCODE_FAIL;
    }

    g_prov_listen_fd = listen_fd;
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "provision HTTP ready");

    while (g_prov_stop_requested == 0) {
        struct sockaddr_in client_addr = {0};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (g_prov_stop_requested != 0) {
                break;
            }
            continue;
        }

        g_prov_client_fd = client_fd;
        wifi_provision_handle_client(client_fd);
        wifi_provision_close_fd(&g_prov_client_fd);
    }

    wifi_provision_close_fd(&g_prov_listen_fd);
    return ERRCODE_SUCC;
}

static void *wifi_provision_main_task(const char *arg)
{
    (void)arg;

    if (wifi_provision_softap_start() == ERRCODE_SUCC) {
        (void)wifi_provision_http_server();
    }

    wifi_provision_stop();
    g_prov_task_running = 0;
    g_prov_stop_requested = 0;
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "provision AP stopped");
    return NULL;
}

errcode_t wifi_provision_start(void)
{
    osal_task *task_handle = NULL;

    if (g_prov_task_running != 0) {
        return ERRCODE_SUCC;
    }

    g_prov_stop_requested = 0;
    g_prov_task_running = 1;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)wifi_provision_main_task, 0,
                                      "WifiProvTask", WIFI_PROV_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WIFI_PROV_TASK_PRIORITY);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();

    if (task_handle == NULL) {
        g_prov_task_running = 0;
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "provision task create failed");
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}
