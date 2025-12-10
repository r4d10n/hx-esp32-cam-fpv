/**
 * FEC Unit Test
 * Tests basic FEC encode/decode functionality without external test data
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../../components/common/fec.h"

// Test parameters
static constexpr int K = 6;  // Data blocks
static constexpr int N = 12; // Total blocks (6 data + 6 FEC)
static constexpr int BLOCK_SIZE = 64;

bool test_basic_fec() {
    printf("Test: Basic FEC encode/decode... ");

    init_fec();
    fec_t* fec = fec_new(K, N);
    if (!fec) {
        printf("FAIL (couldn't create FEC context)\n");
        return false;
    }

    // Create test data blocks
    gf* data_blocks[K];
    gf* fec_blocks[N - K];

    for (int i = 0; i < K; i++) {
        data_blocks[i] = new gf[BLOCK_SIZE];
        for (int j = 0; j < BLOCK_SIZE; j++) {
            data_blocks[i][j] = (i * BLOCK_SIZE + j) & 0xFF;
        }
    }

    for (int i = 0; i < N - K; i++) {
        fec_blocks[i] = new gf[BLOCK_SIZE];
        memset(fec_blocks[i], 0, BLOCK_SIZE);
    }

    // Block numbers for FEC (6, 7, 8, 9, 10, 11)
    unsigned block_nums[N - K];
    for (int i = 0; i < N - K; i++) {
        block_nums[i] = K + i;
    }

    // Encode
    fec_encode(fec, (const gf*const*)data_blocks, fec_blocks, block_nums, N - K, BLOCK_SIZE);

    // Simulate losing blocks 0 and 2
    gf* recovery_src[K];
    unsigned recovery_indices[K];

    // Use blocks 1, 3, 4, 5 (data) and blocks 6, 7 (FEC)
    recovery_src[0] = fec_blocks[0];  // FEC block 6
    recovery_indices[0] = K;

    recovery_src[1] = data_blocks[1]; // Data block 1
    recovery_indices[1] = 1;

    recovery_src[2] = fec_blocks[1];  // FEC block 7
    recovery_indices[2] = K + 1;

    recovery_src[3] = data_blocks[3]; // Data block 3
    recovery_indices[3] = 3;

    recovery_src[4] = data_blocks[4]; // Data block 4
    recovery_indices[4] = 4;

    recovery_src[5] = data_blocks[5]; // Data block 5
    recovery_indices[5] = 5;

    // Buffers for recovered blocks
    gf* recovered[2];
    recovered[0] = new gf[BLOCK_SIZE];
    recovered[1] = new gf[BLOCK_SIZE];
    memset(recovered[0], 0, BLOCK_SIZE);
    memset(recovered[1], 0, BLOCK_SIZE);

    // Decode
    fec_decode(fec, (const gf*const*)recovery_src, recovered, recovery_indices, BLOCK_SIZE);

    // Verify recovered blocks match original
    bool success = true;

    if (memcmp(recovered[0], data_blocks[0], BLOCK_SIZE) != 0) {
        printf("FAIL (block 0 mismatch)\n");
        success = false;
    }

    if (memcmp(recovered[1], data_blocks[2], BLOCK_SIZE) != 0) {
        printf("FAIL (block 2 mismatch)\n");
        success = false;
    }

    // Cleanup
    for (int i = 0; i < K; i++) delete[] data_blocks[i];
    for (int i = 0; i < N - K; i++) delete[] fec_blocks[i];
    delete[] recovered[0];
    delete[] recovered[1];
    fec_free(fec);

    if (success) {
        printf("PASS\n");
    }
    return success;
}

bool test_no_loss() {
    printf("Test: FEC with no loss... ");

    init_fec();
    fec_t* fec = fec_new(K, N);
    if (!fec) {
        printf("FAIL (couldn't create FEC context)\n");
        return false;
    }

    // Create test data blocks with known pattern
    gf* data_blocks[K];
    for (int i = 0; i < K; i++) {
        data_blocks[i] = new gf[BLOCK_SIZE];
        for (int j = 0; j < BLOCK_SIZE; j++) {
            data_blocks[i][j] = (i * 10 + j) & 0xFF;
        }
    }

    // Create FEC blocks
    gf* fec_blocks[N - K];
    for (int i = 0; i < N - K; i++) {
        fec_blocks[i] = new gf[BLOCK_SIZE];
        memset(fec_blocks[i], 0, BLOCK_SIZE);
    }

    unsigned block_nums[N - K];
    for (int i = 0; i < N - K; i++) {
        block_nums[i] = K + i;
    }

    // Encode FEC blocks
    fec_encode(fec, (const gf*const*)data_blocks, fec_blocks, block_nums, N - K, BLOCK_SIZE);

    // Verify FEC blocks are non-zero (sanity check)
    bool has_nonzero = false;
    for (int i = 0; i < N - K && !has_nonzero; i++) {
        for (int j = 0; j < BLOCK_SIZE && !has_nonzero; j++) {
            if (fec_blocks[i][j] != 0) has_nonzero = true;
        }
    }

    // Cleanup
    for (int i = 0; i < K; i++) delete[] data_blocks[i];
    for (int i = 0; i < N - K; i++) delete[] fec_blocks[i];
    fec_free(fec);

    if (!has_nonzero) {
        printf("FAIL (FEC blocks are all zeros)\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

bool test_max_loss_recovery() {
    printf("Test: FEC max loss recovery (lose all %d FEC-recoverable blocks)... ", N - K);

    init_fec();
    fec_t* fec = fec_new(K, N);
    if (!fec) {
        printf("FAIL (couldn't create FEC context)\n");
        return false;
    }

    // Create test data blocks
    gf* data_blocks[K];
    for (int i = 0; i < K; i++) {
        data_blocks[i] = new gf[BLOCK_SIZE];
        for (int j = 0; j < BLOCK_SIZE; j++) {
            data_blocks[i][j] = (unsigned char)((i + 1) * (j + 1));
        }
    }

    // Create and encode FEC blocks
    gf* fec_blocks[N - K];
    for (int i = 0; i < N - K; i++) {
        fec_blocks[i] = new gf[BLOCK_SIZE];
        memset(fec_blocks[i], 0, BLOCK_SIZE);
    }

    unsigned block_nums[N - K];
    for (int i = 0; i < N - K; i++) {
        block_nums[i] = K + i;
    }

    fec_encode(fec, (const gf*const*)data_blocks, fec_blocks, block_nums, N - K, BLOCK_SIZE);

    // Lose all data blocks, keep only FEC blocks
    gf* recovery_src[K];
    unsigned recovery_indices[K];

    for (int i = 0; i < K; i++) {
        recovery_src[i] = fec_blocks[i];
        recovery_indices[i] = K + i;
    }

    // Recover all data blocks
    gf* recovered[K];
    for (int i = 0; i < K; i++) {
        recovered[i] = new gf[BLOCK_SIZE];
        memset(recovered[i], 0, BLOCK_SIZE);
    }

    fec_decode(fec, (const gf*const*)recovery_src, recovered, recovery_indices, BLOCK_SIZE);

    // Verify all recovered blocks
    bool success = true;
    for (int i = 0; i < K; i++) {
        if (memcmp(recovered[i], data_blocks[i], BLOCK_SIZE) != 0) {
            printf("FAIL (block %d mismatch)\n", i);
            success = false;
            break;
        }
    }

    // Cleanup
    for (int i = 0; i < K; i++) {
        delete[] data_blocks[i];
        delete[] recovered[i];
    }
    for (int i = 0; i < N - K; i++) delete[] fec_blocks[i];
    fec_free(fec);

    if (success) {
        printf("PASS\n");
    }
    return success;
}

int main() {
    printf("\n=== FEC Unit Tests ===\n");
    printf("Parameters: K=%d, N=%d, BlockSize=%d\n\n", K, N, BLOCK_SIZE);

    int passed = 0;
    int failed = 0;

    if (test_no_loss()) passed++; else failed++;
    if (test_basic_fec()) passed++; else failed++;
    if (test_max_loss_recovery()) passed++; else failed++;

    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);

    return failed > 0 ? 1 : 0;
}
