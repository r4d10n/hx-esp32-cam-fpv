idf_component_register(
    SRCS
        "test_ipc_common.c"
        "test_ipc_master.c"
        "test_ipc_slave.c"
    INCLUDE_DIRS
        "."
        "../include"
    REQUIRES
        unity
        driver
        esp_timer
)

# Add Unity test utilities
set(UNIT_TEST_SOURCES
    test_ipc_common.c
    test_ipc_master.c
    test_ipc_slave.c
)

list(APPEND COMPONENT_SRCS ${UNIT_TEST_SOURCES})
