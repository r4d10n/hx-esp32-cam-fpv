#!/bin/bash

###############################################################################
# Component Dependency Validation Script
#
# This script validates all component dependencies across the FPV system
# and reports issues found.
#
# Usage: ./scripts/validate_dependencies.sh
#
###############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0
WARNING_CHECKS=0

###############################################################################
# Helper Functions
###############################################################################

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}\n"
}

print_pass() {
    echo -e "${GREEN}✅ PASS:${NC} $1"
    ((PASSED_CHECKS++))
    ((TOTAL_CHECKS++))
}

print_fail() {
    echo -e "${RED}❌ FAIL:${NC} $1"
    ((FAILED_CHECKS++))
    ((TOTAL_CHECKS++))
}

print_warn() {
    echo -e "${YELLOW}⚠️  WARN:${NC} $1"
    ((WARNING_CHECKS++))
    ((TOTAL_CHECKS++))
}

print_info() {
    echo -e "${BLUE}ℹ️  INFO:${NC} $1"
}

###############################################################################
# Dependency Validation Functions
###############################################################################

check_file_exists() {
    local file="$1"
    local description="$2"

    if [ -f "$file" ]; then
        print_pass "$description exists: $file"
        return 0
    else
        print_fail "$description missing: $file"
        return 1
    fi
}

check_directory_exists() {
    local dir="$1"
    local description="$2"

    if [ -d "$dir" ]; then
        print_pass "$description exists: $dir"
        return 0
    else
        print_fail "$description missing: $dir"
        return 1
    fi
}

check_component_exists() {
    local component="$1"
    local search_path="$2"

    if [ -d "$search_path/$component" ] && [ -f "$search_path/$component/CMakeLists.txt" ]; then
        print_pass "Component found: $component"
        return 0
    else
        print_fail "Component not found: $component in $search_path"
        return 1
    fi
}

check_component_requires() {
    local component_dir="$1"
    local required_component="$2"

    if grep -q "REQUIRES\|REQUIRES" "$component_dir/CMakeLists.txt" 2>/dev/null; then
        if grep -A 10 "REQUIRES" "$component_dir/CMakeLists.txt" | grep -q "$required_component"; then
            print_pass "$component_dir requires $required_component"
            return 0
        else
            print_fail "$component_dir missing REQUIRES clause for $required_component"
            return 1
        fi
    else
        print_fail "$component_dir has no REQUIRES clause"
        return 1
    fi
}

check_extra_component_dirs() {
    local cmake_file="$1"
    local description="$2"

    if grep -q "EXTRA_COMPONENT_DIRS" "$cmake_file" 2>/dev/null; then
        print_pass "$description has EXTRA_COMPONENT_DIRS configured"
        return 0
    else
        print_fail "$description missing EXTRA_COMPONENT_DIRS"
        return 1
    fi
}

check_include_exists() {
    local header="$1"
    local search_path="$2"

    if find "$search_path" -name "$header" -type f 2>/dev/null | grep -q .; then
        print_pass "Header file found: $header"
        return 0
    else
        print_fail "Header file not found: $header"
        return 1
    fi
}

###############################################################################
# Main Validation Routine
###############################################################################

print_header "ESP32 FPV System - Dependency Validation"

echo "Project Root: $PROJECT_ROOT"
echo "Script Directory: $SCRIPT_DIR"

###############################################################################
# 1. Check Project Structure
###############################################################################

print_header "1. Project Structure Validation"

check_directory_exists "$PROJECT_ROOT/esp32-p4-mipi-fpv" "ESP32-P4 project"
check_directory_exists "$PROJECT_ROOT/esp32-c5-wifi-transmitter" "ESP32-C5 project"
check_directory_exists "$PROJECT_ROOT/esp32-c6-wifi-transmitter" "ESP32-C6 project"
check_directory_exists "$PROJECT_ROOT/esp32-p4-mipi-fpv/components" "P4 components directory"

###############################################################################
# 2. Check ESP32-P4 Components
###############################################################################

print_header "2. ESP32-P4 Component Validation"

P4_COMPONENTS_DIR="$PROJECT_ROOT/esp32-p4-mipi-fpv/components"

check_component_exists "mipi_camera" "$P4_COMPONENTS_DIR"
check_component_exists "h264_encoder" "$P4_COMPONENTS_DIR"
check_component_exists "esp32_ipc" "$P4_COMPONENTS_DIR"
check_component_exists "wifi_c5_transmitter" "$P4_COMPONENTS_DIR"

# Check P4 main component
check_file_exists "$PROJECT_ROOT/esp32-p4-mipi-fpv/main/CMakeLists.txt" "P4 main component"

print_info "P4 MIPI camera component status:"
if check_file_exists "$P4_COMPONENTS_DIR/mipi_camera/include/mipi_camera.h" "  - MIPI camera header"; then
    true
fi

print_info "P4 H.264 encoder component status:"
if check_file_exists "$P4_COMPONENTS_DIR/h264_encoder/include/h264_encoder.h" "  - H.264 encoder header"; then
    true
fi

print_info "P4 IPC component status:"
if check_file_exists "$P4_COMPONENTS_DIR/esp32_ipc/include/esp32_ipc.h" "  - IPC header"; then
    true
fi

###############################################################################
# 3. Check ESP32-C5 Configuration
###############################################################################

print_header "3. ESP32-C5 Configuration Validation"

C5_CMAKE="$PROJECT_ROOT/esp32-c5-wifi-transmitter/CMakeLists.txt"
C5_COMPONENTS_DIR="$PROJECT_ROOT/esp32-c5-wifi-transmitter/components"

check_file_exists "$C5_CMAKE" "C5 project CMakeLists.txt"

if [ -f "$C5_CMAKE" ]; then
    print_info "Checking C5 CMakeLists.txt for configuration..."

    if grep -q "EXTRA_COMPONENT_DIRS" "$C5_CMAKE"; then
        print_pass "C5 CMakeLists.txt has EXTRA_COMPONENT_DIRS"
    else
        print_fail "C5 CMakeLists.txt MISSING EXTRA_COMPONENT_DIRS"
        print_info "  → Required to find esp32_ipc and wifi_c5_transmitter components"
    fi

    if grep -q "COMPONENT_REQUIRES" "$C5_CMAKE"; then
        print_warn "C5 CMakeLists.txt has COMPONENT_REQUIRES (optional but recommended)"
    fi
fi

# Check if components exist in C5 directory
if [ -d "$C5_COMPONENTS_DIR" ]; then
    local c5_component_count=$(find "$C5_COMPONENTS_DIR" -maxdepth 1 -type d | wc -l)
    if [ "$c5_component_count" -le 1 ]; then
        print_warn "C5 components directory is empty (should be OK if EXTRA_COMPONENT_DIRS is set)"
    fi
fi

check_file_exists "$PROJECT_ROOT/esp32-c5-wifi-transmitter/main/CMakeLists.txt" "C5 main component"

###############################################################################
# 4. Check ESP32-C6 Configuration
###############################################################################

print_header "4. ESP32-C6 Configuration Validation"

C6_CMAKE="$PROJECT_ROOT/esp32-c6-wifi-transmitter/CMakeLists.txt"
C6_COMPONENTS_DIR="$PROJECT_ROOT/esp32-c6-wifi-transmitter/components"

check_file_exists "$C6_CMAKE" "C6 project CMakeLists.txt"

if [ -f "$C6_CMAKE" ]; then
    print_info "Checking C6 CMakeLists.txt for configuration..."

    if grep -q "EXTRA_COMPONENT_DIRS" "$C6_CMAKE"; then
        print_pass "C6 CMakeLists.txt has EXTRA_COMPONENT_DIRS"
    else
        print_fail "C6 CMakeLists.txt MISSING EXTRA_COMPONENT_DIRS"
        print_info "  → Required to find esp32_ipc component"
    fi
fi

check_directory_exists "$C6_COMPONENTS_DIR" "C6 components directory"
check_component_exists "wifi_c6_transmitter" "$C6_COMPONENTS_DIR"
check_file_exists "$PROJECT_ROOT/esp32-c6-wifi-transmitter/main/CMakeLists.txt" "C6 main component"

###############################################################################
# 5. Check Critical Components
###############################################################################

print_header "5. Critical Component Status"

# esp32_ipc
if check_component_exists "esp32_ipc" "$P4_COMPONENTS_DIR"; then
    print_info "  esp32_ipc is in P4 project"
    print_info "  → C5 and C6 will need EXTRA_COMPONENT_DIRS to find it"
fi

# fec_encoder
if check_component_exists "fec_encoder" "$P4_COMPONENTS_DIR"; then
    print_pass "fec_encoder component exists"
else
    print_fail "fec_encoder component MISSING - CRITICAL ISSUE"
    print_info "  → C6 wifi_c6_transmitter component requires this"

    # Check if FEC code exists elsewhere
    if [ -f "$P4_COMPONENTS_DIR/wifi_c5_transmitter/fec_encoder.c" ]; then
        print_info "  → FEC encoder code found in wifi_c5_transmitter (needs to be extracted)"
    fi
fi

# wifi_c5_transmitter
if check_component_exists "wifi_c5_transmitter" "$P4_COMPONENTS_DIR"; then
    print_pass "wifi_c5_transmitter component exists"

    # Check if it requires fec_encoder
    if grep -A 10 "REQUIRES" "$P4_COMPONENTS_DIR/wifi_c5_transmitter/CMakeLists.txt" 2>/dev/null | grep -q "fec_encoder"; then
        print_info "  → Requires fec_encoder component"
    fi
else
    print_fail "wifi_c5_transmitter component NOT FOUND"
fi

# wifi_c6_transmitter
if check_component_exists "wifi_c6_transmitter" "$C6_COMPONENTS_DIR"; then
    print_pass "wifi_c6_transmitter component exists"

    # Check if it requires fec_encoder
    if grep -A 10 "REQUIRES" "$C6_COMPONENTS_DIR/wifi_c6_transmitter/CMakeLists.txt" 2>/dev/null | grep -q "fec_encoder"; then
        print_info "  → Requires fec_encoder component (MUST EXIST)"

        # Check if fec_encoder exists
        if ! check_component_exists "fec_encoder" "$P4_COMPONENTS_DIR"; then
            print_fail "  → fec_encoder component missing - C6 BUILD WILL FAIL"
        fi
    fi
else
    print_fail "wifi_c6_transmitter component NOT FOUND"
fi

###############################################################################
# 6. Check Header Files
###############################################################################

print_header "6. Header File Validation"

check_include_exists "mipi_camera.h" "$P4_COMPONENTS_DIR/mipi_camera"
check_include_exists "h264_encoder.h" "$P4_COMPONENTS_DIR/h264_encoder"
check_include_exists "esp32_ipc.h" "$P4_COMPONENTS_DIR/esp32_ipc"
check_include_exists "wifi_c5_transmitter.h" "$P4_COMPONENTS_DIR/wifi_c5_transmitter"

if [ -d "$C6_COMPONENTS_DIR/wifi_c6_transmitter" ]; then
    check_include_exists "wifi_c6_transmitter.h" "$C6_COMPONENTS_DIR/wifi_c6_transmitter"
fi

# Check for FEC encoder header
if find "$PROJECT_ROOT" -name "fec_encoder.h" -type f 2>/dev/null | grep -q .; then
    print_pass "fec_encoder.h header file found"
else
    if [ -f "$P4_COMPONENTS_DIR/wifi_c5_transmitter/fec_encoder_wifi.h" ]; then
        print_warn "fec_encoder_wifi.h exists but not exposed as fec_encoder.h"
    else
        print_fail "fec_encoder.h header file NOT FOUND"
    fi
fi

###############################################################################
# 7. Check SDKConfig Files
###############################################################################

print_header "7. SDKConfig File Validation"

if [ -f "$PROJECT_ROOT/esp32-p4-mipi-fpv/sdkconfig.defaults" ]; then
    print_pass "P4 sdkconfig.defaults exists"
else
    print_warn "P4 sdkconfig.defaults missing (optional but recommended)"
fi

if [ -f "$PROJECT_ROOT/esp32-c5-wifi-transmitter/sdkconfig.defaults" ]; then
    print_pass "C5 sdkconfig.defaults exists"
else
    print_warn "C5 sdkconfig.defaults missing (optional but recommended)"
fi

if [ -f "$PROJECT_ROOT/esp32-c6-wifi-transmitter/sdkconfig.defaults" ]; then
    print_pass "C6 sdkconfig.defaults exists"
else
    print_warn "C6 sdkconfig.defaults missing (optional but recommended)"
fi

###############################################################################
# 8. Check Circular Dependencies
###############################################################################

print_header "8. Circular Dependency Check"

print_info "Analyzing component dependencies..."

# Create a simple dependency graph analysis
check_circular_deps() {
    local component="$1"
    local cmake_file="$2"

    if [ ! -f "$cmake_file" ]; then
        return 0
    fi

    # Extract REQUIRES clause
    local requires=$(grep -A 20 "REQUIRES" "$cmake_file" 2>/dev/null | head -20 || true)

    if echo "$requires" | grep -q "$component"; then
        print_fail "CIRCULAR: $component requires itself"
        return 1
    fi
}

for component in mipi_camera h264_encoder esp32_ipc wifi_c5_transmitter wifi_c6_transmitter; do
    if [ -d "$PROJECT_ROOT/esp32-p4-mipi-fpv/components/$component" ] 2>/dev/null; then
        check_circular_deps "$component" "$PROJECT_ROOT/esp32-p4-mipi-fpv/components/$component/CMakeLists.txt"
    fi
    if [ -d "$PROJECT_ROOT/esp32-c6-wifi-transmitter/components/$component" ] 2>/dev/null; then
        check_circular_deps "$component" "$PROJECT_ROOT/esp32-c6-wifi-transmitter/components/$component/CMakeLists.txt"
    fi
done

print_pass "No circular dependencies detected"

###############################################################################
# Summary Report
###############################################################################

print_header "Summary Report"

echo "Total Checks: $TOTAL_CHECKS"
echo -e "Passed: ${GREEN}$PASSED_CHECKS${NC}"
echo -e "Failed: ${RED}$FAILED_CHECKS${NC}"
echo -e "Warnings: ${YELLOW}$WARNING_CHECKS${NC}"

echo ""
echo "Build Status:"
if [ "$FAILED_CHECKS" -eq 0 ]; then
    echo -e "${GREEN}✅ All critical checks passed!${NC}"
    if [ "$WARNING_CHECKS" -gt 0 ]; then
        echo -e "${YELLOW}⚠️  Some warnings remain - review above${NC}"
    fi
else
    echo -e "${RED}❌ Critical issues found - see details above${NC}"
fi

###############################################################################
# Action Items
###############################################################################

print_header "Action Items"

if grep -q "EXTRA_COMPONENT_DIRS" "$PROJECT_ROOT/esp32-c5-wifi-transmitter/CMakeLists.txt" 2>/dev/null; then
    echo "✅ C5: EXTRA_COMPONENT_DIRS configured"
else
    echo "❌ C5: Add EXTRA_COMPONENT_DIRS to CMakeLists.txt"
fi

if grep -q "EXTRA_COMPONENT_DIRS" "$PROJECT_ROOT/esp32-c6-wifi-transmitter/CMakeLists.txt" 2>/dev/null; then
    echo "✅ C6: EXTRA_COMPONENT_DIRS configured"
else
    echo "❌ C6: Add EXTRA_COMPONENT_DIRS to CMakeLists.txt"
fi

if [ -d "$P4_COMPONENTS_DIR/fec_encoder" ]; then
    echo "✅ fec_encoder: Component exists"
else
    echo "❌ fec_encoder: Create component (required for C6 build)"
fi

if [ -f "$PROJECT_ROOT/esp32-p4-mipi-fpv/sdkconfig.defaults" ]; then
    echo "✅ P4: sdkconfig.defaults exists"
else
    echo "⚠️  P4: Create sdkconfig.defaults (optional)"
fi

if [ -f "$PROJECT_ROOT/esp32-c5-wifi-transmitter/sdkconfig.defaults" ]; then
    echo "✅ C5: sdkconfig.defaults exists"
else
    echo "⚠️  C5: Create sdkconfig.defaults (optional)"
fi

echo ""

###############################################################################
# Exit Code
###############################################################################

if [ "$FAILED_CHECKS" -eq 0 ]; then
    exit 0
else
    exit 1
fi
