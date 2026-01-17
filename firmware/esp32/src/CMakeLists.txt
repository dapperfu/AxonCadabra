idf_component_register(
    SRCS
        "main.c"
        "ax_log.c"
        "ax_control.c"
    INCLUDE_DIRS
        "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
