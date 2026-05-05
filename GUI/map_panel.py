"""
map_panel.py
A QWidget canvas that draws the world map.

The bot is fixed at a point slightly below center.
Everything else is drawn relative to the bot's current global position.
Grid, scan points, and detected objects are all rendered here.
"""

import math
from PyQt5.QtWidgets import QWidget
from PyQt5.QtCore import Qt, QPointF
from PyQt5.QtGui import QPainter, QPen, QBrush, QColor, QFont

from world import WorldMap, ObjectType, ObjectShape


# Pixels per centimeter — controls the zoom level of the map view.
# At 4px/cm, 80cm fills 320px. Adjust to taste.
SCALE = 4.0

# Bot is drawn at this fraction down from the top of the canvas.
# 0.6 means 60% down, giving more space in front (above) the bot.
BOT_Y_FRACTION = 0.6

# Colors
COLOR_BG         = QColor("#1a1a2e")
COLOR_GRID       = QColor("#2a2a4a")
COLOR_BOT        = QColor("#00d4ff")
COLOR_SCAN_LINE  = QColor("#00ff88")
COLOR_SCAN_POINT = QColor("#00ff88")
COLOR_TALL_OBJ   = QColor("#ff6b35")
COLOR_SHORT_OBJ  = QColor("#ffd166")
COLOR_HOLE       = QColor("#ef476f")
COLOR_BORDER     = QColor("#a8dadc")
COLOR_HEADING    = QColor("#ffffff")


class MapPanel(QWidget):
    def __init__(self, world: WorldMap, parent=None):
        super().__init__(parent)
        self.world = world
        self.setMinimumSize(500, 500)
        self.setStyleSheet(f"background-color: {COLOR_BG.name()};")

    # -----------------------------------------------------------------------
    # Coordinate helpers
    # -----------------------------------------------------------------------

    def _bot_screen_pos(self) -> QPointF:
        """Screen position of the bot (fixed point on canvas)."""
        return QPointF(self.width() / 2, self.height() * BOT_Y_FRACTION)

    def _world_to_screen(self, wx: float, wy: float) -> QPointF:
        """
        Convert a global world coordinate to screen pixels.
        The bot's current world position maps to _bot_screen_pos().
        Y is flipped because screen Y increases downward but world Y increases upward.
        """
        bot = self.world.bot
        bsp = self._bot_screen_pos()
        sx = bsp.x() + (wx - bot.x) * SCALE
        sy = bsp.y() - (wy - bot.y) * SCALE  # flip Y
        return QPointF(sx, sy)

    # -----------------------------------------------------------------------
    # Paint
    # -----------------------------------------------------------------------

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        self._draw_grid(painter)
        self._draw_scan_points(painter)
        self._draw_objects(painter)
        self._draw_bot(painter)
        self._draw_range_ring(painter)

        painter.end()

    def _draw_grid(self, painter: QPainter):
        """Draw a background grid in world-space increments (every 20cm)."""
        pen = QPen(COLOR_GRID, 1, Qt.DotLine)
        painter.setPen(pen)

        grid_step_cm = 20
        grid_step_px = grid_step_cm * SCALE

        bsp = self._bot_screen_pos()
        bot = self.world.bot

        # Offset so grid lines align to world-space multiples
        offset_x = (bot.x % grid_step_cm) * SCALE
        offset_y = (bot.y % grid_step_cm) * SCALE

        x = (bsp.x() - offset_x) % grid_step_px
        while x < self.width():
            painter.drawLine(int(x), 0, int(x), self.height())
            x += grid_step_px

        y = (bsp.y() + offset_y) % grid_step_px
        while y < self.height():
            painter.drawLine(0, int(y), self.width(), int(y))
            y += grid_step_px

    def _draw_scan_points(self, painter: QPainter):
        """
        Draw the current sweep's scan points as dots connected by lines.
        Also draws historical scan points from all_scan_points, faded.
        """
        # Historical points — faded
        old_color = QColor(COLOR_SCAN_POINT)
        old_color.setAlpha(40)
        painter.setPen(QPen(old_color, 1))
        for pt in self.world.all_scan_points:
            sp = self._world_to_screen(pt.global_x, pt.global_y)
            painter.drawEllipse(sp, 2, 2)

        # Current sweep — bright, connected by lines
        sweep = self.world.current_sweep
        if len(sweep) > 1:
            pen = QPen(COLOR_SCAN_LINE, 1)
            painter.setPen(pen)
            for i in range(1, len(sweep)):
                a = self._world_to_screen(sweep[i-1].global_x, sweep[i-1].global_y)
                b = self._world_to_screen(sweep[i].global_x, sweep[i].global_y)
                painter.drawLine(a, b)

        painter.setPen(QPen(COLOR_SCAN_POINT, 2))
        painter.setBrush(QBrush(COLOR_SCAN_POINT))
        for pt in sweep:
            sp = self._world_to_screen(pt.global_x, pt.global_y)
            painter.drawEllipse(sp, 3, 3)

    def _draw_objects(self, painter: QPainter):
        """Draw all detected world objects."""
        for obj in self.world.objects:
            sp = self._world_to_screen(obj.center_x, obj.center_y)
            color = {
                ObjectType.TALL:   COLOR_TALL_OBJ,
                ObjectType.SHORT:  COLOR_SHORT_OBJ,
                ObjectType.HOLE:   COLOR_HOLE,
                ObjectType.BORDER: COLOR_BORDER,
            }.get(obj.obj_type, QColor("white"))

            painter.setPen(QPen(color, 2))
            painter.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 80)))

            radius_px = max(6, obj.size * SCALE)

            if obj.shape == ObjectShape.CYLINDER or obj.shape == ObjectShape.POINT:
                painter.drawEllipse(sp, radius_px, radius_px)
            elif obj.shape in (ObjectShape.CLUSTER, ObjectShape.WALL):
                # Draw a larger circle for clusters/walls for now
                # TODO: draw wall as a line between its endpoints
                painter.drawEllipse(sp, radius_px, radius_px)

            # Label
            if obj.label:
                painter.setPen(QPen(color))
                painter.setFont(QFont("Monospace", 8))
                painter.drawText(sp + QPointF(radius_px + 2, 4), obj.label)

    def _draw_bot(self, painter: QPainter):
        """Draw the bot as a circle with a heading indicator."""
        bsp = self._bot_screen_pos()
        bot_radius = 10

        # Body
        painter.setPen(QPen(COLOR_BOT, 2))
        painter.setBrush(QBrush(QColor(0, 212, 255, 60)))
        painter.drawEllipse(bsp, bot_radius, bot_radius)

        # Heading arrow
        heading_rad = math.radians(self.world.bot.angle)
        arrow_len = bot_radius + 8
        tip = QPointF(
            bsp.x() + arrow_len * math.cos(heading_rad),
            bsp.y() - arrow_len * math.sin(heading_rad)  # flip Y
        )
        painter.setPen(QPen(COLOR_HEADING, 2))
        painter.drawLine(bsp, tip)

    def _draw_range_ring(self, painter: QPainter):
        """Draw a faint circle showing the ~80cm sensor range."""
        bsp = self._bot_screen_pos()
        range_px = 80 * SCALE
        pen = QPen(QColor(255, 255, 255, 30), 1, Qt.DashLine)
        painter.setPen(pen)
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(bsp, range_px, range_px)
