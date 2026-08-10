"""
LoRa Chat — UART LoRa 模块接口 (self_id = 0x05)
通过 USB-UART 直连 LoRa 模块，纯数据收发，不做任何配置或低功耗模式切换。
模块需提前手动配置好固定点透传模式，持续处于工作状态。

用法:
  python sim_lora.py <COM端口> [选项]

示例:
  python sim_lora.py COM5
  python sim_lora.py COM5 --self-id 0x03

依赖: pip install pyserial
"""

import sys
import time
import threading
import random
import argparse
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("请先安装 pyserial: pip install pyserial")
    sys.exit(1)

# ── 常量 ──────────────────────────────────────────────────────────────────
LORA_GROUP = 0x00
LORA_CHANNEL = 0x3C
ACK_TIMEOUT = 3.0        # ACK 等待超时 (秒), 匹配固件
HB_INTERVAL_MIN = 180    # 心跳最小间隔 (秒)
HB_INTERVAL_MAX = 240    # 心跳最大间隔 (秒)

# ── CRC8 表 (多项式 x^8 + x^5 + x^4 + 1, 初始值 0) ──────────────────
CRC8_TABLE = [
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3,
]

# ── 解析状态码 ────────────────────────────────────────────────────────────
PARSE_OK, PARSE_ERR_PTR, PARSE_ERR_SHORT, PARSE_ERR_LEN, PARSE_ERR_CRC = range(5)
PARSE_NAMES = {0: "OK", 1: "ERR_PTR", 2: "ERR_SHORT", 3: "ERR_LEN", 4: "ERR_CRC"}

SEND_IDLE, SEND_WAITING_ACK = 0, 1

ansi = lambda code, s: f"\033[{code}m{s}\033[0m" if sys.stdout.isatty() else s
BOLD = lambda s: ansi("1", s)
GREEN = lambda s: ansi("92", s)
YELLOW = lambda s: ansi("93", s)
RED = lambda s: ansi("91", s)
CYAN = lambda s: ansi("94", s)
GREY = lambda s: ansi("90", s)

# =====================================================================
#  CRC8 / 帧构建 / 帧解析  (与固件完全一致)
# =====================================================================

def crc8(data):
    crc = 0
    for b in data:
        crc = CRC8_TABLE[crc ^ b]
    return crc

def build_app_frame(self_id, target_id, data_str, seq):
    buf = bytearray()
    buf.extend([0xAA, 0x55, seq & 0xFF, self_id, target_id])
    now = datetime.now()
    buf.extend([now.month, now.day, now.hour, now.minute])
    encoded = data_str.encode("utf-8")[:120]
    buf.append(len(encoded))
    buf.extend(encoded)
    buf.append(crc8(bytes(buf[2:])))
    return bytes(buf)

def build_ack_frame(original_seq, own_id, target_id):
    buf = bytearray()
    buf.extend([0xAA, 0x55, original_seq & 0xFF, own_id, target_id])
    now = datetime.now()
    buf.extend([now.month, now.day, now.hour, now.minute])
    buf.append(0)
    buf.append(crc8(bytes(buf[2:])))
    return bytes(buf)

def is_broadcast(target_id):
    # 0x00 = ALL 广播消息, 0xFF = 广播帧(心跳等) —— 两者路由头都是 FF FF
    return target_id in (0x00, 0xFF)

def build_routing(target_id):
    if is_broadcast(target_id):
        return bytes([0xFF, 0xFF, LORA_CHANNEL])
    return bytes([LORA_GROUP, target_id, LORA_CHANNEL])

def parse_frame(buf):
    if buf is None:
        return PARSE_ERR_PTR, None
    if len(buf) < 11 or buf[0] != 0xAA or buf[1] != 0x55:
        return PARSE_ERR_SHORT, None
    dlen = buf[9]
    if dlen > 120 or len(buf) != 10 + dlen + 1:
        return PARSE_ERR_LEN, None
    if crc8(bytes(buf[2:10 + dlen])) != buf[10 + dlen]:
        return PARSE_ERR_CRC, None
    return PARSE_OK, {
        "seq": buf[2], "self_id": buf[3], "target_id": buf[4],
        "month": buf[5], "day": buf[6], "hour": buf[7], "minute": buf[8],
        "data_len": dlen,
        "data_str": buf[10:10 + dlen].decode("utf-8", errors="replace"),
        "is_ack": dlen == 0,
    }

def hexdump(data, label=""):
    s = " ".join(f"{b:02X}" for b in data)
    return f"{label}[{s}]" if label else f"[{s}]"

# =====================================================================
#  串口接口 (纯收发, 不做模式切换)
# =====================================================================

class LoraSerial:
    def __init__(self, port, baud=9600):
        self.port = port
        self.baud = baud
        self.ser = None
        self._lock = threading.Lock()

    def open(self):
        self.ser = serial.Serial(
            port=self.port, baudrate=self.baud, bytesize=8,
            parity='N', stopbits=1, timeout=0.1
        )
        print(f"[SERIAL] 打开 {self.port} @ {self.baud} 8N1")

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[SERIAL] 端口已关闭")

    def write(self, data):
        with self._lock:
            self.ser.write(data)

    def read(self, size=1):
        with self._lock:
            return self.ser.read(size)


class LoraModule:
    def __init__(self, lora_serial, self_id):
        self.ls = lora_serial
        self.self_id = self_id
        self.seq_num = 0
        self.send_state = SEND_IDLE
        self.waiting_ack_seq = None
        self.waiting_ack_target = None
        self.ack_event = threading.Event()
        self.running = False
        self._rx_thread = None
        self._hb_thread = None

    # ── 接收状态机 (匹配 uart_parse_byte) ──
    def _rx_loop(self):
        rx_state = 0  # 0=WAIT_AA, 1=WAIT_55, 2=HEADER, 3=DATA
        rx_buf = bytearray()
        target_len = 0

        while self.running:
            try:
                byte = self.ls.read(1)
            except Exception as e:
                print(f"  {RED('[SERIAL ERR]')} {e}")
                time.sleep(0.5)
                continue

            if not byte:
                continue

            b = byte[0]

            if rx_state == 0:  # WAIT_AA
                if b == 0xAA:
                    rx_buf = bytearray([b])
                    rx_state = 1
            elif rx_state == 1:  # WAIT_55
                if b == 0x55:
                    rx_buf.append(b)
                    rx_state = 2
                else:
                    rx_state = 0
            elif rx_state == 2:  # HEADER
                rx_buf.append(b)
                if len(rx_buf) == 10:
                    dlen = rx_buf[9]
                    if dlen > 120:
                        rx_state = 0
                        continue
                    target_len = 2 + 8 + dlen + 1
                    rx_state = 3
            elif rx_state == 3:  # DATA
                rx_buf.append(b)
                if len(rx_buf) == target_len:
                    self._on_frame(bytes(rx_buf))
                    rx_state = 0

    def _on_frame(self, app_frame):
        status, parsed = parse_frame(app_frame)
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        print(f"\n{ts}  {YELLOW('<<< RAW')} ({len(app_frame)}B) {hexdump(app_frame)}")

        if status != PARSE_OK:
            print(f"     {RED(f'PARSE FAIL: {PARSE_NAMES[status]}')}")
            return

        p = parsed
        kind = YELLOW("ACK") if p["is_ack"] else CYAN("DATA")
        sid = p["self_id"]
        tid = p["target_id"]
        print(f"     [{kind}]  seq=0x{p['seq']:02X}  from=0x{sid:02X}"
              f"  to=0x{tid:02X}  {p['month']:02d}/{p['day']:02d}"
              f" {p['hour']:02d}:{p['minute']:02d}  len={p['data_len']}")

        if p["is_ack"]:
            print(f"     acking seq=0x{p['seq']:02X}  from=0x{sid:02X}")
            if (self.send_state == SEND_WAITING_ACK
                    and tid == self.self_id
                    and self.waiting_ack_seq == p["seq"]):
                print(f"     {GREEN('>>> ACK MATCH!  SEND COMPLETE <<<')}")
                self.ack_event.set()
            else:
                print(f"     {GREY('(unexpected ACK, ignoring)')}")
        else:
            # 数据帧分类：定点(回ACK) / ALL广播0x00(不回ACK) / 其他广播0xFF(如心跳, 不回ACK)
            if p["target_id"] == self.self_id:
                print(f"     msg=\"{p['data_str']}\"")
                self._send_ack(p["seq"], sid)
                print(f"     {GREEN(f'[ACK SENT]  to 0x{sid:02X}')}")
            elif p["target_id"] == 0x00:
                print(f"     {CYAN('[ALL]')}  msg=\"{p['data_str']}\"")
                print(f"     {GREY('[ALL broadcast, no ACK]')}")
            elif p["target_id"] == 0xFF:
                kind = "HB" if p["data_str"] == "HB" else "BCAST"
                print(f"     {YELLOW(f'[{kind}]')}  msg=\"{p['data_str']}\"")
            else:
                print(f"     {GREY('[ignored] not for me')}")

    # ── 发送 ACK ──
    def _send_ack(self, original_seq, target_id):
        frame = build_ack_frame(original_seq, self.self_id, target_id)
        routing = build_routing(target_id)
        pkt = routing + frame
        print(f"     {GREY(f'[TX ACK] {hexdump(routing)} + {hexdump(frame)}')}")
        self.ls.write(pkt)

    # ── 通过 UART 发送 ──
    def _uart_tx(self, pkt):
        self.ls.write(pkt)

    # ── 发送消息 (匹配固件 send_lora_packet + send_state) ──
    def send_message(self, target_id, text):
        seq = self.seq_num
        self.seq_num = (self.seq_num + 1) & 0xFF

        frame = build_app_frame(self.self_id, target_id, text, seq)
        routing = build_routing(target_id)
        pkt = routing + frame

        if target_id == 0x00:
            target_str = "ALL"
        elif target_id == 0xFF:
            target_str = "BROADCAST"
        else:
            target_str = f"0x{target_id:02X}"
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"\n{ts}  {BOLD(f'>>> SEND to {target_str}')} seq=0x{seq:02X}"
              f"  \"{text}\"")
        print(f"     App: {hexdump(frame)}")
        print(f"     Pkt: {hexdump(pkt)}")

        self.send_state = SEND_WAITING_ACK
        self.waiting_ack_seq = seq
        self.waiting_ack_target = target_id
        self.ack_event.clear()

        self._uart_tx(pkt)

        if not is_broadcast(target_id):
            got = self.ack_event.wait(timeout=ACK_TIMEOUT)
            if got:
                print(f"     {GREEN(f'[SEND OK]  seq=0x{seq:02X}  ACK received')}")
            else:
                print(f"     {RED(f'[SEND TIMEOUT]  seq=0x{seq:02X}  no ACK')}")
        else:
            print(f"     [BROADCAST]  no ACK wait")

        self.send_state = SEND_IDLE
        return got if not is_broadcast(target_id) else True

    # ── 心跳任务 ──
    def _heartbeat_loop(self):
        while self.running:
            delay = random.randint(HB_INTERVAL_MIN, HB_INTERVAL_MAX)
            time.sleep(delay)
            if self.running:
                self.send_message(0xFF, "HB")

    # ── 启动/停止 ──
    def start(self):
        self.running = True
        self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._rx_thread.start()
        self._hb_thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
        self._hb_thread.start()
        print(f"[MODULE] 启动 (self_id=0x{self.self_id:02X})")

    def stop(self):
        self.running = False


# =====================================================================
#  主程序
# =====================================================================

def list_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("未检测到串口.")
    for p in sorted(ports):
        print(f"  {p.device}  —  {p.description}")


def main():
    parser = argparse.ArgumentParser(
        description="LoRa Chat UART 接口 (self_id=0x05)")
    parser.add_argument("port", nargs="?", help="COM 端口, 如 COM5")
    parser.add_argument("--self-id", type=lambda x: int(x, 16),
                        default=0x05, help="本机 ID (hex), 默认 0x05")
    parser.add_argument("--baud", type=int, default=9600,
                        help="波特率, 默认 9600")
    parser.add_argument("--list-ports", action="store_true",
                        help="列出可用串口")
    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return

    if not args.port:
        print("请指定 COM 端口. 用 --list-ports 查看可用端口.")
        parser.print_help()
        return

    ls = LoraSerial(args.port, args.baud)
    try:
        ls.open()
    except serial.SerialException as e:
        print(f"{RED('[ERROR]')} 无法打开 {args.port}: {e}")
        sys.exit(1)

    module = LoraModule(ls, args.self_id)
    module.start()

    current_target = 0x01

    def print_help():
        print(f"\n  命令:")
        print(f"    <text>       — 发送消息给当前目标")
        print(f"    /to <hex>    — 切换目标 (/to 01 定点, /to 00 = ALL广播, /to ff = 广播)")
        print(f"    /hb          — 发送心跳 (广播)")
        print(f"    /stats       — 显示统计")
        print(f"    /q           — 退出")
        if current_target == 0x00:
            t_label = "ALL (广播)"
        elif current_target == 0xFF:
            t_label = "BROADCAST"
        else:
            t_label = f"0x{current_target:02X}"
        print(f"  当前目标: {BOLD(t_label)}")

    print()
    print_help()
    print()

    stats = {"tx": 0, "rx": 0, "acks": 0}

    try:
        while True:
            try:
                raw = input(f"{BOLD(f'[0x{args.self_id:02X}]')}> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not raw:
                continue

            if raw == "/q":
                break
            elif raw.startswith("/to "):
                try:
                    t = int(raw[4:], 16)
                    current_target = t
                    if t == 0x00:
                        t_str = "ALL (广播)"
                    elif t == 0xFF:
                        t_str = "BROADCAST"
                    else:
                        t_str = f"0x{t:02X}"
                    print(f"  目标 -> {BOLD(t_str)}")
                except ValueError:
                    print("  /to <hex>  e.g. /to 01, /to 00=ALL, /to ff=BROADCAST")
            elif raw == "/hb":
                module.send_message(0xFF, "HB")
                stats["tx"] += 1
            elif raw == "/stats":
                print(f"  TX={stats['tx']}  RX={stats['rx']}  ACKs={stats['acks']}")
            elif raw == "/help":
                print_help()
            else:
                module.send_message(current_target, raw)
                stats["tx"] += 1

    finally:
        module.stop()
        ls.close()
        print("已退出.")


if __name__ == "__main__":
    main()
