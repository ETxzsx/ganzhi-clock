idf_component_register(
    SRCS "app_main.c" "web_server.c" "mini_json.c" "dns_portal.c" "mdns_mini.c"
         "gz_core.c" "xl9555.c" "ssd1306.c" "st7789.c" "keys.c" "ui.c"
    INCLUDE_DIRS "."
    EMBED_FILES "../www/index.html" "../www/setup.html" "../www/core.js" "../www/astro_tables.js" "../www/liuyao.js"
)