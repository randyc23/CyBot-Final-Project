"""
world.py
Holds all state about the known world and the bot's current position.
Completely independent of the GUI — no imports from gui.py or comms.py.

Coordinate system:
  - Origin (0, 0) is the bot's starting position.
  - Angle 0   = facing east  (positive X direction)
  - Angle 90  = facing north (positive Y direction)
  - All distances in centimeters.
"""

import math
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional

# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------

class ObjectType(Enum):
    TALL    = auto()   # Detected by IR and ping (cylindrical obstacles)
    SHORT   = auto()   # Detected by bump sensor only
    HOLE    = auto()   # Detected by cliff sensor
    BORDER  = auto()   # Detected by bottom border sensor


class ObjectShape(Enum):
    CYLINDER  = auto()   # Single tall cylinder
    CLUSTER   = auto()   # Group of cylinders close together
    WALL      = auto()   # Line of cylinders forming a wall
    POINT     = auto()   # Short object or hole (no meaningful shape)


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class BotState:
    """Current known state of the bot in global coordinates."""
    x: float = 0.0       # cm
    y: float = 0.0       # cm
    angle: float = 90.0  # degrees (90 = facing north/up on screen)

    def update(self, x: float, y: float, angle: float):
        self.x = x
        self.y = y
        self.angle = angle


@dataclass
class ScanPoint:
    """
    A single raw scan reading converted to global coordinates.
    Stored so you can redraw the raw scan visualization at any time.
    """
    global_x: float
    global_y: float
    ir: float           # raw IR value
    ping: float         # raw ping value (cm)
    bot_x: float        # bot position when reading was taken
    bot_y: float
    bot_angle: float    # bot heading when reading was taken
    scan_angle: float   # angle offset of this reading relative to bot heading


@dataclass
class WorldObject:
    """A detected object in global space."""
    center_x: float
    center_y: float
    obj_type: ObjectType
    shape: ObjectShape
    radius: float = 0.0          # approximate radius or extent in cm
    label: Optional[str] = None

    def distance_to(self, x: float, y: float) -> float:
        """Distance from this object's center to a given point."""
        return math.hypot(self.center_x - x, self.center_y - y)


# ---------------------------------------------------------------------------
# Coordinate conversion
# ---------------------------------------------------------------------------

def polar_to_global(
    bot_x: float,
    bot_y: float,
    bot_angle: float,
    scan_angle: float,
    distance: float
) -> tuple[float, float]:
    """
    Convert a bot-relative polar scan reading to global Cartesian coordinates.

    Args:
        bot_x, bot_y:   Bot's current global position (cm)
        bot_angle:      Bot's current global heading (degrees, 0=east, 90=north)
        scan_angle:     Angle of this scan reading relative to bot heading (degrees)
                        0 = straight ahead, positive = left, negative = right
        distance:       Distance of the reading (cm)

    Returns:
        (global_x, global_y) tuple
    """
    total_angle = math.radians(bot_angle - scan_angle)
    global_x = bot_x + distance * math.cos(total_angle)
    global_y = bot_y + distance * math.sin(total_angle)
    return global_x, global_y


# ---------------------------------------------------------------------------
# Clustering
# ---------------------------------------------------------------------------

# Maximum distance (cm) between two scan points to be considered the same object.
# Tune this based on real field data.
CLUSTER_THRESHOLD_CM = 5.0

# Minimum number of scan points to form an object.
MIN_POINTS_FOR_OBJECT = 2


def cluster_scan_points(points: list[ScanPoint]) -> list[WorldObject]:
    """

    This is intentionally naive — replace with something smarter once you
    have real field data to work with.
    """
    if not points:
        return []

    objects = []
    current_cluster: list[ScanPoint] = [points[0]]

    for point in points[1:]:
        prev = current_cluster[-1]
        dist = math.hypot(point.global_x - prev.global_x,
                          point.global_y - prev.global_y)
        if dist <= CLUSTER_THRESHOLD_CM:
            current_cluster.append(point)
        else:
            obj = _cluster_to_object(current_cluster)
            if obj:
                objects.append(obj)
            current_cluster = [point]

    # Don't forget the last cluster
    obj = _cluster_to_object(current_cluster)
    if obj:
        objects.append(obj)

    return objects


def _cluster_to_object(cluster: list[ScanPoint]) -> Optional[WorldObject]:
    """Convert a cluster of scan points into a WorldObject."""
    if len(cluster) < MIN_POINTS_FOR_OBJECT:
        return None

    # Center is the average of all points
    cx = sum(p.global_x for p in cluster) / len(cluster)
    cy = sum(p.global_y for p in cluster) / len(cluster)

    # radius is the max distance from center to any point
    radius = max(math.hypot(p.global_x - cx, p.global_y - cy) for p in cluster)

    # Determine shape based on spread
    # TODO: refine these thresholds with real data
    if len(cluster) <= 3:
        shape = ObjectShape.CYLINDER
    elif radius > 40.0:
        shape = ObjectShape.WALL
    else:
        shape = ObjectShape.CLUSTER

    return WorldObject(
        center_x=cx,
        center_y=cy,
        obj_type=ObjectType.TALL,
        shape=shape,
        radius=radius
    )


# ---------------------------------------------------------------------------
# World Map
# ---------------------------------------------------------------------------

class WorldMap:
    """
    The main world state container.
    Holds the bot's current position, all raw scan points from the current
    sweep, and all detected objects accumulated over the session.
    """

    def __init__(self):
        self.bot = BotState()

        # Raw scan points from the current active sweep
        self.current_sweep: list[ScanPoint] = []

        # All scan points ever collected (for redrawing history)
        self.all_scan_points: list[ScanPoint] = []

        # All detected objects in the world
        self.objects: list[WorldObject] = []

        # Whether a sweep is currently in progress
        self.sweep_active: bool = False

        # Emergency stop flag — set when bump or cliff event received
        self.emergency_stop: bool = False
        self.last_interrupt: Optional[str] = None  # "bump" or "cliff"

    # --- Bot state ---

    def update_bot(self, x: float, y: float, angle: float):
        """Update bot position from a position packet."""
        self.bot.update(x, y, angle)

    # --- Sweep management ---

    def start_sweep(self):
        """Called when a new scan sweep begins."""
        self.current_sweep = []
        self.sweep_active = True

    def add_scan_reading(self, scan_angle: float, ir: float, ping: float):
        """
        Add a single reading from an in-progress sweep.
        Uses ping distance for position (more reliable for mapping).
        If ping reads max range or clearly out of bounds, skip it.
        """
        # TODO: define MAX_RANGE based on your sensor specs
        MAX_RANGE = 80.0  # cm — readings beyond this are treated as no-detection

        if ping >= MAX_RANGE or ir < 600:
            return

        gx, gy = polar_to_global(
            self.bot.x, self.bot.y, self.bot.angle,
            scan_angle, ping
        )

        point = ScanPoint(
            global_x=gx,
            global_y=gy,
            ir=ir,
            ping=ping,
            bot_x=self.bot.x,
            bot_y=self.bot.y,
            bot_angle=self.bot.angle,
            scan_angle=scan_angle
        )
        self.current_sweep.append(point)
        self.all_scan_points.append(point)

    def finish_sweep(self):
        """
        Called when the sweep is complete.
        Clusters the current sweep into objects and merges with the world map.
        """
        print("sweep complete")
        self.sweep_active = False
        new_objects = cluster_scan_points(self.current_sweep)
        # TODO: merge new_objects with existing self.objects
        # For now, just append them all (will create duplicates on rescan)
        self.objects.extend(new_objects)

    # --- Interrupt events ---

    def handle_bump(self, sensor_id: str):
        """Called when a bump packet is received."""
        self.emergency_stop = True
        self.last_interrupt = f"BUMP:{sensor_id}"
        print(f"[world] Bump event on sensor: {sensor_id}")

    def handle_cliff(self, sensor_id: str):
        """Called when a cliff packet is received."""
        self.emergency_stop = True
        self.last_interrupt = f"CLIFF:{sensor_id}"
        print(f"[world] Cliff event on sensor: {sensor_id}")

    def clear_interrupt(self):
        """Clear the emergency stop flag after the operator acknowledges it."""
        self.emergency_stop = False
        self.last_interrupt = None

    # --- Queries ---

    def objects_near(self, x: float, y: float, radius: float) -> list[WorldObject]:
        """Return all objects within radius cm of the given point."""
        return [obj for obj in self.objects
                if obj.distance_to(x, y) <= radius]
