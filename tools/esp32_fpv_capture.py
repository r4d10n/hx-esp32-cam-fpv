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

Usage:
    # From pcapng file:
    python esp32_fpv_capture.py --input capture.pcapng --output frames/

    # Live capture (requires root and monitor mode interface):
    python esp32_fpv_capture.py --interface wlan0mon --output frames/

    # Specify custom FEC K value (default: 6):
    python esp32_fpv_capture.py --input capture.pcapng --output frames/ --fec-k 8
"""

import argparse
import os
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass
from typing import Optional, Dict, List, Tuple

try:
    import dpkt
    HAS_DPKT = True
except ImportError:
    HAS_DPKT = False

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


def process_packet(raw_data: bytes, assembler: FrameAssembler) -> bool:
    """
    Process a single raw packet and extract video data.
    Returns True if packet was processed successfully.
    """
    # Find FEC header in the packet
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

    # Parse video packet header
    video_offset = fec_offset + 12
    if len(raw_data) < video_offset + 18:
        return False

    video_header = VideoPacketHeader.from_bytes(raw_data[video_offset:video_offset + 18])
    if video_header is None:
        return False

    # Only process video packets
    if video_header.packet_type != PacketType.VIDEO:
        return True

    # Extract JPEG payload
    payload_offset = video_offset + 18
    payload_end = fec_offset + 12 + fec_header.size

    if payload_end > len(raw_data):
        payload_end = len(raw_data)

    jpeg_data = raw_data[payload_offset:payload_end]

    if len(jpeg_data) == 0:
        return False

    # Add to frame assembler
    part = VideoPart(
        part_index=video_header.part_index,
        last_part=video_header.last_part,
        data=jpeg_data
    )

    assembler.add_part(video_header.frame_index, part)
    return True


def process_pcapng_file(input_file: str, assembler: FrameAssembler) -> None:
    """Process packets from a pcapng file"""
    if not HAS_DPKT:
        print("Error: dpkt library is required for pcapng file processing")
        print("Install with: pip install dpkt")
        sys.exit(1)

    print(f"Reading {input_file}...")

    with open(input_file, 'rb') as f:
        try:
            pcap = dpkt.pcapng.Reader(f)
        except ValueError:
            # Try as regular pcap
            f.seek(0)
            pcap = dpkt.pcap.Reader(f)

        packet_count = 0
        for ts, buf in pcap:
            process_packet(buf, assembler)
            packet_count += 1

            if packet_count % 1000 == 0:
                print(f"Processed {packet_count} packets, {assembler.stats['frames_saved']} frames saved...")

    print(f"Finished processing {packet_count} packets")


def process_live_capture_pcap(interface: str, assembler: FrameAssembler,
                              count: int = 0, timeout: int = None) -> None:
    """Capture using pypcap library (preferred for monitor mode)"""
    print(f"Starting live capture on {interface} using pcap...")
    print("Press Ctrl+C to stop")

    try:
        # Create pcap handle for monitor mode interface
        pc = pcap.pcap(name=interface, promisc=True, immediate=True,
                       timeout_ms=1000 if timeout else 0)

        packet_count = 0
        try:
            for ts, buf in pc:
                process_packet(buf, assembler)
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
                               count: int = 0, timeout: int = None) -> None:
    """Capture using scapy library (fallback)"""
    print(f"Starting live capture on {interface} using scapy...")
    print("Press Ctrl+C to stop")

    packet_count = [0]  # Use list to allow modification in nested function

    def packet_handler(pkt):
        try:
            raw_data = bytes(pkt)
            process_packet(raw_data, assembler)
            packet_count[0] += 1

            if packet_count[0] % 100 == 0:
                print(f"Captured {packet_count[0]} packets, {assembler.stats['frames_saved']} frames...")
        except Exception as e:
            if assembler.verbose:
                print(f"Error processing packet: {e}")

    try:
        # Use L2socket explicitly for monitor mode
        sniff(iface=interface, prn=packet_handler,
              count=count if count > 0 else 0,
              timeout=timeout, store=False,
              monitor=True)  # Enable monitor mode in scapy
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
                        count: int = 0, timeout: int = None) -> None:
    """Capture and process live packets from monitor mode interface"""
    # Prefer pcap over scapy for monitor mode (more reliable)
    if HAS_PCAP:
        process_live_capture_pcap(interface, assembler, count, timeout)
    elif HAS_SCAPY:
        process_live_capture_scapy(interface, assembler, count, timeout)
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
    # Extract frames from pcapng file:
    python esp32_fpv_capture.py -i capture.pcapng -o frames/

    # Live capture from monitor mode interface:
    sudo python esp32_fpv_capture.py -I wlan0mon -o frames/

    # Extract with verbose output:
    python esp32_fpv_capture.py -i capture.pcapng -o frames/ -v

    # Use custom FEC K value:
    python esp32_fpv_capture.py -i capture.pcapng -o frames/ --fec-k 8

Dependencies for live capture (install one):
    pip install pypcap   # Recommended - uses libpcap directly
    pip install scapy    # Fallback - may have issues with monitor mode
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

    args = parser.parse_args()

    # Create frame assembler
    assembler = FrameAssembler(args.output, fec_k=args.fec_k, verbose=args.verbose)

    try:
        if args.input:
            if not os.path.exists(args.input):
                print(f"Error: Input file '{args.input}' not found")
                sys.exit(1)
            process_pcapng_file(args.input, assembler)
        else:
            process_live_capture(args.interface, assembler,
                               count=args.count, timeout=args.timeout)
    finally:
        # Finalize and print stats
        assembler.finalize()

        print("\n=== Statistics ===")
        print(f"Packets processed: {assembler.stats['packets_processed']}")
        print(f"  Primary packets: {assembler.stats['primary_packets']}")
        print(f"  FEC packets:     {assembler.stats['fec_packets']}")
        print(f"Frames saved:      {assembler.stats['frames_saved']}")
        print(f"Frames incomplete: {assembler.stats['frames_incomplete']}")
        print(f"\nOutput directory: {os.path.abspath(args.output)}")


if __name__ == '__main__':
    main()
