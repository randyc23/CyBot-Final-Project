"""
gui.py
Main application window.
Composed of three panels:
  - MapPanel:     Renders the world map with the bot at center
  - ScanPanel:    Shows raw scan data (live values + sweep history)
  - CommandPanel: Connection controls, status display, manual command buttons

The GUI polls world state and the comms queue on a timer each frame.
It does not do any logic itself — it only reads from WorldMap and
calls methods on BotConnection.
"""

import sys
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QHBoxLayout, QVBoxLayout, QLabel,
    QPushButton, QLineEdit, QGroupBox,
    QTextEdit, QSplitter
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QKeyEvent

from comms import (
    BotConnection,
    PACKET_SCAN, PACKET_POSITION, PACKET_BUMP, PACKET_BOUNDARY, PACKET_HOLE, PACKET_SWEEP_START,
    PACKET_SWEEP_END, SweepStartPacket, SweepEndPacket, ScanPacket, PositionPacket, BumpPacket,
    BoundaryPacket, HolePacket
)
from world import WorldMap
from map_panel import MapPanel
from scan_panel import ScanPanel


# How often the GUI polls the comms queue and redraws (milliseconds)
POLL_INTERVAL_MS = 50


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Bot Control Station")
        self.resize(1200, 800)

        self.world = WorldMap()
        self.conn = BotConnection()

        self._build_ui()
        self._start_poll_timer()

    # -----------------------------------------------------------------------
    # UI construction
    # -----------------------------------------------------------------------

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)

        root_layout = QHBoxLayout(central)

        # Left: map view
        self.map_panel = MapPanel(self.world)
        root_layout.addWidget(self.map_panel, stretch=3)

        # Right: scan panel + command panel stacked vertically
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)

        self.scan_panel = ScanPanel(self.world)
        right_layout.addWidget(self.scan_panel, stretch=2)

        right_layout.addWidget(self._build_command_panel(), stretch=1)

        root_layout.addWidget(right_panel, stretch=1)

    def _build_command_panel(self) -> QGroupBox:
        group = QGroupBox("Command Panel")
        layout = QVBoxLayout(group)

        # Connection controls
        conn_row = QHBoxLayout()
        self.host_input = QLineEdit("192.168.1.1")
        self.port_input = QLineEdit("288")
        self.port_input.setFixedWidth(60)
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self._on_connect)
        conn_row.addWidget(QLabel("Host:"))
        conn_row.addWidget(self.host_input)
        conn_row.addWidget(QLabel("Port:"))
        conn_row.addWidget(self.port_input)
        conn_row.addWidget(self.connect_btn)
        layout.addLayout(conn_row)

        # Status label
        self.status_label = QLabel("Not connected")
        layout.addWidget(self.status_label)

        # Bot position display
        self.position_label = QLabel("Bot: x=0.0  y=0.0  angle=90.0°")
        layout.addWidget(self.position_label)

        # Interrupt alert label (shows bump/cliff events)
        self.interrupt_label = QLabel("")
        self.interrupt_label.setStyleSheet("color: red; font-weight: bold;")
        layout.addWidget(self.interrupt_label)

        # Manual movement buttons (also controlled by keyboard)
        move_grid = QHBoxLayout()
        for label, cmd in [("← (A)", "a"), ("↑ (W)", "w"),
                            ("↓ (S)", "s"), ("→ (D)", "d")]:
            btn = QPushButton(label)
            btn.clicked.connect(lambda _, c=cmd: self._send(c))
            move_grid.addWidget(btn)
        layout.addLayout(move_grid)

        # Other command buttons
        action_row = QHBoxLayout()
        scan_btn = QPushButton("Scan (e)")
        scan_btn.clicked.connect(lambda: self._send("e"))
        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._clear_objects)
        ack_btn = QPushButton("Ack Interrupt")
        ack_btn.clicked.connect(self._ack_interrupt)
        action_row.addWidget(scan_btn)
        action_row.addWidget(clear_btn)
        action_row.addWidget(ack_btn)
        layout.addLayout(action_row)

        # Log / raw output
        self.log = QTextEdit()
        self.log.setReadOnly(True)
        self.log.setFixedHeight(100)
        layout.addWidget(self.log)

        return group

    # -----------------------------------------------------------------------
    # Poll timer
    # -----------------------------------------------------------------------

    def _start_poll_timer(self):
        self.timer = QTimer()
        self.timer.timeout.connect(self._poll)
        self.timer.start(POLL_INTERVAL_MS)

    def _poll(self):
        """
        Called every POLL_INTERVAL_MS ms.
        Drains the incoming packet queue and updates world state.
        Then triggers a GUI redraw.
        """
        while not self.conn.incoming_queue.empty():
            packet = self.conn.incoming_queue.get_nowait()
            self._handle_packet(packet)

        self._refresh_ui()

    def _handle_packet(self, packet):
        """Route an incoming packet to the appropriate world update."""
        if packet.type == PACKET_POSITION:
            self.world.update_bot(packet.x, packet.y, packet.angle)

        elif packet.type == PACKET_SWEEP_START:
            self.world.start_sweep()

        elif packet.type == PACKET_SWEEP_END:
            self.world.finish_sweep()

        elif packet.type == PACKET_SCAN:
            self.world.add_scan_reading(packet.angle, packet.ir, packet.ping)

        elif packet.type == PACKET_BUMP:
            self.world.handle_bump(packet.sensor_id)
            self._log(f"BUMP event: {packet.sensor_id}")

        elif packet.type == PACKET_BOUNDARY:
            self.world.handle_cliff2(packet.sensor_id, "BORDER")
            self._log(f"BOUNDARY event: {packet.sensor_id}")

        elif packet.type == PACKET_HOLE:
            self.world.handle_cliff2(packet.sensor_id, "HOLE")
            self._log(f"HOLE event: {packet.sensor_id}")

    def _refresh_ui(self):
        """Update all display elements from current world state."""
        bot = self.world.bot
        self.position_label.setText(
            f"Bot: x={bot.x:.1f}  y={bot.y:.1f}  angle={bot.angle:.1f}°"
        )

        if self.world.emergency_stop:
            self.interrupt_label.setText(
                f"⚠ INTERRUPT: {self.world.last_interrupt}"
            )
        else:
            self.interrupt_label.setText("")

        self.map_panel.update()   # triggers repaint
        self.scan_panel.update()

    # -----------------------------------------------------------------------
    # Connection
    # -----------------------------------------------------------------------

    def _on_connect(self):
        if self.conn.connected:
            self.conn.disconnect()
            self.connect_btn.setText("Connect")
            self.status_label.setText("Disconnected")
        else:
            host = self.host_input.text().strip()
            port = int(self.port_input.text().strip())
            success = self.conn.connect(host, port)
            if success:
                self.connect_btn.setText("Disconnect")
                self.status_label.setText(f"Connected to {host}:{port}")
            else:
                self.status_label.setText("Connection failed")

    # -----------------------------------------------------------------------
    # Commands
    # -----------------------------------------------------------------------

    def _send(self, command: str):
        self.conn.send(command)
        self._log(f"→ {repr(command)}")

    def _ack_interrupt(self):
        self.world.clear_interrupt()

    def _log(self, msg: str):
        self.log.append(msg)

    def _clear_objects(self):
        self.world.objects.clear() 

    # -----------------------------------------------------------------------
    # Keyboard input
    # -----------------------------------------------------------------------

    def keyPressEvent(self, event: QKeyEvent):
        """
        Capture keyboard input and send commands to the bot.
        The window must have focus for this to fire.
        """
        key_map = {
            Qt.Key_W:      "w",
            Qt.Key_A:      "a",
            Qt.Key_S:      "s",
            Qt.Key_D:      "d",
            Qt.Key_Up:     "w",
            Qt.Key_Left:   "a",
            Qt.Key_Down:   "s",
            Qt.Key_Right:  "d",
            Qt.Key_E:  "e",   # trigger scan
            Qt.Key_X:      "x",   # stop
        }
        cmd = key_map.get(event.key())
        if cmd:
            self._send(cmd)
        else:
            super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QKeyEvent):
        """Send stop command when a movement key is released."""
        movement_keys = {
            Qt.Key_W, Qt.Key_A, Qt.Key_S, Qt.Key_D,
            Qt.Key_Up, Qt.Key_Left, Qt.Key_Down, Qt.Key_Right,
        }
        if event.key() in movement_keys and not event.isAutoRepeat():
            self._send("x")
        else:
            super().keyReleaseEvent(event)

    def closeEvent(self, event):
        self.conn.disconnect()
        event.accept()


# -----------------------------------------------------------------------
# Entry point (also see main.py)
# -----------------------------------------------------------------------

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
