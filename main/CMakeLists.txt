set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES "esp_wifi" "nvs_flash")

set(COMPONENT_SRCS "main.c" "server.c" "strip.c" "wifi.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()
