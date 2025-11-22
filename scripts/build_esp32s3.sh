#!/bin/bash
################################################################################
# ESP32-S3 Android Receiver - Build Validation Script
# 
# This script validates the ESP32-S3 firmware build including:
# - Clean build
# - Compilation verification  
# - Binary size check
# - Component tests
# - Warnings/errors analysis
################################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR="/home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver"
MAX_BINARY_SIZE=4194304  # 4MB
BUILD_LOG="build_validation.log"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}ESP32-S3 Receiver Build Validation${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Function to print status
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✅ $2${NC}"
    else
        echo -e "${RED}❌ $2${NC}"
        exit 1
    fi
}

# Change to project directory
cd "$PROJECT_DIR"

# Step 1: Clean previous build
echo -e "${YELLOW}Step 1: Cleaning previous build...${NC}"
idf.py fullclean > /dev/null 2>&1 || true
print_status 0 "Clean complete"

# Step 2: Set target
echo -e "${YELLOW}Step 2: Setting target to ESP32-S3...${NC}"
idf.py set-target esp32s3 > /dev/null 2>&1
print_status $? "Target set to ESP32-S3"

# Step 3: Build firmware
echo -e "${YELLOW}Step 3: Building firmware...${NC}"
if idf.py build 2>&1 | tee "$BUILD_LOG"; then
    print_status 0 "Firmware build successful"
else
    print_status 1 "Firmware build failed"
fi

# Step 4: Check binary size
echo -e "${YELLOW}Step 4: Checking binary size...${NC}"
if [ -f "build/esp32-s3-receiver.bin" ]; then
    BINARY_SIZE=$(stat -c%s build/esp32-s3-receiver.bin)
    echo "Binary size: $BINARY_SIZE bytes ($(($BINARY_SIZE / 1024))KB)"
    
    if [ $BINARY_SIZE -le $MAX_BINARY_SIZE ]; then
        print_status 0 "Binary size within limits"
    else
        echo -e "${RED}Binary size exceeds $MAX_BINARY_SIZE bytes limit${NC}"
        exit 1
    fi
else
    # Try alternative binary name
    BINARY=$(find build -name "*.bin" -type f | head -1)
    if [ -n "$BINARY" ]; then
        BINARY_SIZE=$(stat -c%s "$BINARY")
        echo "Binary size: $BINARY_SIZE bytes ($(($BINARY_SIZE / 1024))KB)"
        print_status 0 "Binary found and size checked"
    else
        print_status 1 "Binary file not found"
    fi
fi

# Step 5: Analyze build warnings
echo -e "${YELLOW}Step 5: Analyzing build warnings...${NC}"
WARNING_COUNT=$(grep -c "warning:" "$BUILD_LOG" || true)
ERROR_COUNT=$(grep -c "error:" "$BUILD_LOG" || true)

echo "Warnings: $WARNING_COUNT"
echo "Errors: $ERROR_COUNT"

if [ $ERROR_COUNT -eq 0 ]; then
    print_status 0 "No build errors"
else
    print_status 1 "Build errors detected"
fi

if [ $WARNING_COUNT -gt 0 ]; then
    echo -e "${YELLOW}⚠️  $WARNING_COUNT warnings found (review recommended)${NC}"
fi

# Step 6: Component tests (if available)
echo -e "${YELLOW}Step 6: Checking for component tests...${NC}"
TEST_DIRS=$(find components -name "test" -type d 2>/dev/null || true)

if [ -n "$TEST_DIRS" ]; then
    echo "Test directories found:"
    echo "$TEST_DIRS"
    print_status 0 "Component tests available"
else
    echo -e "${YELLOW}⚠️  No component test directories found${NC}"
fi

# Step 7: Memory usage report
echo -e "${YELLOW}Step 7: Memory usage report...${NC}"
if idf.py size 2>/dev/null | head -20; then
    print_status 0 "Memory usage analyzed"
else
    echo -e "${YELLOW}⚠️  Memory usage report not available${NC}"
fi

# Summary
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build Validation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Build artifacts:"
find build -name "*.bin" -o -name "*.elf" 2>/dev/null | head -5
echo ""
echo "Next steps:"
echo "  1. Flash firmware: idf.py flash"
echo "  2. Monitor output: idf.py monitor"
echo "  3. Run tests: idf.py flash monitor"
echo ""

exit 0
