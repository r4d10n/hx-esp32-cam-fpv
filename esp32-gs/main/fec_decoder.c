/**
 * ESP32 FPV Ground Station - FEC Decoder
 *
 * Reed-Solomon FEC decoder over GF(2^8) using Vandermonde matrix.
 * Ported from zfec library.
 */

#include "fec_decoder.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "fec_dec";

// GF(2^8) tables
static uint8_t gf_exp[510];     // index->poly form
static int gf_log[256];         // poly->index form
static uint8_t gf_inverse[256]; // inverse of field element
static uint8_t (*gf_mul_table)[256] = NULL;  // 64KB multiplication table

// FEC code structure
typedef struct {
    uint8_t k;
    uint8_t n;
    uint8_t *enc_matrix;
} fec_code_t;

// Decoder state
static struct {
    bool initialized;
    uint8_t k;
    uint8_t n;
    uint8_t block_count;
    uint32_t timeout_ms;
    fec_code_t *code;
    fec_block_t *blocks;
    fec_stats_t stats;
    fec_packet_callback_t callback;
    SemaphoreHandle_t mutex;
    uint32_t oldest_block_index;
} s_decoder = {0};

// Forward declarations
static void init_gf_tables(void);
static fec_code_t *fec_new(uint8_t k, uint8_t n);
static void fec_free(fec_code_t *code);
static void fec_decode_block(fec_code_t *code, const uint8_t **inpkts,
                             uint8_t **outpkts, const unsigned *indices, size_t sz);
static fec_block_t *get_or_create_block(uint32_t block_index);
static void process_block(fec_block_t *block);
static void expire_old_blocks(void);

// GF(2^8) modular arithmetic
static uint8_t modnn(int x) {
    while (x >= 255) {
        x -= 255;
        x = (x >> 8) + (x & 255);
    }
    return (uint8_t)x;
}

#define gf_mul(x, y) gf_mul_table[x][y]

// Initialize GF tables
static void init_gf_tables(void) {
    // Primitive polynomial: x^8 + x^4 + x^3 + x^2 + 1 = 0x11d
    static const char *Pp = "101110001";

    uint8_t mask = 1;
    gf_exp[8] = 0;

    // Generate exp and log tables
    for (int i = 0; i < 8; i++, mask <<= 1) {
        gf_exp[i] = mask;
        gf_log[gf_exp[i]] = i;
        if (Pp[i] == '1') {
            gf_exp[8] ^= mask;
        }
    }
    gf_log[gf_exp[8]] = 8;

    mask = 1 << 7;
    for (int i = 9; i < 255; i++) {
        if (gf_exp[i - 1] >= mask) {
            gf_exp[i] = gf_exp[8] ^ ((gf_exp[i - 1] ^ mask) << 1);
        } else {
            gf_exp[i] = gf_exp[i - 1] << 1;
        }
        gf_log[gf_exp[i]] = i;
    }

    gf_log[0] = 255;

    // Extended exp table for fast multiply
    for (int i = 0; i < 255; i++) {
        gf_exp[i + 255] = gf_exp[i];
    }

    // Inverse table
    gf_inverse[0] = 0;
    gf_inverse[1] = 1;
    for (int i = 2; i <= 255; i++) {
        gf_inverse[i] = gf_exp[255 - gf_log[i]];
    }

    // Multiplication table (64KB, allocated in PSRAM)
    gf_mul_table = heap_caps_malloc(256 * 256, MALLOC_CAP_SPIRAM);
    if (gf_mul_table == NULL) {
        // Fall back to internal RAM if PSRAM not available
        gf_mul_table = heap_caps_malloc(256 * 256, MALLOC_CAP_8BIT);
    }

    if (gf_mul_table != NULL) {
        for (int i = 0; i < 256; i++) {
            for (int j = 0; j < 256; j++) {
                gf_mul_table[i][j] = gf_exp[modnn(gf_log[i] + gf_log[j])];
            }
        }
        for (int j = 0; j < 256; j++) {
            gf_mul_table[0][j] = gf_mul_table[j][0] = 0;
        }
    }

    ESP_LOGI(TAG, "GF(2^8) tables initialized, mul_table=%p", gf_mul_table);
}

// addmul: dst = dst + c * src
static void addmul(uint8_t *dst, const uint8_t *src, uint8_t c, size_t sz) {
    if (c == 0) return;
    const uint8_t *mul_row = gf_mul_table[c];
    for (size_t i = 0; i < sz; i++) {
        dst[i] ^= mul_row[src[i]];
    }
}

// Invert matrix in place (Gauss-Jordan)
static void invert_mat(uint8_t *src, unsigned k) {
    unsigned *indxc = alloca(k * sizeof(unsigned));
    unsigned *indxr = alloca(k * sizeof(unsigned));
    unsigned *ipiv = alloca(k * sizeof(unsigned));
    uint8_t *id_row = alloca(k);

    memset(id_row, 0, k);
    memset(ipiv, 0, k * sizeof(unsigned));

    for (unsigned col = 0; col < k; col++) {
        unsigned irow = 0, icol = 0;
        uint8_t *pivot_row;

        // Find pivot
        if (ipiv[col] != 1 && src[col * k + col] != 0) {
            irow = col;
            icol = col;
        } else {
            for (unsigned row = 0; row < k; row++) {
                if (ipiv[row] != 1) {
                    for (unsigned ix = 0; ix < k; ix++) {
                        if (ipiv[ix] == 0 && src[row * k + ix] != 0) {
                            irow = row;
                            icol = ix;
                            goto found_piv;
                        }
                    }
                }
            }
        }
    found_piv:
        ipiv[icol]++;

        // Swap rows if needed
        if (irow != icol) {
            for (unsigned ix = 0; ix < k; ix++) {
                uint8_t tmp = src[irow * k + ix];
                src[irow * k + ix] = src[icol * k + ix];
                src[icol * k + ix] = tmp;
            }
        }

        indxr[col] = irow;
        indxc[col] = icol;
        pivot_row = &src[icol * k];
        uint8_t c = pivot_row[icol];

        if (c != 1) {
            c = gf_inverse[c];
            pivot_row[icol] = 1;
            for (unsigned ix = 0; ix < k; ix++) {
                pivot_row[ix] = gf_mul(c, pivot_row[ix]);
            }
        }

        id_row[icol] = 1;
        if (memcmp(pivot_row, id_row, k) != 0) {
            uint8_t *p = src;
            for (unsigned ix = 0; ix < k; ix++, p += k) {
                if (ix != icol) {
                    c = p[icol];
                    p[icol] = 0;
                    addmul(p, pivot_row, c, k);
                }
            }
        }
        id_row[icol] = 0;
    }

    // Undo column swaps
    for (int col = k - 1; col >= 0; col--) {
        if (indxr[col] != indxc[col]) {
            for (unsigned row = 0; row < k; row++) {
                uint8_t tmp = src[row * k + indxr[col]];
                src[row * k + indxr[col]] = src[row * k + indxc[col]];
                src[row * k + indxc[col]] = tmp;
            }
        }
    }
}

// Fast Vandermonde matrix inversion
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
static void invert_vdm(uint8_t *src, unsigned k) {
    if (k == 1) return;

    uint8_t *c = calloc(k, 1);
    uint8_t *b = calloc(k, 1);
    uint8_t *p = calloc(k, 1);

    if (!c || !b || !p) {
        free(c);
        free(b);
        free(p);
        return;
    }

    // Initialize p array from source matrix
    for (unsigned i = 0; i < k; i++) {
        p[i] = src[i * k + 1];  // Second column
    }

    // Build P(x) = prod(x - p_i)
    c[k - 1] = p[0];
    for (unsigned i = 1; i < k; i++) {
        uint8_t p_i = p[i];
        for (unsigned j = k - 1 - (i - 1); j < k - 1; j++) {
            c[j] ^= gf_mul(p_i, c[j + 1]);
        }
        c[k - 1] ^= p_i;
    }

    for (unsigned row = 0; row < k; row++) {
        uint8_t xx = p[row];
        uint8_t t = 1;
        b[k - 1] = 1;
        for (int i = k - 2; i >= 0; i--) {
            b[i] = c[i + 1] ^ gf_mul(xx, b[i + 1]);
            t = gf_mul(xx, t) ^ b[i];
        }
        for (unsigned col = 0; col < k; col++) {
            src[col * k + row] = gf_mul(gf_inverse[t], b[col]);
        }
    }

    free(c);
    free(b);
    free(p);
}
#pragma GCC diagnostic pop

// Matrix multiply C = A * B (A: n*k, B: k*m, C: n*m)
static void matmul(uint8_t *a, uint8_t *b, uint8_t *c, unsigned n, unsigned k, unsigned m) {
    for (unsigned row = 0; row < n; row++) {
        for (unsigned col = 0; col < m; col++) {
            uint8_t *pa = &a[row * k];
            uint8_t *pb = &b[col];
            uint8_t acc = 0;
            for (unsigned i = 0; i < k; i++, pa++, pb += m) {
                acc ^= gf_mul(*pa, *pb);
            }
            c[row * m + col] = acc;
        }
    }
}

// Create FEC code
static fec_code_t *fec_new(uint8_t k, uint8_t n) {
    fec_code_t *code = malloc(sizeof(fec_code_t));
    if (!code) return NULL;

    code->k = k;
    code->n = n;
    code->enc_matrix = malloc(n * k);

    if (!code->enc_matrix) {
        free(code);
        return NULL;
    }

    // Build Vandermonde matrix
    uint8_t *tmp_m = malloc(n * k);
    if (!tmp_m) {
        free(code->enc_matrix);
        free(code);
        return NULL;
    }

    // First row is [1, 0, 0, ...]
    tmp_m[0] = 1;
    for (unsigned col = 1; col < k; col++) {
        tmp_m[col] = 0;
    }

    // Remaining rows: row[col] = alpha^(row * col)
    uint8_t *p = tmp_m + k;
    for (unsigned row = 0; row < n - 1; row++, p += k) {
        for (unsigned col = 0; col < k; col++) {
            p[col] = gf_exp[modnn(row * col)];
        }
    }

    // Invert top k*k matrix and multiply
    invert_vdm(tmp_m, k);
    matmul(tmp_m + k * k, tmp_m, code->enc_matrix + k * k, n - k, k, k);

    // Top k*k is identity
    memset(code->enc_matrix, 0, k * k);
    for (unsigned col = 0; col < k; col++) {
        code->enc_matrix[col * k + col] = 1;
    }

    free(tmp_m);
    return code;
}

static void fec_free(fec_code_t *code) {
    if (code) {
        free(code->enc_matrix);
        free(code);
    }
}

// Build decode matrix
static void build_decode_matrix(fec_code_t *code, const unsigned *indices, uint8_t *matrix) {
    for (unsigned i = 0; i < code->k; i++) {
        uint8_t *p = matrix + i * code->k;
        if (indices[i] < code->k) {
            memset(p, 0, code->k);
            p[i] = 1;
        } else {
            memcpy(p, &code->enc_matrix[indices[i] * code->k], code->k);
        }
    }
    invert_mat(matrix, code->k);
}

// Decode FEC block
static void fec_decode_block(fec_code_t *code, const uint8_t **inpkts,
                             uint8_t **outpkts, const unsigned *indices, size_t sz) {
    uint8_t *m_dec = alloca(code->k * code->k);
    build_decode_matrix(code, indices, m_dec);

    uint8_t outix = 0;
    for (unsigned row = 0; row < code->k; row++) {
        if (indices[row] >= code->k) {
            // This packet needs to be recovered
            memset(outpkts[outix], 0, sz);
            for (unsigned col = 0; col < code->k; col++) {
                addmul(outpkts[outix], inpkts[col], m_dec[row * code->k + col], sz);
            }
            outix++;
        }
    }
}

// Get or create block for given index
static fec_block_t *get_or_create_block(uint32_t block_index) {
    // Find existing block
    for (int i = 0; i < s_decoder.block_count; i++) {
        if (s_decoder.blocks[i].block_index == block_index &&
            s_decoder.blocks[i].state != FEC_BLOCK_EMPTY) {
            return &s_decoder.blocks[i];
        }
    }

    // Find empty slot or oldest block
    int empty_slot = -1;
    int oldest_slot = 0;
    uint32_t oldest_index = UINT32_MAX;

    for (int i = 0; i < s_decoder.block_count; i++) {
        if (s_decoder.blocks[i].state == FEC_BLOCK_EMPTY) {
            empty_slot = i;
            break;
        }
        if (s_decoder.blocks[i].block_index < oldest_index) {
            oldest_index = s_decoder.blocks[i].block_index;
            oldest_slot = i;
        }
    }

    int slot = (empty_slot >= 0) ? empty_slot : oldest_slot;
    fec_block_t *block = &s_decoder.blocks[slot];

    // If reusing old block, count dropped packets
    if (block->state != FEC_BLOCK_EMPTY && block->state != FEC_BLOCK_DECODED) {
        s_decoder.stats.packets_dropped += block->total_count;
        if (block->state == FEC_BLOCK_FAILED) {
            s_decoder.stats.blocks_failed++;
        }
    }

    // Initialize new block
    memset(block, 0, sizeof(fec_block_t));
    block->block_index = block_index;
    block->state = FEC_BLOCK_PARTIAL;
    block->first_packet_time = esp_timer_get_time() / 1000;

    // Allocate packet buffers from PSRAM
    for (int i = 0; i < FEC_MAX_N; i++) {
        if (block->packets[i].data == NULL) {
            block->packets[i].data = heap_caps_malloc(FEC_MAX_PACKET_SIZE, MALLOC_CAP_SPIRAM);
            if (block->packets[i].data == NULL) {
                block->packets[i].data = heap_caps_malloc(FEC_MAX_PACKET_SIZE, MALLOC_CAP_8BIT);
            }
        }
    }

    s_decoder.stats.blocks_received++;
    return block;
}

// Process a block that may be ready for decoding
static void process_block(fec_block_t *block) {
    if (block->state != FEC_BLOCK_READY) {
        return;
    }

    uint8_t k = s_decoder.k;

    // Check if we have all data packets (no FEC needed)
    if (block->data_count >= k) {
        block->state = FEC_BLOCK_COMPLETE;
        s_decoder.stats.blocks_complete++;

        // Deliver all data packets in order
        if (s_decoder.callback) {
            for (uint8_t i = 0; i < k; i++) {
                if (block->packets[i].present) {
                    s_decoder.callback(block->block_index, i,
                                       block->packets[i].data,
                                       block->packets[i].size, false);
                }
            }
        }
        block->state = FEC_BLOCK_DECODED;
        return;
    }

    // Need FEC recovery
    if (block->total_count < k) {
        // Not enough packets yet
        return;
    }

    // Build input arrays for FEC decoder
    const uint8_t *inpkts[FEC_MAX_K];
    unsigned indices[FEC_MAX_K];
    uint8_t missing_indices[FEC_MAX_K];
    uint8_t missing_count = 0;

    int in_idx = 0;

    // First, add available data packets in order
    for (uint8_t i = 0; i < k && in_idx < k; i++) {
        if (block->packets[i].present) {
            inpkts[in_idx] = block->packets[i].data;
            indices[in_idx] = i;
            in_idx++;
        } else {
            missing_indices[missing_count++] = i;
        }
    }

    // Fill remaining slots with FEC packets
    for (uint8_t i = k; i < s_decoder.n && in_idx < k; i++) {
        if (block->packets[i].present) {
            inpkts[in_idx] = block->packets[i].data;
            indices[in_idx] = i;
            in_idx++;
        }
    }

    if (in_idx < k) {
        // Still not enough packets
        return;
    }

    // Allocate output buffers for missing packets
    uint8_t *outpkts[FEC_MAX_K];
    for (int i = 0; i < missing_count; i++) {
        outpkts[i] = block->packets[missing_indices[i]].data;
    }

    // Perform FEC decode
    fec_decode_block(s_decoder.code, inpkts, outpkts, indices, block->max_packet_size);

    // Mark recovered packets
    for (int i = 0; i < missing_count; i++) {
        uint8_t idx = missing_indices[i];
        block->packets[idx].present = true;
        block->packets[idx].recovered = true;
        block->packets[idx].size = block->max_packet_size;
        s_decoder.stats.packets_recovered++;
    }

    block->state = FEC_BLOCK_DECODED;
    s_decoder.stats.blocks_recovered++;

    // Deliver all data packets in order
    if (s_decoder.callback) {
        for (uint8_t i = 0; i < k; i++) {
            s_decoder.callback(block->block_index, i,
                               block->packets[i].data,
                               block->packets[i].size,
                               block->packets[i].recovered);
        }
    }
}

// Expire blocks that have timed out
static void expire_old_blocks(void) {
    int64_t now = esp_timer_get_time() / 1000;

    for (int i = 0; i < s_decoder.block_count; i++) {
        fec_block_t *block = &s_decoder.blocks[i];

        if (block->state == FEC_BLOCK_PARTIAL || block->state == FEC_BLOCK_READY) {
            if (now - block->first_packet_time > s_decoder.timeout_ms) {
                // Try to decode with what we have
                if (block->total_count >= s_decoder.k) {
                    block->state = FEC_BLOCK_READY;
                    process_block(block);
                } else {
                    s_decoder.stats.blocks_failed++;
                    s_decoder.stats.packets_dropped += block->total_count;
                    block->state = FEC_BLOCK_FAILED;
                }
            }
        }
    }
}

// Public API

esp_err_t fec_decoder_init(uint8_t k, uint8_t n, uint8_t block_count, uint32_t timeout_ms) {
    if (s_decoder.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (k > FEC_MAX_K || n > FEC_MAX_N || k > n) {
        ESP_LOGE(TAG, "Invalid FEC parameters k=%d, n=%d", k, n);
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize GF tables
    init_gf_tables();

    if (gf_mul_table == NULL) {
        ESP_LOGE(TAG, "Failed to allocate GF multiplication table");
        return ESP_ERR_NO_MEM;
    }

    // Create FEC code
    s_decoder.code = fec_new(k, n);
    if (s_decoder.code == NULL) {
        ESP_LOGE(TAG, "Failed to create FEC code");
        return ESP_ERR_NO_MEM;
    }

    // Allocate block array
    s_decoder.blocks = heap_caps_calloc(block_count, sizeof(fec_block_t), MALLOC_CAP_SPIRAM);
    if (s_decoder.blocks == NULL) {
        s_decoder.blocks = heap_caps_calloc(block_count, sizeof(fec_block_t), MALLOC_CAP_8BIT);
    }
    if (s_decoder.blocks == NULL) {
        fec_free(s_decoder.code);
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_decoder.mutex = xSemaphoreCreateMutex();
    if (s_decoder.mutex == NULL) {
        free(s_decoder.blocks);
        fec_free(s_decoder.code);
        return ESP_ERR_NO_MEM;
    }

    s_decoder.k = k;
    s_decoder.n = n;
    s_decoder.block_count = block_count;
    s_decoder.timeout_ms = timeout_ms;
    s_decoder.initialized = true;

    ESP_LOGI(TAG, "FEC decoder initialized: k=%d, n=%d, blocks=%d, timeout=%lums",
             k, n, block_count, (unsigned long)timeout_ms);

    return ESP_OK;
}

void fec_decoder_deinit(void) {
    if (!s_decoder.initialized) return;

    xSemaphoreTake(s_decoder.mutex, portMAX_DELAY);

    // Free packet buffers
    for (int i = 0; i < s_decoder.block_count; i++) {
        for (int j = 0; j < FEC_MAX_N; j++) {
            if (s_decoder.blocks[i].packets[j].data) {
                free(s_decoder.blocks[i].packets[j].data);
            }
        }
    }

    free(s_decoder.blocks);
    fec_free(s_decoder.code);

    if (gf_mul_table) {
        free(gf_mul_table);
        gf_mul_table = NULL;
    }

    xSemaphoreGive(s_decoder.mutex);
    vSemaphoreDelete(s_decoder.mutex);

    memset(&s_decoder, 0, sizeof(s_decoder));
}

void fec_decoder_set_callback(fec_packet_callback_t callback) {
    s_decoder.callback = callback;
}

esp_err_t fec_decoder_add_packet(uint32_t block_index, uint8_t packet_index,
                                  const uint8_t *data, size_t size) {
    if (!s_decoder.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (packet_index >= s_decoder.n || size > FEC_MAX_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_decoder.mutex, portMAX_DELAY);

    fec_block_t *block = get_or_create_block(block_index);
    if (block == NULL) {
        xSemaphoreGive(s_decoder.mutex);
        return ESP_ERR_NO_MEM;
    }

    // Check if we already have this packet
    if (block->packets[packet_index].present) {
        xSemaphoreGive(s_decoder.mutex);
        return ESP_OK;  // Duplicate
    }

    // Store packet
    if (block->packets[packet_index].data == NULL) {
        block->packets[packet_index].data = heap_caps_malloc(FEC_MAX_PACKET_SIZE, MALLOC_CAP_SPIRAM);
        if (block->packets[packet_index].data == NULL) {
            block->packets[packet_index].data = malloc(FEC_MAX_PACKET_SIZE);
        }
    }

    if (block->packets[packet_index].data == NULL) {
        xSemaphoreGive(s_decoder.mutex);
        return ESP_ERR_NO_MEM;
    }

    memcpy(block->packets[packet_index].data, data, size);
    block->packets[packet_index].size = size;
    block->packets[packet_index].packet_index = packet_index;
    block->packets[packet_index].present = true;
    block->packets[packet_index].recovered = false;

    if (size > block->max_packet_size) {
        block->max_packet_size = size;
    }

    block->total_count++;
    if (packet_index < s_decoder.k) {
        block->data_count++;
    } else {
        block->fec_count++;
    }

    // Check if block is ready for decoding
    if (block->total_count >= s_decoder.k) {
        block->state = FEC_BLOCK_READY;
        process_block(block);
    }

    xSemaphoreGive(s_decoder.mutex);
    return ESP_OK;
}

void fec_decoder_process(void) {
    if (!s_decoder.initialized) return;

    xSemaphoreTake(s_decoder.mutex, portMAX_DELAY);

    // Process any ready blocks
    for (int i = 0; i < s_decoder.block_count; i++) {
        if (s_decoder.blocks[i].state == FEC_BLOCK_READY) {
            process_block(&s_decoder.blocks[i]);
        }
    }

    // Expire old blocks
    expire_old_blocks();

    xSemaphoreGive(s_decoder.mutex);
}

void fec_decoder_get_stats(fec_stats_t *stats) {
    if (stats) {
        memcpy(stats, &s_decoder.stats, sizeof(fec_stats_t));
    }
}

void fec_decoder_reset(void) {
    if (!s_decoder.initialized) return;

    xSemaphoreTake(s_decoder.mutex, portMAX_DELAY);

    for (int i = 0; i < s_decoder.block_count; i++) {
        s_decoder.blocks[i].state = FEC_BLOCK_EMPTY;
        s_decoder.blocks[i].total_count = 0;
    }

    memset(&s_decoder.stats, 0, sizeof(fec_stats_t));

    xSemaphoreGive(s_decoder.mutex);
}
