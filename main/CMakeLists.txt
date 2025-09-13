idf_component_register(
	SRCS "bytecode.c" "main.c" "server.c" "strip.c" "wifi.c"
	INCLUDE_DIRS "."
	PRIV_REQUIRES "esp_http_server" "esp_wifi" "nvs_flash"
	EMBED_FILES "files/favicon.ico" "files/index.html" "files/ops.h")
