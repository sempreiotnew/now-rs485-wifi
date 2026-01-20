idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_wifi json common
    EMBED_FILES 
        "html/index.html"
        "html/index.js"
)
