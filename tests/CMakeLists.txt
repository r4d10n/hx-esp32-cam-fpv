cmake_minimum_required(VERSION 3.5)

project(video_pipeline_tests)

# Test framework
include(CTest)

# Options
option(ENABLE_INTEGRATION_TESTS "Enable integration tests" ON)
option(ENABLE_PERFORMANCE_TESTS "Enable performance benchmarks" ON)
option(ENABLE_STRESS_TESTS "Enable stress tests" OFF)

# Test fixtures
add_library(test_fixtures STATIC
    fixtures/test_fixtures.c
)

target_include_directories(test_fixtures PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
)

# Main integration test executable
if(ENABLE_INTEGRATION_TESTS)
    add_executable(test_video_pipeline
        integration/test_video_pipeline.c
    )

    target_include_directories(test_video_pipeline PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    )

    target_link_libraries(test_video_pipeline
        test_fixtures
        unity  # Test framework
        m      # Math library for latency simulation
    )

    # Register test
    add_test(
        NAME VideoPipelineIntegration
        COMMAND test_video_pipeline
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Set test properties
    set_tests_properties(VideoPipelineIntegration PROPERTIES
        TIMEOUT 3600  # 1 hour timeout
        LABELS "integration"
    )
endif()

# Performance benchmark executable
if(ENABLE_PERFORMANCE_TESTS)
    add_executable(test_performance_benchmarks
        benchmarks/test_benchmarks.c
    )

    target_include_directories(test_performance_benchmarks PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    )

    target_link_libraries(test_performance_benchmarks
        test_fixtures
        unity
        m
    )

    add_test(
        NAME PerformanceBenchmarks
        COMMAND test_performance_benchmarks
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    set_tests_properties(PerformanceBenchmarks PROPERTIES
        TIMEOUT 1800  # 30 minutes timeout
        LABELS "benchmark"
    )
endif()

# Stress test executable
if(ENABLE_STRESS_TESTS)
    add_executable(test_stress
        stress/test_stress.c
    )

    target_include_directories(test_stress PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    )

    target_link_libraries(test_stress
        test_fixtures
        unity
        m
    )

    add_test(
        NAME StressTests
        COMMAND test_stress
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    set_tests_properties(StressTests PROPERTIES
        TIMEOUT 7200  # 2 hours timeout
        LABELS "stress"
    )
endif()

# Test summary target
add_custom_target(test-summary
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_video_pipeline
    COMMENT "Run all integration tests with verbose output"
)

# Generate test report
add_custom_target(test-report
    COMMAND ${CMAKE_CTEST_COMMAND} -T Test --output-on-failure
    COMMENT "Generate test report"
)

# Print test configuration
message(STATUS "Integration Tests Configuration:")
message(STATUS "  Integration Tests: ${ENABLE_INTEGRATION_TESTS}")
message(STATUS "  Performance Tests: ${ENABLE_PERFORMANCE_TESTS}")
message(STATUS "  Stress Tests: ${ENABLE_STRESS_TESTS}")
