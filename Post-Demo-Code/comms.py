"""
comms.py
Handles the WiFi socket connection to the bot.
Runs a background thread that reads incoming data and puts parsed packets
into a queue. The main app polls that queue each frame.
"""

import socket
import threading
import queue

PACKET_SWEEP_START = "SS"
PACKET_SWEEP_END   = "SE"
PACKET_SCAN     = "S"
PACKET_POSITION = "P"
PACKET_BUMP     = "B"
PACKET_BOUNDARY = "E"
PACKET_HOLE     = "H"

class Packet:
    """Base class for all parsed packets."""
    def __init__(self, packet_type: str):
        self.type = packet_type

class SweepStartPacket(Packet):
    def __init__(self):
        super().__init__(PACKET_SWEEP_START)

class SweepEndPacket(Packet):
    def __init__(self):
        super().__init__(PACKET_SWEEP_END)

class ScanPacket(Packet):
    def __init__(self, angle: float, ir: float, ping: float):
        super().__init__(PACKET_SCAN)
        self.angle = angle   # degrees, relative to bot heading
        self.ir    = ir      # IR distance reading (units TBD)
        self.ping  = ping    # Ultrasonic ping distance reading (units TBD)


class PositionPacket(Packet):
    def __init__(self, x: float, y: float, angle: float):
        super().__init__(PACKET_POSITION)
        self.x     = x      # global x position (cm)
        self.y     = y      # global y position (cm)
        self.angle = angle  # global heading in degrees (0 = east, 90 = north)


class BumpPacket(Packet):
    def __init__(self, sensor_id: str):
        super().__init__(PACKET_BUMP)
        self.sensor_id = sensor_id  # which bump sensor triggered


class BoundaryPacket(Packet):
    def __init__(self, sensor_id: str):
        super().__init__(PACKET_BOUNDARY)
        self.sensor_id = sensor_id  # which cliff sensor triggered

class HolePacket(Packet):
    def __init__(self, sensor_id: str):
        super().__init__(PACKET_HOLE)
        self.sensor_id = sensor_id  # which cliff sensor triggered

def parse_line(line: str) -> Packet | None:
    """
    Parse a raw line from the bot into a Packet object.
    Returns None if the line is unrecognized or malformed.
    """
    line = line.strip()
    if not line:
        return None

    parts = line.split("\t")
    prefix = parts[0]

    print(parts)

    try:
        if prefix == "SS":
            return SweepStartPacket()
        elif line == "SE":
            return SweepEndPacket()

        if prefix == PACKET_SCAN and len(parts) == 4:
            print(f"angle {parts[1]} ir {parts[2]} ping {parts[3]}")
            return ScanPacket(
                angle=float(parts[1]),
                ir=float(parts[2]),
                ping=float(parts[3])
            )

        
        elif prefix == PACKET_POSITION and len(parts) == 4:
            return PositionPacket(
                x=float(parts[1]),
                y=float(parts[2]),
                angle=float(parts[3])
            )

        elif prefix == PACKET_BUMP and len(parts) == 2:
            return BumpPacket(sensor_id=parts[1])

        elif prefix == PACKET_BOUNDARY and len(parts) == 2:
            print(parts)
            return BoundaryPacket(sensor_id=parts[1])

        elif prefix == PACKET_HOLE and len(parts) == 2:
            print(parts)
            return HolePacket(sensor_id=parts[1])

    except ValueError:
        print(f"[comms] Malformed packet: {repr(line)}")

    return None


class BotConnection:
    """
    Manages the TCP socket connection to the bot.
    Spawns a background reader thread that parses incoming lines
    and pushes Packet objects onto self.incoming_queue.
    """

    def __init__(self):
        self.sock: socket.socket | None = None
        self.connected = False
        self.incoming_queue: queue.Queue[Packet] = queue.Queue()
        self._reader_thread: threading.Thread | None = None
        self._stop_event = threading.Event()

    def connect(self, host: str, port: int) -> bool:
        """
        Open a TCP connection to the bot.
        Returns True on success, False on failure.
        """
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((host, port))
            self.connected = True
            self._stop_event.clear()
            self._reader_thread = threading.Thread(
                target=self._read_loop,
                daemon=True
            )
            self._reader_thread.start()
            print(f"[comms] Connected to {host}:{port}")
            return True
        except OSError as e:
            print(f"[comms] Connection failed: {e}")
            self.connected = False
            return False

    def disconnect(self):
        """Close the connection and stop the reader thread."""
        self._stop_event.set()
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.connected = False
        print("[comms] Disconnected.")

    def send(self, command: str):
        """
        Send a command string to the bot.
        Commands are single characters or short strings (e.g. 'w', 'a', 's', 'd').
        """
        if not self.connected or self.sock is None:
            print("[comms] Cannot send — not connected.")
            return
        try:
            self.sock.sendall(command.encode("utf-8"))
        except OSError as e:
            print(f"[comms] Send error: {e}")
            self.connected = False

    def _read_loop(self):
        """
        Background thread: reads lines from the socket, parses them,
        and puts resulting Packet objects into the queue.
        """
        buffer = ""
        while not self._stop_event.is_set():
            try:
                self.sock.settimeout(0.1)
                chunk = self.sock.recv(1024).decode("utf-8", errors="replace")
                if not chunk:
                    # Connection closed by bot
                    print("[comms] Connection closed by remote.")
                    self.connected = False
                    break
                buffer += chunk
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    packet = parse_line(line)
                    if packet is not None:
                        self.incoming_queue.put(packet)
            except socket.timeout:
                continue
            except OSError:
                break
