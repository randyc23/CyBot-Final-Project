"""
scan_panel.py
Displays raw scan data from the current sweep.
Shows a simple polar plot of the most recent sweep and live IR/ping readouts.
"""

import math
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QLabel, QFrame
from PyQt5.QtCore import Qt, QPointF
from PyQt5.QtGui import QPainter, QPen, QBrush, QColor, QFont

from world import WorldMap


COLOR_BG     = QColor("#12121e")
COLOR_RING   = QColor("#2a2a4a")
COLOR_IR     = QColor("#ff6b35")
COLOR_PING   = QColor("#00d4ff")
COLOR_LABEL  = QColor("#aaaacc")

# Max display range for the polar plot in cm
POLAR_MAX_CM = 100.0


class ScanPanel(QWidget):
    def __init__(self, world: WorldMap, parent=None):
        super().__init__(parent)
        self.world = world
        self.setMinimumHeight(300)
        self.setStyleSheet(f"background-color: {COLOR_BG.name()};")

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        self._draw_polar_background(painter)
        self._draw_sweep(painter)
        self._draw_live_readout(painter)

        painter.end()

    def _center(self) -> QPointF:
        # Center the polar plot in the lower portion, leaving room for readout at top
        return QPointF(self.width() / 2, self.height() * 0.6)

    def _polar_radius(self) -> float:
        return min(self.width(), self.height()) * 0.4

    def _cm_to_px(self, cm: float) -> float:
        return (cm / POLAR_MAX_CM) * self._polar_radius()

    def _polar_to_screen(self, angle_deg: float, distance_cm: float) -> QPointF:
        """
        Convert a bot-relative polar coordinate to screen position on the polar plot.
        0° is forward (up on screen), positive angles sweep left.
        """
        c = self._center()
        r = self._cm_to_px(distance_cm)
        # Rotate so 0° points up: subtract from 90°
        screen_angle = math.radians(90.0 - angle_deg)
        return QPointF(
            c.x() + r * math.cos(screen_angle),
            c.y() - r * math.sin(screen_angle)
        )

    def _draw_polar_background(self, painter: QPainter):
        """Draw concentric range rings and angle lines."""
        c = self._center()
        max_r = self._polar_radius()

        painter.setPen(QPen(COLOR_RING, 1, Qt.DotLine))
        painter.setBrush(Qt.NoBrush)

        # Range rings every 20cm
        for r_cm in range(20, int(POLAR_MAX_CM) + 1, 20):
            r_px = self._cm_to_px(r_cm)
            painter.drawEllipse(c, r_px, r_px)
            # Label
            painter.setPen(QPen(COLOR_LABEL, 1))
            painter.setFont(QFont("Monospace", 7))
            painter.drawText(
                QPointF(c.x() + r_px + 2, c.y()),
                f"{r_cm}cm"
            )
            painter.setPen(QPen(COLOR_RING, 1, Qt.DotLine))

        # Angle lines every 30°
        for a in range(-90, 91, 30):
            tip = self._polar_to_screen(a, POLAR_MAX_CM)
            painter.drawLine(c, tip)

    def _draw_sweep(self, painter: QPainter):
        """Draw the current sweep's readings as dots connected by lines on the polar plot."""
        sweep = self.world.current_sweep
        if not sweep:
            return

        # Lines connecting readings
        if len(sweep) > 1:
            painter.setPen(QPen(COLOR_PING, 1))
            for i in range(1, len(sweep)):
                a = self._polar_to_screen(sweep[i-1].scan_angle, sweep[i-1].ping)
                b = self._polar_to_screen(sweep[i].scan_angle,   sweep[i].ping)
                painter.drawLine(a, b)

        # Dots for each reading
        for pt in sweep:
            ping_sp = self._polar_to_screen(pt.scan_angle, pt.ping)
            painter.setPen(QPen(COLOR_PING, 2))
            painter.setBrush(QBrush(COLOR_PING))
            painter.drawEllipse(ping_sp, 3, 3)

            # IR reading as a smaller dot — only if meaningfully different
            # TODO: normalize IR to cm once you know the IR calibration curve
            # For now just draw at the same angle, slightly different radius placeholder

    def _draw_live_readout(self, painter: QPainter):
        """Show the latest IR and ping values as text at the top of the panel."""
        if not self.world.current_sweep:
            return

        latest = self.world.current_sweep[-1]
        painter.setFont(QFont("Monospace", 10))

        painter.setPen(QPen(COLOR_IR))
        painter.drawText(10, 20, f"IR:   {latest.ir:.1f}")

        painter.setPen(QPen(COLOR_PING))
        painter.drawText(10, 38, f"PING: {latest.ping:.1f} cm")

        painter.setPen(QPen(COLOR_LABEL))
        painter.drawText(10, 56, f"Angle: {latest.scan_angle:.1f}°")

        if self.world.sweep_active:
            painter.setPen(QPen(QColor("#00ff88")))
            painter.drawText(10, 74, "● SWEEPING")
        else:
            pts = len(self.world.current_sweep)
            painter.setPen(QPen(COLOR_LABEL))
            painter.drawText(10, 74, f"Last sweep: {pts} pts")
