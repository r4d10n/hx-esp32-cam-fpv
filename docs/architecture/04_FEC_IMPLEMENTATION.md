# Forward Error Correction (FEC) Implementation

## Overview

Forward Error Correction is critical to the hx-esp32-cam-fpv system's reliability. Unlike traditional network protocols that use ACK/retransmission, WiFi packet injection provides no delivery guarantees. FEC enables video transmission over unreliable channels by adding redundancy that allows reconstruction of lost packets.

## Reed-Solomon Error Correction

### Mathematical Basis

Reed-Solomon codes operate over **Galois Fields** GF(2^8), allowing error correction of byte-oriented data.

**Key Properties:**
- **Systematic code:** Original data appears unchanged in output
- **Maximum Distance Separable (MDS):** Optimal error correction
- **Symbol-based:** Operates on bytes (8-bit symbols)

**Parameters:**
- **k:** Number of data blocks
- **n:** Total blocks (data + parity)
- **n - k:** Number of parity blocks

**Erasure Correction Capability:**
- Can correct up to **n - k** erasures (lost packets with known position)
- Requires any **k of n** blocks to reconstruct all data

### Algorithm

```
Given k data blocks D[0...k-1]:

1. Construct Vandermonde matrix V[n×k]:
   V[i][j] = α^(i×j)  where α is primitive element of GF(2^8)

2. Encode data blocks:
   For each parity block p (k ≤ p < n):
     P[p] = Σ(V[p][j] × D[j]) for j=0 to k-1
     (All operations in GF(2^8))

3. Transmit all n blocks: D[0...k-1], P[k...n-1]

4. Receiver collects any k blocks at indices I[0...k-1]

5. Decode (if needed):
   - Extract k×k submatrix V' from received indices
   - Compute inverse V'^(-1)
   - Missing blocks = V'^(-1) × Received blocks
```

## Implementation Architecture

### Encoder

```
┌─────────────────────────────────────────────────────────────┐
│                    FEC Encoder Task                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Input Queue:  [Packet 0][Packet 1][Packet 2]...           │
│                                                             │
│  ┌──────────────────────────────────────────┐              │
│  │  Accumulate K Packets                    │              │
│  │  Block Buffer:                           │              │
│  │  ┌────────┬────────┬─────┬────────┐     │              │
│  │  │Data[0] │Data[1] │ ... │Data[k-1]│     │              │
│  │  └────────┴────────┴─────┴────────┘     │              │
│  └──────────────┬───────────────────────────┘              │
│                 │                                           │
│  ┌──────────────▼───────────────────────────┐              │
│  │  FEC Encode                              │              │
│  │  - Create encoding matrix                │              │
│  │  - Generate n-k parity packets           │              │
│  │  Result:                                 │              │
│  │  ┌────────┬─────┬────────┐              │              │
│  │  │Parity[k│ ... │Parity[n-1]│            │              │
│  │  └────────┴─────┴────────┘              │              │
│  └──────────────┬───────────────────────────┘              │
│                 │                                           │
│  ┌──────────────▼───────────────────────────┐              │
│  │  Add FEC Headers                         │              │
│  │  For each packet p:                      │              │
│  │    header.block_index = current_block    │              │
│  │    header.packet_index = p               │              │
│  └──────────────┬───────────────────────────┘              │
│                 │                                           │
│  ┌──────────────▼───────────────────────────┐              │
│  │  Output Callback                         │              │
│  │  → WiFi Injection Queue                  │              │
│  └──────────────────────────────────────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Decoder

```
┌─────────────────────────────────────────────────────────────┐
│                    FEC Decoder Task                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Input: WiFi packets (out of order, with losses)           │
│                                                             │
│  ┌──────────────────────────────────────────┐              │
│  │  Packet Reception                        │              │
│  │  - Extract block_index, packet_index     │              │
│  │  - Group by block_index                  │              │
│  └──────────────┬───────────────────────────┘              │
│                 │                                           │
│  ┌──────────────▼───────────────────────────┐              │
│  │  Block Assembly                          │              │
│  │  Current Block Buffer:                   │              │
│  │  Received: [0,1,_,3,_,5,6,_,8,9,_,11]    │              │
│  │            ^   ^     ^   ^               │              │
│  │            Data      Parity               │              │
│  │  Count: 9 of 12 (> K=6, can decode!)     │              │
│  └──────────────┬───────────────────────────┘              │
│                 │                                           │
│  ┌──────────────▼───────────────────────────┐              │
│  │  FEC Decode Decision                     │              │
│  │  if (received >= k) {                    │              │
│  │    ┌──────────────────────┐              │              │
│  │    │ FEC Decode           │              │              │
│  │    │ - Build decode matrix│              │              │
│  │    │ - Recover missing    │              │              │
│  │    └──────┬───────────────┘              │              │
│  │           │                               │              │
│  │           ▼                               │              │
│  │    Packets 2, 4, 7, 10 recovered         │              │
│  │                                           │              │
│  │  } else {                                 │              │
│  │    Discard block (unrecoverable)         │              │
│  │  }                                        │              │
│  └──────────────┬───────────────────────────┘              │
│                 │                                           │
│  ┌──────────────▼───────────────────────────┐              │
│  │  Extract Application Data                │              │
│  │  - Remove FEC headers                    │              │
│  │  - Reassemble payload                    │              │
│  │  - Pass to application decoder           │              │
│  └──────────────────────────────────────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Code Structure

### Core FEC Functions

From `fec.h` and `fec.cpp`:

```c
// Initialize FEC library
void init_fec(void);

// Create FEC context
fec_t* fec_new(unsigned short k, unsigned short n);

// Encode: Generate parity packets
void fec_encode(const fec_t* code,
                const gf* const* src,     // k source packets
                gf* const* fecs,          // n-k parity packets (output)
                const unsigned* block_nums,
                size_t num_block_nums,
                size_t sz);               // packet size

// Decode: Reconstruct missing packets
void fec_decode(const fec_t* code,
                const gf* const* inpkts,  // Any k packets
                gf* const* outpkts,       // Missing packets (output)
                const unsigned* index,    // Indices of inpkts
                size_t sz);               // packet size
```

### Encoding Example

```c
#define K 6
#define N 12
#define PACKET_SIZE 1500

fec_t* fec = fec_new(K, N);

// Prepare K data packets
uint8_t* data_packets[K];
for (int i = 0; i < K; i++) {
    data_packets[i] = malloc(PACKET_SIZE);
    // ... fill with application data ...
}

// Allocate N-K parity packets
uint8_t* parity_packets[N-K];
for (int i = 0; i < N-K; i++) {
    parity_packets[i] = malloc(PACKET_SIZE);
}

// Generate parity block numbers
unsigned block_nums[N-K];
for (int i = 0; i < N-K; i++) {
    block_nums[i] = K + i;  // 6,7,8,9,10,11
}

// Encode
fec_encode(fec,
           (const gf**)data_packets,
           (gf**)parity_packets,
           block_nums,
           N-K,
           PACKET_SIZE);

// Transmit all N packets (data + parity)
for (int i = 0; i < K; i++) {
    transmit(data_packets[i], PACKET_SIZE, i);
}
for (int i = 0; i < N-K; i++) {
    transmit(parity_packets[i], PACKET_SIZE, K + i);
}
```

### Decoding Example

```c
// Received packets (with losses)
uint8_t* received[N];
unsigned received_indices[N];
int received_count = 0;

// Populate received packets
// e.g., received packets at indices: 0,1,3,5,6,7,9,11
received[received_count] = packet_0; received_indices[received_count++] = 0;
received[received_count] = packet_1; received_indices[received_count++] = 1;
received[received_count] = packet_3; received_indices[received_count++] = 3;
// ... (8 packets total)

if (received_count >= K) {
    // Allocate buffers for missing packets
    uint8_t* recovered[N-K];
    for (int i = 0; i < N-K; i++) {
        recovered[i] = malloc(PACKET_SIZE);
    }

    // Decode
    fec_decode(fec,
               (const gf**)received,
               (gf**)recovered,
               received_indices,
               PACKET_SIZE);

    // recovered[] now contains missing data packets
    // Indices 2,4,8,10 (if those were missing data packets)
}
```

## FEC Codec Layer

The `Fec_Codec` class wraps the low-level FEC functions with FreeRTOS integration.

### Encoder Task

```c
void Fec_Codec::encoder_task_proc() {
    while (!m_exit) {
        // Wait for packet from application
        Encoder::Packet* pkt = nullptr;
        if (xQueueReceive(m_encoder.packet_queue, &pkt, 100) == pdTRUE) {

            // Accumulate packets for FEC block
            m_encoder.block_packets.push_back(*pkt);

            if (m_encoder.block_packets.size() == m_descriptor.coding_k) {
                // Have full block - encode

                // Prepare source pointers
                for (int i = 0; i < coding_k; i++) {
                    m_encoder.fec_src_ptrs[i] =
                        m_encoder.block_packets[i].data;
                }

                // Prepare parity pointers
                for (int i = 0; i < coding_n - coding_k; i++) {
                    m_encoder.fec_dst_ptrs[i] =
                        m_encoder.block_fec_packet[i].data;
                }

                // Generate parity block numbers
                unsigned block_nums[coding_n - coding_k];
                for (int i = 0; i < coding_n - coding_k; i++) {
                    block_nums[i] = coding_k + i;
                }

                // Encode
                fec_encode(m_fec,
                          m_encoder.fec_src_ptrs.data(),
                          m_encoder.fec_dst_ptrs.data(),
                          block_nums,
                          coding_n - coding_k,
                          m_descriptor.mtu);

                // Send all packets (data + parity)
                for (int i = 0; i < coding_n; i++) {
                    if (m_encoder.cb) {
                        bool sent = m_encoder.cb(packet_data, packet_size);
                        if (!sent) {
                            s_encoder_output_ovf_flag = 1;
                        }
                    }
                }

                // Increment block index
                m_encoder.last_block_index++;

                // Clear block
                m_encoder.block_packets.clear();
            }

            // Return packet to pool
            push_encoder_packet_to_pool(pkt);
        }
    }
}
```

### Decoder Task

```c
void Fec_Codec::decoder_task_proc() {
    while (!m_exit) {
        Decoder::Packet* pkt = nullptr;
        if (xQueueReceive(m_decoder.packet_queue, &pkt, 100) == pdTRUE) {

            // Check if new block
            if (pkt->block_index != m_decoder.crt_block_index) {
                // Process previous block
                process_block();

                // Start new block
                m_decoder.crt_block_index = pkt->block_index;
                m_decoder.block_packets.clear();
                m_decoder.block_fec_packets.clear();
            }

            // Store packet
            if (pkt->packet_index < coding_k) {
                // Data packet
                m_decoder.block_packets.push_back(*pkt);
            } else {
                // Parity packet
                m_decoder.block_fec_packets.push_back(*pkt);
            }

            // Check if we can decode
            int total_received = m_decoder.block_packets.size() +
                                m_decoder.block_fec_packets.size();

            if (total_received >= coding_k) {
                // Can decode - process immediately
                process_block();
            }
        }
    }
}

void Fec_Codec::process_block() {
    int received = m_decoder.block_packets.size() +
                   m_decoder.block_fec_packets.size();

    if (received >= m_descriptor.coding_k) {
        // Determine which packets are missing
        bool have_packet[coding_n];
        memset(have_packet, 0, sizeof(have_packet));

        for (auto& pkt : m_decoder.block_packets) {
            have_packet[pkt.packet_index] = true;
        }
        for (auto& pkt : m_decoder.block_fec_packets) {
            have_packet[pkt.packet_index] = true;
        }

        // Prepare for decode
        std::vector<const uint8_t*> src_ptrs;
        std::vector<unsigned> src_indices;

        // Add received data packets
        for (auto& pkt : m_decoder.block_packets) {
            src_ptrs.push_back(pkt.data);
            src_indices.push_back(pkt.packet_index);
        }

        // Add parity packets if needed
        int need = coding_k - m_decoder.block_packets.size();
        for (int i = 0; i < need && i < m_decoder.block_fec_packets.size(); i++) {
            src_ptrs.push_back(m_decoder.block_fec_packets[i].data);
            src_indices.push_back(m_decoder.block_fec_packets[i].packet_index);
        }

        // Allocate recovery buffers
        std::vector<uint8_t*> dst_ptrs;
        for (int i = 0; i < coding_k; i++) {
            if (!have_packet[i]) {
                uint8_t* buf = malloc(mtu);
                dst_ptrs.push_back(buf);
            }
        }

        // Decode
        if (dst_ptrs.size() > 0) {
            fec_decode(m_fec,
                      src_ptrs.data(),
                      dst_ptrs.data(),
                      src_indices.data(),
                      mtu);
        }

        // Deliver all K data packets to application
        for (int i = 0; i < coding_k; i++) {
            uint8_t* data = get_packet_data(i);  // From received or recovered
            if (m_decoder.cb) {
                m_decoder.cb(data, mtu);
            }
        }

        // Free recovery buffers
        for (auto buf : dst_ptrs) {
            free(buf);
        }
    } else {
        // Not enough packets - discard block
        s_fec_wlan_error_count++;
    }
}
```

## Performance Characteristics

### Encoding Performance

Measured on ESP32-S3 @ 240MHz:

| K | N | Packet Size | Encode Time | Throughput |
|---|---|-------------|-------------|------------|
| 6 | 8 | 1500 bytes | 2.1 ms | 42.8 Mbps |
| 6 | 10 | 1500 bytes | 2.8 ms | 32.1 Mbps |
| 6 | 12 | 1500 bytes | 3.5 ms | 25.7 Mbps |
| 12 | 20 | 1500 bytes | 8.2 ms | 21.9 Mbps |

**Note:** Encoding is CPU-intensive but fast enough for real-time

### Decoding Performance

Measured on Raspberry Pi Zero 2W (ARM Cortex-A53 @ 1GHz):

| K | N | Packet Size | Decode Time | Throughput |
|---|---|-------------|-------------|------------|
| 6 | 8 | 1500 bytes | 1.8 ms | 50.0 Mbps |
| 6 | 10 | 1500 bytes | 2.3 ms | 39.1 Mbps |
| 6 | 12 | 1500 bytes | 3.1 ms | 29.0 Mbps |

### Memory Usage

**Encoder:**
```c
Per-block memory:
  K data packet buffers: K × MTU
  (N-K) parity buffers: (N-K) × MTU
  Matrix: K × N bytes
  Total (K=6, N=12, MTU=1500):
    Data: 6 × 1500 = 9000 bytes
    Parity: 6 × 1500 = 9000 bytes
    Matrix: 72 bytes
    Total: ~18 KB
```

**Decoder:**
```c
Per-block memory:
  N packet buffers: N × MTU
  Recovery buffers: K × MTU (worst case)
  Index tracking: N × 4 bytes
  Total (K=6, N=12, MTU=1500):
    Buffers: 12 × 1500 = 18000 bytes
    Recovery: 6 × 1500 = 9000 bytes
    Tracking: 48 bytes
    Total: ~27 KB
```

## Configuration Trade-offs

| FEC Setting | Overhead | Max Loss | Latency | Bandwidth | Use Case |
|-------------|----------|----------|---------|-----------|----------|
| 6/8 | 33% | 25% | Low | High | Good conditions |
| 6/10 | 66% | 40% | Medium | Medium | Normal |
| 6/12 | 100% | 50% | Higher | Lower | Poor conditions |

**Recommendation:** FEC 6/12 (default) for reliable FPV

## Galois Field Arithmetic

### GF(2^8) Operations

**Addition:** XOR
```c
gf gf_add(gf a, gf b) {
    return a ^ b;
}
```

**Multiplication:** Lookup tables
```c
extern uint8_t gf_log[256];
extern uint8_t gf_exp[256];

gf gf_mul(gf a, gf b) {
    if (a == 0 || b == 0) return 0;
    return gf_exp[(gf_log[a] + gf_log[b]) % 255];
}
```

**Division:**
```c
gf gf_div(gf a, gf b) {
    if (b == 0) return 0;  // Undefined
    if (a == 0) return 0;
    return gf_exp[(gf_log[a] - gf_log[b] + 255) % 255];
}
```

### Matrix Inversion

The decoder must invert the k×k submatrix corresponding to received packets:

```c
// Gaussian elimination in GF(2^8)
void invert_matrix(gf* matrix, int k) {
    // Create augmented matrix [A | I]
    gf aug[k][2*k];
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            aug[i][j] = matrix[i*k + j];
        }
        for (int j = k; j < 2*k; j++) {
            aug[i][j] = (j-k == i) ? 1 : 0;
        }
    }

    // Forward elimination
    for (int i = 0; i < k; i++) {
        // Find pivot
        gf pivot = aug[i][i];
        if (pivot == 0) {
            // Swap rows
            // ...
        }

        // Scale row
        gf inv_pivot = gf_inv(pivot);
        for (int j = 0; j < 2*k; j++) {
            aug[i][j] = gf_mul(aug[i][j], inv_pivot);
        }

        // Eliminate column
        for (int j = 0; j < k; j++) {
            if (i != j) {
                gf factor = aug[j][i];
                for (int l = 0; l < 2*k; l++) {
                    aug[j][l] = gf_add(aug[j][l],
                                      gf_mul(factor, aug[i][l]));
                }
            }
        }
    }

    // Extract inverse from [I | A^-1]
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            matrix[i*k + j] = aug[i][k+j];
        }
    }
}
```

## Debugging and Monitoring

### Statistics

```c
struct FEC_Stats {
    uint32_t blocks_encoded;
    uint32_t blocks_decoded;
    uint32_t blocks_recovered;  // Required FEC decode
    uint32_t blocks_lost;       // < K packets
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_lost;

    float packet_loss_rate;
    float block_success_rate;
};

// Calculate
stats.packet_loss_rate = 1.0 - (packets_received / packets_sent);
stats.block_success_rate = blocks_decoded / (blocks_decoded + blocks_lost);
```

### Profiling

```c
// Encoder timing
uint64_t start = esp_timer_get_time();
fec_encode(/* ... */);
uint64_t encode_time_us = esp_timer_get_time() - start;

// Decoder timing
start = esp_timer_get_time();
fec_decode(/* ... */);
uint64_t decode_time_us = esp_timer_get_time() - start;
```

## See Also

- [02_PROTOCOL_SPECIFICATION.md](02_PROTOCOL_SPECIFICATION.md) - Protocol layer
- [03_VIDEO_PIPELINE.md](03_VIDEO_PIPELINE.md) - Video integration
- [06_GROUND_STATION.md](06_GROUND_STATION.md) - GS decoder usage
