set(srcs "fec_decoder.c")

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../../components/common"
    PRIV_REQUIRES esp_timer
)

# Add common FEC library sources (fec.cpp) to this component
# This allows us to use the Reed-Solomon codec without pulling in the entire common component
target_sources(${COMPONENT_LIB} PRIVATE
    "../../../components/common/fec.cpp"
)

# Include path for FEC library
target_include_directories(${COMPONENT_LIB} PRIVATE
    "../../../components/common"
)

# Add compile options for optimization on ESP32-S3
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -ffast-math
)
