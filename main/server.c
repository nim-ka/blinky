#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_http_server.h"

#include "bytecode.h"
#include "wifi.h"
#include "server.h"

#define SEND_FILE(filename) \
	extern const uint8_t filename ## _start[] asm("_binary_" #filename "_start"); \
	extern const uint8_t filename ## _end[] asm("_binary_" #filename "_end"); \
	httpd_resp_send(req, (const char *) filename ## _start, (size_t) (filename ## _end - filename ## _start));

static uint8_t sNewBytecode[BC_MAX_LEN];

static esp_err_t server_favicon_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_set_status(req, "200 OK");

	SEND_FILE(favicon_ico);
	return ESP_OK;
}

static esp_err_t server_index_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/html");
	httpd_resp_set_status(req, "200 OK");

	SEND_FILE(index_html);
	return ESP_OK;
}

static esp_err_t server_ops_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/plain");
	httpd_resp_set_status(req, "200 OK");

	SEND_FILE(ops_h);
	return ESP_OK;
}

static esp_err_t server_bytecode_get_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "application/octet-stream");
	httpd_resp_set_status(req, "200 OK");

	httpd_resp_send(req, (char *) gBytecode, gBytecodeLen);
	return ESP_OK;
}

static esp_err_t server_bytecode_put_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/plain");

	size_t len = req->content_len;
	size_t cur = 0;

	if (len > BC_MAX_LEN) {
		httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Exceeded max bytecode length");
		return ESP_FAIL;
	}

	memset(sNewBytecode, 0xFF, sizeof(sNewBytecode));

	while (cur < len) {
		cur += httpd_req_recv(req, (char *) &sNewBytecode[cur], len - cur);
	}

	if (!bc_try_update(sNewBytecode)) {
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bytecode checksum verification fail");
		return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Updated bytecode successfully");
	return ESP_OK;
}

void server_init(void) {
	wifi_init();
}

void server_start(void) {
	httpd_config_t httpdCfg = HTTPD_DEFAULT_CONFIG();
	httpdCfg.stack_size = SERVER_TASK_STACK_SIZE_BYTES;
	httpdCfg.task_priority = SERVER_TASK_PRIORITY;
	httpdCfg.core_id = SERVER_TASK_CORE;

	httpd_handle_t server;
	httpd_start(&server, &httpdCfg);

	httpd_uri_t uris[] = {
		{
			.uri = "/favicon.ico",
			.method = HTTP_GET,
			.handler = server_favicon_handler
		},
		{
			.uri = "/",
			.method = HTTP_GET,
			.handler = server_index_handler
		},
		{
			.uri = "/ops.h",
			.method = HTTP_GET,
			.handler = server_ops_handler
		},
		{
			.uri = "/bytecode.bin",
			.method = HTTP_GET,
			.handler = server_bytecode_get_handler
		},
		{
			.uri = "/bytecode.bin",
			.method = HTTP_PUT,
			.handler = server_bytecode_put_handler
		}
	};

	for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
		httpd_register_uri_handler(server, &uris[i]);
	}
}
