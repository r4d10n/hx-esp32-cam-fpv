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
- Live frame display using OpenCV
- Video recording to file (AVI/MP4)
- Optional frame saving to disk

Usage:
    # Live capture with display:
    sudo python esp32_fpv_capture.py -I wlan0mon --fec --display

    # Live capture and save video:
    sudo python esp32_fpv_capture.py -I wlan0mon --fec --display --video output.avi

    # Extract frames from pcapng:
    python esp32_fpv_capture.py -i capture.pcapng -o frames/ --fec

    # Display only (no frame saving):
    sudo python esp32_fpv_capture.py -I wlan0mon --fec --display --no-save
"""

import argparse
import os
import struct
import sys
import time
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Optional, Dict, List, Tuple, Callable

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

# Try to import OpenCV for display and video
HAS_CV2 = False
try:
    import cv2
    import numpy as np
    HAS_CV2 = True
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
    conf.verb = 0
    HAS_SCAPY = True
except (ImportError, Exception):
    pass

# Protocol constants
PACKET_SIGNATURE = 0x38  # 56 decimal
PACKET_VERSION = 3
DEFAULT_FEC_K = 6
DEFAULT_FEC_N = 12

class PacketType:
    VIDEO = 0
    TELEMETRY = 1
    OSD = 2
    CONFIG = 3

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
        return cls(version=version, signature=signature,
                   from_device_id=from_device_id, to_device_id=to_device_id,
                   size=size, block_index=block_index, packet_index=packet_index)


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
        return cls(packet_type=packet_type, size=size, pong=pong, version=version,
                   crc=crc, air_device_id=air_device_id, gs_device_id=gs_device_id,
                   resolution=resolution, part_index=part_index, last_part=last_part,
                   frame_index=frame_index)


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
    packets: Dict[int, bytes] = field(default_factory=dict)
    payload_size: int = 0


class FrameDisplay:
    """Handles live frame display and video recording"""

    def __init__(self, window_name: str = "ESP32-CAM FPV",
                 video_output: str = None, fps: float = 30.0,
                 enable_display: bool = True):
        self.window_name = window_name
        self.video_output = video_output
        self.fps = fps
        self.video_writer = None
        self.frame_size = None
        self.last_frame_time = 0
        self.frame_count = 0
        self.start_time = time.time()
        self.has_gui = False

        if HAS_CV2 and enable_display:
            try:
                cv2.namedWindow(self.window_name, cv2.WINDOW_NORMAL)
                self.has_gui = True
            except cv2.error as e:
                print(f"Warning: GUI display not available ({e})")
                print("Video recording will still work if --video is specified")
                self.has_gui = False

    def display_frame(self, jpeg_data: bytes) -> bool:
        """Display a JPEG frame. Returns False if window closed."""
        if not HAS_CV2:
            return True

        try:
            # Decode JPEG
            nparr = np.frombuffer(jpeg_data, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

            if frame is None:
                return True

            # Initialize video writer if needed
            if self.video_output and self.video_writer is None:
                h, w = frame.shape[:2]
                self.frame_size = (w, h)
                fourcc = cv2.VideoWriter_fourcc(*'MJPG')
                if self.video_output.endswith('.mp4'):
                    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
                self.video_writer = cv2.VideoWriter(
                    self.video_output, fourcc, self.fps, self.frame_size)

            # Write to video file
            if self.video_writer:
                self.video_writer.write(frame)

            # Only display if GUI is available
            if self.has_gui:
                # Calculate FPS
                self.frame_count += 1
                elapsed = time.time() - self.start_time
                current_fps = self.frame_count / elapsed if elapsed > 0 else 0

                # Add FPS overlay
                cv2.putText(frame, f"FPS: {current_fps:.1f}", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

                # Display frame
                cv2.imshow(self.window_name, frame)

                # Check for key press (q to quit)
                key = cv2.waitKey(1) & 0xFF
                if key == ord('q') or key == 27:  # q or ESC
                    return False

            return True

        except Exception as e:
            print(f"Display error: {e}")
            return True

    def close(self):
        """Clean up resources"""
        if self.video_writer:
            self.video_writer.release()
            print(f"Video saved to: {self.video_output}")
        if self.has_gui:
            cv2.destroyAllWindows()


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
        if block_index not in self.blocks:
            self.blocks[block_index] = FECBlock(block_index=block_index)
        block = self.blocks[block_index]
        if packet_index in block.packets:
            return None
        block.packets[packet_index] = payload
        if block.payload_size == 0:
            block.payload_size = len(payload)
        if len(block.packets) >= self.k:
            result = self._try_decode_block(block)
            if result is not None:
                self._cleanup_old_blocks(block_index)
            return result
        return None

    def _try_decode_block(self, block: FECBlock) -> Optional[List[bytes]]:
        primary_packets = {idx: data for idx, data in block.packets.items() if idx < self.k}
        if len(primary_packets) == self.k:
            self.stats['blocks_complete'] += 1
            return [primary_packets[i] for i in range(self.k)]
        if not HAS_ZFEC or self.decoder is None:
            self.stats['blocks_failed'] += 1
            return None
        available = sorted(block.packets.keys())[:self.k]
        if len(available) < self.k:
            return None
        try:
            shares = []
            sharenums = []
            max_size = max(len(block.packets[idx]) for idx in available)
            for idx in available:
                data = block.packets[idx]
                if len(data) < max_size:
                    data = data + b'\x00' * (max_size - len(data))
                shares.append(data)
                sharenums.append(idx)
            decoded = self.decoder.decode(shares, sharenums)
            self.stats['blocks_recovered'] += 1
            if self.verbose:
                missing = [i for i in range(self.k) if i not in primary_packets]
                print(f"Block {block.block_index}: Recovered {missing}")
            return list(decoded)
        except Exception as e:
            if self.verbose:
                print(f"Block {block.block_index}: FEC failed: {e}")
            self.stats['blocks_failed'] += 1
            return None

    def _cleanup_old_blocks(self, current_block: int):
        old_blocks = [idx for idx in self.blocks.keys() if idx < current_block - 100]
        for idx in old_blocks:
            if len(self.blocks[idx].packets) < self.k:
                self.stats['blocks_failed'] += 1
            del self.blocks[idx]

    def finalize(self):
        for block in self.blocks.values():
            if len(block.packets) < self.k:
                self.stats['blocks_failed'] += 1


class FrameAssembler:
    """Assembles complete JPEG frames from video packet parts"""

    def __init__(self, output_dir: str = None, fec_k: int = DEFAULT_FEC_K,
                 verbose: bool = False, save_frames: bool = True,
                 on_frame_complete: Callable[[bytes, int], None] = None):
        self.output_dir = output_dir
        self.fec_k = fec_k
        self.verbose = verbose
        self.save_frames = save_frames
        self.on_frame_complete = on_frame_complete
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
        if output_dir and save_frames:
            os.makedirs(output_dir, exist_ok=True)

    def add_part(self, frame_index: int, part: VideoPart) -> Optional[str]:
        if frame_index in self.completed_frames:
            return None
        self.frames[frame_index][part.part_index] = part
        if part.last_part:
            return self._try_assemble_frame(frame_index, part.part_index)
        return None

    def _try_assemble_frame(self, frame_index: int, last_part_index: int) -> Optional[str]:
        parts = self.frames[frame_index]
        missing_parts = [i for i in range(last_part_index + 1) if i not in parts]
        if missing_parts:
            if self.verbose:
                print(f"Frame {frame_index}: Missing {missing_parts}")
            self.stats['frames_incomplete'] += 1
            return None

        jpeg_data = b''.join(parts[i].data for i in range(last_part_index + 1))

        if len(jpeg_data) < 4 or jpeg_data[:2] != b'\xff\xd8':
            if self.verbose:
                print(f"Frame {frame_index}: Invalid JPEG")
            return None

        # Call frame complete callback (for display)
        if self.on_frame_complete:
            self.on_frame_complete(jpeg_data, frame_index)

        # Save frame to disk
        filename = None
        if self.save_frames and self.output_dir:
            filename = os.path.join(self.output_dir, f"frame_{frame_index:08d}.jpg")
            with open(filename, 'wb') as f:
                f.write(jpeg_data)
            if self.verbose:
                print(f"Saved {filename} ({len(jpeg_data)} bytes)")

        self.completed_frames.add(frame_index)
        self.stats['frames_saved'] += 1
        self.frame_count += 1
        del self.frames[frame_index]
        return filename

    def finalize(self) -> None:
        for frame_index in list(self.frames.keys()):
            if self.verbose:
                parts = self.frames[frame_index]
                print(f"Frame {frame_index}: Incomplete, had {sorted(parts.keys())}")
            self.stats['frames_incomplete'] += 1
        self.frames.clear()


def find_fec_header(data: bytes, start: int = 0, end: int = None) -> int:
    if end is None:
        end = min(len(data), 150)
    for pos in range(start, end - 1):
        if data[pos + 1] == PACKET_SIGNATURE:
            return pos
    return -1


def extract_video_part(payload: bytes) -> Optional[Tuple[VideoPart, int]]:
    if len(payload) < 18:
        return None
    video_header = VideoPacketHeader.from_bytes(payload[:18])
    if video_header is None or video_header.packet_type != PacketType.VIDEO:
        return None
    jpeg_data = payload[18:]
    if len(jpeg_data) == 0:
        return None
    part = VideoPart(part_index=video_header.part_index,
                     last_part=video_header.last_part, data=jpeg_data)
    return (part, video_header.frame_index)


def process_packet_with_fec(raw_data: bytes, fec_decoder: FECDecoder,
                            assembler: FrameAssembler) -> bool:
    fec_offset = find_fec_header(raw_data, start=40, end=100)
    if fec_offset < 0:
        return False
    fec_header = FECHeader.from_bytes(raw_data[fec_offset:fec_offset + 12])
    if fec_header is None:
        return False
    assembler.stats['packets_processed'] += 1
    if fec_header.packet_index < fec_decoder.k:
        assembler.stats['primary_packets'] += 1
    else:
        assembler.stats['fec_packets'] += 1
    payload_start = fec_offset + 12
    payload_end = min(fec_offset + 12 + fec_header.size, len(raw_data))
    payload = raw_data[payload_start:payload_end]
    decoded_payloads = fec_decoder.add_packet(
        fec_header.block_index, fec_header.packet_index, payload)
    if decoded_payloads:
        for p in decoded_payloads:
            result = extract_video_part(p)
            if result:
                part, frame_index = result
                assembler.add_part(frame_index, part)
    return True


def process_packet_simple(raw_data: bytes, assembler: FrameAssembler) -> bool:
    fec_offset = find_fec_header(raw_data, start=40, end=100)
    if fec_offset < 0:
        return False
    fec_header = FECHeader.from_bytes(raw_data[fec_offset:fec_offset + 12])
    if fec_header is None:
        return False
    assembler.stats['packets_processed'] += 1
    if fec_header.packet_index >= assembler.fec_k:
        assembler.stats['fec_packets'] += 1
        return True
    assembler.stats['primary_packets'] += 1
    payload_start = fec_offset + 12
    payload_end = min(fec_offset + 12 + fec_header.size, len(raw_data))
    payload = raw_data[payload_start:payload_end]
    result = extract_video_part(payload)
    if result:
        part, frame_index = result
        assembler.add_part(frame_index, part)
    return True


def process_pcapng_file(input_file: str, assembler: FrameAssembler,
                        fec_decoder: Optional[FECDecoder] = None,
                        display: Optional[FrameDisplay] = None) -> None:
    if not HAS_DPKT:
        print("Error: dpkt required. Install: pip install dpkt")
        sys.exit(1)

    print(f"Reading {input_file}...")
    if fec_decoder:
        print("FEC decoding enabled")

    with open(input_file, 'rb') as f:
        try:
            pcap_reader = dpkt.pcapng.Reader(f)
        except ValueError:
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
                print(f"Processed {packet_count} packets, {assembler.stats['frames_saved']} frames...")

            # Check for quit
            if display and not display.display_frame(b''):
                break

    print(f"Finished processing {packet_count} packets")


def process_live_capture(interface: str, assembler: FrameAssembler,
                        fec_decoder: Optional[FECDecoder] = None,
                        display: Optional[FrameDisplay] = None,
                        count: int = 0, timeout: int = None) -> None:
    if HAS_PCAP:
        _capture_with_pcap(interface, assembler, fec_decoder, display, count, timeout)
    elif HAS_SCAPY:
        _capture_with_scapy(interface, assembler, fec_decoder, display, count, timeout)
    else:
        print("Error: No capture library. Install: pip install pypcap")
        sys.exit(1)


def _capture_with_pcap(interface: str, assembler: FrameAssembler,
                       fec_decoder: Optional[FECDecoder],
                       display: Optional[FrameDisplay],
                       count: int, timeout: int) -> None:
    print(f"Starting capture on {interface} using pcap...")
    if fec_decoder:
        print("FEC decoding enabled")
    if display:
        print("Live display enabled (press 'q' to quit)")
    print("Press Ctrl+C to stop")

    try:
        pc = pcap.pcap(name=interface, promisc=True, immediate=True,
                       timeout_ms=1000 if timeout else 0)
        packet_count = 0
        running = True

        try:
            for ts, buf in pc:
                if not running:
                    break

                if fec_decoder:
                    process_packet_with_fec(buf, fec_decoder, assembler)
                else:
                    process_packet_simple(buf, assembler)
                packet_count += 1

                if packet_count % 100 == 0:
                    print(f"\rCaptured {packet_count} pkts, {assembler.stats['frames_saved']} frames", end='', flush=True)

                if count > 0 and packet_count >= count:
                    break

                # Process display events
                if display and HAS_CV2:
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord('q') or key == 27:
                        running = False

        except KeyboardInterrupt:
            print("\nCapture stopped")

        print(f"\nCaptured {packet_count} packets total")

    except Exception as e:
        print(f"Capture error: {e}")
        sys.exit(1)


def _capture_with_scapy(interface: str, assembler: FrameAssembler,
                        fec_decoder: Optional[FECDecoder],
                        display: Optional[FrameDisplay],
                        count: int, timeout: int) -> None:
    print(f"Starting capture on {interface} using scapy...")
    if fec_decoder:
        print("FEC decoding enabled")
    print("Press Ctrl+C to stop")

    packet_count = [0]
    running = [True]

    def packet_handler(pkt):
        if not running[0]:
            return
        try:
            raw_data = bytes(pkt)
            if fec_decoder:
                process_packet_with_fec(raw_data, fec_decoder, assembler)
            else:
                process_packet_simple(raw_data, assembler)
            packet_count[0] += 1

            if packet_count[0] % 100 == 0:
                print(f"\rCaptured {packet_count[0]} pkts, {assembler.stats['frames_saved']} frames", end='', flush=True)

            if display and HAS_CV2:
                key = cv2.waitKey(1) & 0xFF
                if key == ord('q') or key == 27:
                    running[0] = False

        except Exception as e:
            if assembler.verbose:
                print(f"Error: {e}")

    try:
        sniff(iface=interface, prn=packet_handler,
              count=count if count > 0 else 0,
              timeout=timeout, store=False, monitor=True)
    except KeyboardInterrupt:
        print("\nCapture stopped")
    except Exception as e:
        print(f"Capture error: {e}")
        sys.exit(1)

    print(f"\nCaptured {packet_count[0]} packets total")


def main():
    parser = argparse.ArgumentParser(
        description='ESP32-CAM FPV Packet Capture and Frame Extraction',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Live capture with display:
    sudo python esp32_fpv_capture.py -I wlan0mon --fec --display

    # Live capture, display, and record video:
    sudo python esp32_fpv_capture.py -I wlan0mon --fec --display --video output.avi

    # Display only, no frame saving:
    sudo python esp32_fpv_capture.py -I wlan0mon --fec --display --no-save

    # Extract frames from pcapng file:
    python esp32_fpv_capture.py -i capture.pcapng -o frames/ --fec

    # Playback pcapng with display:
    python esp32_fpv_capture.py -i capture.pcapng --fec --display

Dependencies:
    pip install dpkt         # pcapng file reading
    pip install zfec         # FEC decoding (--fec)
    pip install opencv-python # Display and video (--display, --video)
    pip install pypcap       # Live capture
        """
    )

    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument('-i', '--input', metavar='FILE',
                            help='Input pcapng/pcap file')
    input_group.add_argument('-I', '--interface', metavar='IFACE',
                            help='Network interface in monitor mode')

    parser.add_argument('-o', '--output', metavar='DIR', default='frames',
                        help='Output directory for frames (default: frames)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')
    parser.add_argument('-c', '--count', type=int, default=0,
                        help='Packet count limit (0 = unlimited)')
    parser.add_argument('-t', '--timeout', type=int, default=None,
                        help='Capture timeout in seconds')

    # FEC options
    parser.add_argument('--fec', action='store_true',
                        help='Enable FEC decoding (requires zfec)')
    parser.add_argument('-k', '--fec-k', type=int, default=DEFAULT_FEC_K,
                        help=f'FEC K value (default: {DEFAULT_FEC_K})')
    parser.add_argument('-n', '--fec-n', type=int, default=DEFAULT_FEC_N,
                        help=f'FEC N value (default: {DEFAULT_FEC_N})')

    # Display and video options
    parser.add_argument('--display', '-d', action='store_true',
                        help='Show live frame display (requires opencv)')
    parser.add_argument('--video', metavar='FILE',
                        help='Save video to file (e.g., output.avi)')
    parser.add_argument('--fps', type=float, default=30.0,
                        help='Video FPS for recording (default: 30)')
    parser.add_argument('--no-save', action='store_true',
                        help='Do not save individual frames to disk')

    args = parser.parse_args()

    # Check dependencies
    if args.fec and not HAS_ZFEC:
        print("Error: zfec required for --fec. Install: pip install zfec")
        sys.exit(1)

    if (args.display or args.video) and not HAS_CV2:
        print("Error: opencv required for --display/--video. Install: pip install opencv-python")
        sys.exit(1)

    # Setup FEC decoder
    fec_decoder = None
    if args.fec:
        fec_decoder = FECDecoder(k=args.fec_k, n=args.fec_n, verbose=args.verbose)

    # Setup display
    display = None
    if args.display or args.video:
        display = FrameDisplay(video_output=args.video, fps=args.fps,
                               enable_display=args.display)

    # Setup frame assembler
    save_frames = not args.no_save
    output_dir = args.output if save_frames else None

    def on_frame_complete(jpeg_data: bytes, frame_index: int):
        if display:
            display.display_frame(jpeg_data)

    assembler = FrameAssembler(
        output_dir=output_dir,
        fec_k=args.fec_k,
        verbose=args.verbose,
        save_frames=save_frames,
        on_frame_complete=on_frame_complete if display else None
    )

    try:
        if args.input:
            if not os.path.exists(args.input):
                print(f"Error: File not found: {args.input}")
                sys.exit(1)
            process_pcapng_file(args.input, assembler, fec_decoder, display)
        else:
            process_live_capture(args.interface, assembler, fec_decoder, display,
                               count=args.count, timeout=args.timeout)
    finally:
        # Cleanup
        if fec_decoder:
            fec_decoder.finalize()
        assembler.finalize()
        if display:
            display.close()

        # Print statistics
        print("\n=== Statistics ===")
        print(f"Packets processed: {assembler.stats['packets_processed']}")
        print(f"  Primary packets: {assembler.stats['primary_packets']}")
        print(f"  FEC packets:     {assembler.stats['fec_packets']}")
        if fec_decoder:
            print(f"FEC blocks complete:  {fec_decoder.stats['blocks_complete']}")
            print(f"FEC blocks recovered: {fec_decoder.stats['blocks_recovered']}")
            print(f"FEC blocks failed:    {fec_decoder.stats['blocks_failed']}")
        print(f"Frames decoded:    {assembler.stats['frames_saved']}")
        print(f"Frames incomplete: {assembler.stats['frames_incomplete']}")
        if save_frames and output_dir:
            print(f"Output directory:  {os.path.abspath(output_dir)}")


if __name__ == '__main__':
    main()
