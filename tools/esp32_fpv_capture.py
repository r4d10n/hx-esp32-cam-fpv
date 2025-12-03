#!/usr/bin/env python3
"""
ESP32-CAM FPV Packet Capture and Frame Extraction Tool

This tool captures packets from ESP32-CAM FPV transmissions and extracts
JPEG frames. It can read from pcapng files or capture live from a monitor
mode interface.

Compatible with hx-esp32-cam-fpv protocol v0.4 (RomanLut/hx-esp32-cam-fpv)

Protocol structure:
- FEC Packet_Header (12 bytes): version, signature, device IDs, size, block/packet indices
- Air2Ground_Video_Packet (18 bytes): type, size, resolution, part_index, last_part, frame_index
- JPEG payload follows the headers

Features:
- FEC decoding support using zfec library (can recover from up to 50% packet loss)
- Supports both pcapng file input and live capture

Usage:
    # From pcapng file:
    python esp32_fpv_capture.py --input capture.pcapng --output frames/

    # Live capture (requires root and monitor mode interface):
    python esp32_fpv_capture.py --interface wlan0mon --output frames/

    # Enable FEC decoding (requires: pip install zfec):
    python esp32_fpv_capture.py --input capture.pcapng --output frames/ --fec
"""

import argparse
import os
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Optional, Dict, List, Tuple

try:
    import dpkt
    HAS_DPKT = True
except ImportError:
    HAS_DPKT = False

# Try to import zfec for FEC decoding
HAS_ZFEC = False
try:
    import zfec
    HAS_ZFEC = True
except ImportError:
    pass

# Try to import pcap for live capture (more reliable than scapy for monitor mode)
HAS_PCAP = False
try:
    import pcap
    HAS_PCAP = True
except ImportError:
    pass

# Scapy as fallback for live capture (has issues in some environments)
HAS_SCAPY = False
try:
    from scapy.all import sniff, conf
    # Disable scapy's verbose output
    conf.verb = 0
    HAS_SCAPY = True
except (ImportError, Exception):
    # Scapy may fail to import due to cryptography/cffi issues
    pass

# Protocol constants
PACKET_SIGNATURE = 0x38  # 56 decimal
PACKET_VERSION = 3       # Current version in captures (was 2 in older code)
DEFAULT_FEC_K = 6        # Default number of primary data packets per FEC block
DEFAULT_FEC_N = 12       # Default total packets per FEC block (K primary + N-K parity)

# Packet type enumeration
class PacketType:
    VIDEO = 0
    TELEMETRY = 1
    OSD = 2
    CONFIG = 3

# Resolution enumeration
RESOLUTIONS = {
    0: (320, 240),    # QVGA
    1: (400, 296),    # CIF
    2: (480, 320),    # HVGA
    3: (640, 480),    # VGA
    4: (640, 360),    # VGA16
    5: (800, 600),    # SVGA
    6: (800, 456),    # SVGA16
    7: (1024, 768),   # XGA
    8: (1024, 576),   # XGA16
    9: (1280, 960),   # SXGA
    10: (1280, 720),  # HD
    11: (1600, 1200), # UXGA
}


@dataclass
class FECHeader:
    """FEC Packet_Header structure (12 bytes)"""
    version: int
    signature: int
    from_device_id: int
    to_device_id: int
    size: int
    block_index: int
    packet_index: int

    @classmethod
    def from_bytes(cls, data: bytes) -> Optional['FECHeader']:
        """Parse FEC header from raw bytes"""
        if len(data) < 12:
            return None

        version = data[0]
        signature = data[1]

        if signature != PACKET_SIGNATURE:
            return None

        from_device_id = struct.unpack('<H', data[2:4])[0]
        to_device_id = struct.unpack('<H', data[4:6])[0]
        size = struct.unpack('<H', data[6:8])[0]
        block_pkt = struct.unpack('<I', data[8:12])[0]
        block_index = block_pkt & 0xFFFFFF
        packet_index = (block_pkt >> 24) & 0xFF

        return cls(
            version=version,
            signature=signature,
            from_device_id=from_device_id,
            to_device_id=to_device_id,
            size=size,
            block_index=block_index,
            packet_index=packet_index
        )


@dataclass
class VideoPacketHeader:
    """Air2Ground_Video_Packet header structure (18 bytes)"""
    packet_type: int
    size: int
    pong: int
    version: int
    crc: int
    air_device_id: int
    gs_device_id: int
    resolution: int
    part_index: int
    last_part: bool
    frame_index: int

    @classmethod
    def from_bytes(cls, data: bytes) -> Optional['VideoPacketHeader']:
        """Parse video packet header from raw bytes"""
        if len(data) < 18:
            return None

        packet_type = data[0]
        size = struct.unpack('<I', data[1:5])[0]
        pong = data[5]
        version = data[6]
        crc = data[7]
        air_device_id = struct.unpack('<H', data[8:10])[0]
        gs_device_id = struct.unpack('<H', data[10:12])[0]
        resolution = data[12]
        part_last = data[13]
        part_index = part_last & 0x7F
        last_part = bool((part_last >> 7) & 0x1)
        frame_index = struct.unpack('<I', data[14:18])[0]

        return cls(
            packet_type=packet_type,
            size=size,
            pong=pong,
            version=version,
            crc=crc,
            air_device_id=air_device_id,
            gs_device_id=gs_device_id,
            resolution=resolution,
            part_index=part_index,
            last_part=last_part,
            frame_index=frame_index
        )


@dataclass
class VideoPart:
    """A single part of a video frame"""
    part_index: int
    last_part: bool
    data: bytes


@dataclass
class FECBlock:
    """Holds packets for a single FEC block"""
    block_index: int
    packets: Dict[int, bytes] = field(default_factory=dict)  # packet_index -> payload data
    payload_size: int = 0  # Size of payloads in this block


class FECDecoder:
    """Handles FEC block collection and decoding"""

    def __init__(self, k: int = DEFAULT_FEC_K, n: int = DEFAULT_FEC_N, verbose: bool = False):
        self.k = k
        self.n = n
        self.verbose = verbose
        self.blocks: Dict[int, FECBlock] = {}
        self.decoder = None
        self.stats = {
            'blocks_complete': 0,
            'blocks_recovered': 0,
            'blocks_failed': 0,
        }

        if HAS_ZFEC:
            self.decoder = zfec.Decoder(k, n)

    def add_packet(self, block_index: int, packet_index: int, payload: bytes) -> Optional[List[bytes]]:
        """
        Add a packet to its FEC block.
        Returns list of decoded primary payloads if block can be decoded, None otherwise.
        """
        # Get or create block
        if block_index not in self.blocks:
            self.blocks[block_index] = FECBlock(block_index=block_index)

        block = self.blocks[block_index]

        # Skip duplicate packets
        if packet_index in block.packets:
            return None

        # Store packet
        block.packets[packet_index] = payload
        if block.payload_size == 0:
            block.payload_size = len(payload)

        # Check if we can decode
        if len(block.packets) >= self.k:
            result = self._try_decode_block(block)
            if result is not None:
                # Clean up old blocks
                self._cleanup_old_blocks(block_index)
            return result

        return None

    def _try_decode_block(self, block: FECBlock) -> Optional[List[bytes]]:
        """Try to decode a block, returns list of K primary payloads or None"""
        # Count primary packets we have
        primary_packets = {idx: data for idx, data in block.packets.items() if idx < self.k}

        # If we have all primary packets, no decoding needed
        if len(primary_packets) == self.k:
            self.stats['blocks_complete'] += 1
            result = [primary_packets[i] for i in range(self.k)]
            return result

        # Need FEC decoding
        if not HAS_ZFEC or self.decoder is None:
            self.stats['blocks_failed'] += 1
            return None

        # Collect K packets (mix of primary and FEC)
        available = sorted(block.packets.keys())[:self.k]

        if len(available) < self.k:
            return None

        try:
            # Prepare data for zfec decoder
            # zfec.Decoder.decode expects (shares, sharenums) where shares are padded to same length
            shares = []
            sharenums = []

            # Ensure all packets are same size (pad if needed)
            max_size = max(len(block.packets[idx]) for idx in available)

            for idx in available:
                data = block.packets[idx]
                if len(data) < max_size:
                    data = data + b'\x00' * (max_size - len(data))
                shares.append(data)
                sharenums.append(idx)

            # Decode
            decoded = self.decoder.decode(shares, sharenums)

            self.stats['blocks_recovered'] += 1
            if self.verbose:
                missing = [i for i in range(self.k) if i not in primary_packets]
                print(f"Block {block.block_index}: Recovered packets {missing} using FEC")

            return list(decoded)

        except Exception as e:
            if self.verbose:
                print(f"Block {block.block_index}: FEC decode failed: {e}")
            self.stats['blocks_failed'] += 1
            return None

    def _cleanup_old_blocks(self, current_block: int):
        """Remove blocks that are too old"""
        old_blocks = [idx for idx in self.blocks.keys() if idx < current_block - 100]
        for idx in old_blocks:
            if len(self.blocks[idx].packets) < self.k:
                self.stats['blocks_failed'] += 1
            del self.blocks[idx]

    def finalize(self):
        """Clean up remaining blocks"""
        for block in self.blocks.values():
            if len(block.packets) < self.k:
                self.stats['blocks_failed'] += 1


class FrameAssembler:
    """Assembles complete JPEG frames from video packet parts"""

    def __init__(self, output_dir: str, fec_k: int = DEFAULT_FEC_K, verbose: bool = False):
        self.output_dir = output_dir
        self.fec_k = fec_k
        self.verbose = verbose
        self.frames: Dict[int, Dict[int, VideoPart]] = defaultdict(dict)
        self.completed_frames: set = set()
        self.frame_count = 0
        self.stats = {
            'packets_processed': 0,
            'primary_packets': 0,
            'fec_packets': 0,
            'frames_saved': 0,
            'frames_incomplete': 0,
        }

        os.makedirs(output_dir, exist_ok=True)

    def add_part(self, frame_index: int, part: VideoPart) -> Optional[str]:
        """
        Add a video part to the frame buffer.
        Returns the saved filename if frame is complete, None otherwise.
        """
        if frame_index in self.completed_frames:
            return None

        self.frames[frame_index][part.part_index] = part

        # Check if frame is complete
        if part.last_part:
            return self._try_assemble_frame(frame_index, part.part_index)

        return None

    def _try_assemble_frame(self, frame_index: int, last_part_index: int) -> Optional[str]:
        """Try to assemble a complete frame"""
        parts = self.frames[frame_index]

        # Check if we have all parts from 0 to last_part_index
        missing_parts = []
        for i in range(last_part_index + 1):
            if i not in parts:
                missing_parts.append(i)

        if missing_parts:
            if self.verbose:
                print(f"Frame {frame_index}: Missing parts {missing_parts}")
            self.stats['frames_incomplete'] += 1
            # Don't delete yet - might get missing parts later
            return None

        # Assemble the frame
        jpeg_data = b''
        for i in range(last_part_index + 1):
            jpeg_data += parts[i].data

        # Validate JPEG (should start with FFD8 and end with FFD9)
        if len(jpeg_data) < 4:
            if self.verbose:
                print(f"Frame {frame_index}: Too small ({len(jpeg_data)} bytes)")
            return None

        if jpeg_data[:2] != b'\xff\xd8':
            if self.verbose:
                print(f"Frame {frame_index}: Invalid JPEG header: {jpeg_data[:4].hex()}")
            return None

        # Save the frame
        filename = os.path.join(self.output_dir, f"frame_{frame_index:08d}.jpg")
        with open(filename, 'wb') as f:
            f.write(jpeg_data)

        self.completed_frames.add(frame_index)
        self.stats['frames_saved'] += 1
        self.frame_count += 1

        # Clean up
        del self.frames[frame_index]

        if self.verbose:
            print(f"Saved {filename} ({len(jpeg_data)} bytes, {last_part_index + 1} parts)")

        return filename

    def finalize(self) -> None:
        """Clean up any remaining incomplete frames"""
        for frame_index in list(self.frames.keys()):
            if self.verbose:
                parts = self.frames[frame_index]
                print(f"Frame {frame_index}: Incomplete, had parts {sorted(parts.keys())}")
            self.stats['frames_incomplete'] += 1
        self.frames.clear()


def find_fec_header(data: bytes, start: int = 0, end: int = None) -> int:
    """Find the offset of FEC header in raw packet data"""
    if end is None:
        end = min(len(data), 150)

    for pos in range(start, end - 1):
        if data[pos + 1] == PACKET_SIGNATURE:
            return pos
    return -1


def extract_video_part(payload: bytes) -> Optional[Tuple[VideoPart, int]]:
    """Extract video part from FEC payload, returns (VideoPart, frame_index) or None"""
    if len(payload) < 18:
        return None

    video_header = VideoPacketHeader.from_bytes(payload[:18])
    if video_header is None:
        return None

    # Only process video packets
    if video_header.packet_type != PacketType.VIDEO:
        return None

    jpeg_data = payload[18:]
    if len(jpeg_data) == 0:
        return None

    part = VideoPart(
        part_index=video_header.part_index,
        last_part=video_header.last_part,
        data=jpeg_data
    )

    return (part, video_header.frame_index)


def process_packet_with_fec(raw_data: bytes, fec_decoder: FECDecoder,
                            assembler: FrameAssembler) -> bool:
    """Process packet with FEC decoding support"""
    # Find FEC header
    fec_offset = find_fec_header(raw_data, start=40, end=100)
    if fec_offset < 0:
        return False

    # Parse FEC header
    fec_header = FECHeader.from_bytes(raw_data[fec_offset:fec_offset + 12])
    if fec_header is None:
        return False

    assembler.stats['packets_processed'] += 1

    if fec_header.packet_index < fec_decoder.k:
        assembler.stats['primary_packets'] += 1
    else:
        assembler.stats['fec_packets'] += 1

    # Extract payload (everything after FEC header)
    payload_start = fec_offset + 12
    payload_end = fec_offset + 12 + fec_header.size
    if payload_end > len(raw_data):
        payload_end = len(raw_data)

    payload = raw_data[payload_start:payload_end]

    # Add to FEC decoder
    decoded_payloads = fec_decoder.add_packet(
        fec_header.block_index,
        fec_header.packet_index,
        payload
    )

    # If block was decoded, process all primary payloads
    if decoded_payloads:
        for payload in decoded_payloads:
            result = extract_video_part(payload)
            if result:
                part, frame_index = result
                assembler.add_part(frame_index, part)

    return True


def process_packet_simple(raw_data: bytes, assembler: FrameAssembler) -> bool:
    """Process packet without FEC decoding (simple mode)"""
    # Find FEC header
    fec_offset = find_fec_header(raw_data, start=40, end=100)
    if fec_offset < 0:
        return False

    # Parse FEC header
    fec_header = FECHeader.from_bytes(raw_data[fec_offset:fec_offset + 12])
    if fec_header is None:
        return False

    assembler.stats['packets_processed'] += 1

    # Skip FEC parity packets (only process primary data packets)
    if fec_header.packet_index >= assembler.fec_k:
        assembler.stats['fec_packets'] += 1
        return True

    assembler.stats['primary_packets'] += 1

    # Extract payload
    payload_start = fec_offset + 12
    payload_end = fec_offset + 12 + fec_header.size
    if payload_end > len(raw_data):
        payload_end = len(raw_data)

    payload = raw_data[payload_start:payload_end]

    # Extract video part
    result = extract_video_part(payload)
    if result:
        part, frame_index = result
        assembler.add_part(frame_index, part)

    return True


def process_pcapng_file(input_file: str, assembler: FrameAssembler,
                        fec_decoder: Optional[FECDecoder] = None) -> None:
    """Process packets from a pcapng file"""
    if not HAS_DPKT:
        print("Error: dpkt library is required for pcapng file processing")
        print("Install with: pip install dpkt")
        sys.exit(1)

    print(f"Reading {input_file}...")
    if fec_decoder:
        print("FEC decoding enabled")

    with open(input_file, 'rb') as f:
        try:
            pcap_reader = dpkt.pcapng.Reader(f)
        except ValueError:
            # Try as regular pcap
            f.seek(0)
            pcap_reader = dpkt.pcap.Reader(f)

        packet_count = 0
        for ts, buf in pcap_reader:
            if fec_decoder:
                process_packet_with_fec(buf, fec_decoder, assembler)
            else:
                process_packet_simple(buf, assembler)
            packet_count += 1

            if packet_count % 1000 == 0:
                print(f"Processed {packet_count} packets, {assembler.stats['frames_saved']} frames saved...")

    print(f"Finished processing {packet_count} packets")


def process_live_capture_pcap(interface: str, assembler: FrameAssembler,
                              fec_decoder: Optional[FECDecoder] = None,
                              count: int = 0, timeout: int = None) -> None:
    """Capture using pypcap library (preferred for monitor mode)"""
    print(f"Starting live capture on {interface} using pcap...")
    if fec_decoder:
        print("FEC decoding enabled")
    print("Press Ctrl+C to stop")

    try:
        pc = pcap.pcap(name=interface, promisc=True, immediate=True,
                       timeout_ms=1000 if timeout else 0)

        packet_count = 0
        try:
            for ts, buf in pc:
                if fec_decoder:
                    process_packet_with_fec(buf, fec_decoder, assembler)
                else:
                    process_packet_simple(buf, assembler)
                packet_count += 1

                if packet_count % 100 == 0:
                    print(f"Captured {packet_count} packets, {assembler.stats['frames_saved']} frames...")

                if count > 0 and packet_count >= count:
                    break
        except KeyboardInterrupt:
            print("\nCapture stopped by user")

        print(f"Captured {packet_count} packets total")

    except Exception as e:
        print(f"Error during pcap capture: {e}")
        sys.exit(1)


def process_live_capture_scapy(interface: str, assembler: FrameAssembler,
                               fec_decoder: Optional[FECDecoder] = None,
                               count: int = 0, timeout: int = None) -> None:
    """Capture using scapy library (fallback)"""
    print(f"Starting live capture on {interface} using scapy...")
    if fec_decoder:
        print("FEC decoding enabled")
    print("Press Ctrl+C to stop")

    packet_count = [0]

    def packet_handler(pkt):
        try:
            raw_data = bytes(pkt)
            if fec_decoder:
                process_packet_with_fec(raw_data, fec_decoder, assembler)
            else:
                process_packet_simple(raw_data, assembler)
            packet_count[0] += 1

            if packet_count[0] % 100 == 0:
                print(f"Captured {packet_count[0]} packets, {assembler.stats['frames_saved']} frames...")
        except Exception as e:
            if assembler.verbose:
                print(f"Error processing packet: {e}")

    try:
        sniff(iface=interface, prn=packet_handler,
              count=count if count > 0 else 0,
              timeout=timeout, store=False,
              monitor=True)
    except KeyboardInterrupt:
        print("\nCapture stopped by user")
    except PermissionError:
        print("Error: Root privileges required for live capture")
        sys.exit(1)
    except Exception as e:
        print(f"Error during scapy capture: {e}")
        print("Try using pypcap instead: pip install pypcap")
        sys.exit(1)

    print(f"Captured {packet_count[0]} packets total")


def process_live_capture(interface: str, assembler: FrameAssembler,
                        fec_decoder: Optional[FECDecoder] = None,
                        count: int = 0, timeout: int = None) -> None:
    """Capture and process live packets from monitor mode interface"""
    if HAS_PCAP:
        process_live_capture_pcap(interface, assembler, fec_decoder, count, timeout)
    elif HAS_SCAPY:
        process_live_capture_scapy(interface, assembler, fec_decoder, count, timeout)
    else:
        print("Error: No packet capture library available for live capture")
        print("Install one of:")
        print("  pip install pypcap   (recommended for monitor mode)")
        print("  pip install scapy    (fallback)")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='ESP32-CAM FPV Packet Capture and Frame Extraction',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Extract frames from pcapng file (simple mode, no FEC):
    python esp32_fpv_capture.py -i capture.pcapng -o frames/

    # Extract with FEC decoding (recovers lost packets):
    python esp32_fpv_capture.py -i capture.pcapng -o frames/ --fec

    # Live capture from monitor mode interface:
    sudo python esp32_fpv_capture.py -I wlan0mon -o frames/ --fec

    # Extract with verbose output:
    python esp32_fpv_capture.py -i capture.pcapng -o frames/ -v --fec

Dependencies:
    pip install dpkt        # Required for pcapng file reading
    pip install zfec        # Required for --fec (FEC decoding)
    pip install pypcap      # Recommended for live capture
    pip install scapy       # Fallback for live capture
        """
    )

    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument('-i', '--input', metavar='FILE',
                            help='Input pcapng/pcap file')
    input_group.add_argument('-I', '--interface', metavar='IFACE',
                            help='Network interface in monitor mode for live capture')

    parser.add_argument('-o', '--output', metavar='DIR', default='frames',
                        help='Output directory for extracted frames (default: frames)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')
    parser.add_argument('-c', '--count', type=int, default=0,
                        help='Number of packets to capture (0 = unlimited, live capture only)')
    parser.add_argument('-t', '--timeout', type=int, default=None,
                        help='Capture timeout in seconds (live capture only)')
    parser.add_argument('-k', '--fec-k', type=int, default=DEFAULT_FEC_K,
                        help=f'FEC K value - number of primary data packets (default: {DEFAULT_FEC_K})')
    parser.add_argument('-n', '--fec-n', type=int, default=DEFAULT_FEC_N,
                        help=f'FEC N value - total packets per block (default: {DEFAULT_FEC_N})')
    parser.add_argument('--fec', action='store_true',
                        help='Enable FEC decoding (requires zfec library)')

    args = parser.parse_args()

    # Check for FEC support
    fec_decoder = None
    if args.fec:
        if not HAS_ZFEC:
            print("Error: zfec library required for FEC decoding")
            print("Install with: pip install zfec")
            sys.exit(1)
        fec_decoder = FECDecoder(k=args.fec_k, n=args.fec_n, verbose=args.verbose)

    # Create frame assembler
    assembler = FrameAssembler(args.output, fec_k=args.fec_k, verbose=args.verbose)

    try:
        if args.input:
            if not os.path.exists(args.input):
                print(f"Error: Input file '{args.input}' not found")
                sys.exit(1)
            process_pcapng_file(args.input, assembler, fec_decoder)
        else:
            process_live_capture(args.interface, assembler, fec_decoder,
                               count=args.count, timeout=args.timeout)
    finally:
        # Finalize
        if fec_decoder:
            fec_decoder.finalize()
        assembler.finalize()

        print("\n=== Statistics ===")
        print(f"Packets processed: {assembler.stats['packets_processed']}")
        print(f"  Primary packets: {assembler.stats['primary_packets']}")
        print(f"  FEC packets:     {assembler.stats['fec_packets']}")
        if fec_decoder:
            print(f"FEC blocks complete:  {fec_decoder.stats['blocks_complete']}")
            print(f"FEC blocks recovered: {fec_decoder.stats['blocks_recovered']}")
            print(f"FEC blocks failed:    {fec_decoder.stats['blocks_failed']}")
        print(f"Frames saved:      {assembler.stats['frames_saved']}")
        print(f"Frames incomplete: {assembler.stats['frames_incomplete']}")
        print(f"\nOutput directory: {os.path.abspath(args.output)}")


if __name__ == '__main__':
    main()
