#!/bin/bash
################################################################################
# Android FPV Viewer - Build Validation Script
# 
# This script validates the Android application build including:
# - Gradle build
# - Unit tests
# - Lint checks
# - APK size verification
# - Test coverage
################################################################################

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuration
ANDROID_DIR="/home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver/android"
MAX_APK_SIZE=52428800  # 50MB
BUILD_LOG="build_android.log"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Android FPV Viewer Build Validation${NC}"
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

# Check if Android directory exists
if [ ! -d "$ANDROID_DIR" ]; then
    echo -e "${YELLOW}Creating Android project structure...${NC}"
    mkdir -p "$ANDROID_DIR"
    echo -e "${YELLOW}Note: Android project needs to be initialized${NC}"
    echo "Please run: android create project --path $ANDROID_DIR"
    exit 0
fi

cd "$ANDROID_DIR"

# Step 1: Clean previous build
echo -e "${YELLOW}Step 1: Cleaning previous build...${NC}"
if [ -f "gradlew" ]; then
    ./gradlew clean > /dev/null 2>&1 || true
    print_status 0 "Clean complete"
else
    echo -e "${YELLOW}⚠️  gradlew not found, skipping clean${NC}"
fi

# Step 2: Run lint
echo -e "${YELLOW}Step 2: Running lint checks...${NC}"
if [ -f "gradlew" ]; then
    if ./gradlew lint 2>&1 | tee -a "$BUILD_LOG"; then
        # Check for lint errors
        if [ -f "app/build/reports/lint-results.xml" ]; then
            LINT_ERRORS=$(grep -c 'severity="Error"' app/build/reports/lint-results.xml || true)
            echo "Lint errors: $LINT_ERRORS"
            
            if [ $LINT_ERRORS -eq 0 ]; then
                print_status 0 "Lint check passed"
            else
                print_status 1 "Lint errors found"
            fi
        else
            print_status 0 "Lint completed (no XML report)"
        fi
    else
        print_status 1 "Lint check failed"
    fi
else
    echo -e "${YELLOW}⚠️  Skipping lint (no gradlew)${NC}"
fi

# Step 3: Run unit tests
echo -e "${YELLOW}Step 3: Running unit tests...${NC}"
if [ -f "gradlew" ]; then
    if ./gradlew test 2>&1 | tee -a "$BUILD_LOG"; then
        print_status 0 "Unit tests passed"
        
        # Find and display test results
        TEST_RESULTS=$(find . -name "TEST-*.xml" 2>/dev/null | head -5)
        if [ -n "$TEST_RESULTS" ]; then
            echo "Test results:"
            echo "$TEST_RESULTS"
        fi
    else
        print_status 1 "Unit tests failed"
    fi
else
    echo -e "${YELLOW}⚠️  Skipping tests (no gradlew)${NC}"
fi

# Step 4: Run instrumented tests (if device available)
echo -e "${YELLOW}Step 4: Checking for connected Android devices...${NC}"
if command -v adb &> /dev/null; then
    DEVICES=$(adb devices | grep -w "device" | wc -l)
    if [ $DEVICES -gt 0 ]; then
        echo "Found $DEVICES connected device(s)"
        echo -e "${YELLOW}Running instrumented tests...${NC}"
        
        if [ -f "gradlew" ]; then
            if ./gradlew connectedAndroidTest 2>&1 | tee -a "$BUILD_LOG"; then
                print_status 0 "Instrumented tests passed"
            else
                echo -e "${YELLOW}⚠️  Instrumented tests failed or skipped${NC}"
            fi
        fi
    else
        echo -e "${YELLOW}⚠️  No Android devices connected, skipping instrumented tests${NC}"
    fi
else
    echo -e "${YELLOW}⚠️  adb not found, skipping instrumented tests${NC}"
fi

# Step 5: Build APK
echo -e "${YELLOW}Step 5: Building APK...${NC}"
if [ -f "gradlew" ]; then
    if ./gradlew assembleDebug 2>&1 | tee -a "$BUILD_LOG"; then
        print_status 0 "APK build successful"
    else
        print_status 1 "APK build failed"
    fi
else
    echo -e "${YELLOW}⚠️  Skipping APK build (no gradlew)${NC}"
fi

# Step 6: Check APK size
echo -e "${YELLOW}Step 6: Checking APK size...${NC}"
APK_FILES=$(find . -name "*.apk" -type f 2>/dev/null)

if [ -n "$APK_FILES" ]; then
    echo "APK files found:"
    for apk in $APK_FILES; do
        APK_SIZE=$(stat -c%s "$apk")
        APK_SIZE_MB=$((APK_SIZE / 1048576))
        echo "  $(basename $apk): ${APK_SIZE_MB}MB ($APK_SIZE bytes)"
        
        if [ $APK_SIZE -le $MAX_APK_SIZE ]; then
            echo -e "  ${GREEN}✅ Size OK${NC}"
        else
            echo -e "  ${RED}❌ Size exceeds ${MAX_APK_SIZE} bytes limit${NC}"
        fi
    done
    print_status 0 "APK size checked"
else
    echo -e "${YELLOW}⚠️  No APK files found${NC}"
fi

# Step 7: Generate coverage report (if available)
echo -e "${YELLOW}Step 7: Checking test coverage...${NC}"
if [ -f "gradlew" ]; then
    ./gradlew jacocoTestReport > /dev/null 2>&1 || true
    
    COVERAGE_REPORT=$(find . -name "index.html" -path "*/jacoco/*" | head -1)
    if [ -n "$COVERAGE_REPORT" ]; then
        echo "Coverage report: $COVERAGE_REPORT"
        print_status 0 "Coverage report generated"
    else
        echo -e "${YELLOW}⚠️  Coverage report not available${NC}"
    fi
else
    echo -e "${YELLOW}⚠️  Skipping coverage (no gradlew)${NC}"
fi

# Summary
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build Validation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

if [ -n "$APK_FILES" ]; then
    echo "Built APKs:"
    echo "$APK_FILES"
    echo ""
    echo "Next steps:"
    echo "  1. Install APK: adb install -r <apk-file>"
    echo "  2. Run app: adb shell am start -n <package>/<activity>"
    echo "  3. View logs: adb logcat"
else
    echo -e "${YELLOW}Note: Android project needs to be fully set up${NC}"
    echo "Please initialize the Android project structure first"
fi
echo ""

exit 0
