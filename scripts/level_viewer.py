#!/usr/bin/env python3
"""AirStrike 3D HMAP Level Viewer — Panda3D

Requires: Python >= 3.12, panda3d
Usage:    python viewer.py level.hsc [-w]
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Final

from direct.gui.OnscreenText import OnscreenText
from direct.showbase.ShowBase import ShowBase
from direct.task import Task
from panda3d.core import (
    AntialiasAttrib,
    CullFaceAttrib,
    Fog,
    Geom,
    GeomNode,
    GeomTriangles,
    GeomVertexData,
    GeomVertexFormat,
    GeomVertexWriter,
    KeyboardButton,
    LineSegs,
    LVector3,
    NodePath,
    TextNode,
    WindowProperties,
)

# ─── Constants ────────────────────────────────────────────────────────────────

HEADER_MAGIC: Final[bytes] = b"HMAP"
HEADER_SIZE: Final[int] = 28
OBJECT_MARKER: Final[bytes] = b"\xff\xff"
OBJECT_RECORD_SIZE: Final[int] = 8  # 2B marker + 2B type + 2B x + 2B y

COLOR_MODES: Final[list[str]] = ["terrain", "gradient", "contour", "satellite"]

GRID_LINE_SPACING: Final[int] = 8

# Color classification keywords → RGBA
_ENEMY_KEYS: Final[tuple[str, ...]] = ("helic", "tank", "turret", "btr")
_ITEM_KEYS: Final[tuple[str, ...]] = ("item",)
_NATURE_KEYS: Final[tuple[str, ...]] = ("tree", "grass", "kust", "kamni")
_BUILDING_KEYS: Final[tuple[str, ...]] = ("kolhoz", "dom", "angar", "zabor")
_VEHICLE_KEYS: Final[tuple[str, ...]] = ("jeep", "uaz", "gruzovik")

_OBJ_COLOR_MAP: Final[
    list[tuple[tuple[str, ...], tuple[float, float, float, float]]]
] = [
    (_ENEMY_KEYS, (1.0, 0.2, 0.2, 1.0)),
    (_ITEM_KEYS, (1.0, 1.0, 0.2, 1.0)),
    (_NATURE_KEYS, (0.2, 0.8, 0.2, 1.0)),
    (_BUILDING_KEYS, (0.8, 0.5, 0.2, 1.0)),
    (_VEHICLE_KEYS, (0.9, 0.6, 0.1, 1.0)),
]
_OBJ_COLOR_DEFAULT: Final[tuple[float, float, float, float]] = (0.6, 0.6, 0.6, 1.0)

# Height at which terrain is treated as "cliff/plateau"
CLIFF_THRESHOLD: Final[int] = 250
CLIFF_HEIGHT: Final[float] = 8.0
CLIFF_COLOR: Final[tuple[float, float, float, float]] = (0.55, 0.45, 0.35, 1.0)

# Viewer defaults
DEFAULT_HEIGHT_SCALE: Final[float] = 0.1
DEFAULT_YAW: Final[float] = 45.0
DEFAULT_PITCH: Final[float] = 30.0
ZOOM_STEP: Final[float] = 3.0
ZOOM_MIN: Final[float] = 5.0
ZOOM_MAX: Final[float] = 500.0
MOVE_SPEED: Final[float] = 0.5
HEIGHT_SCALE_FACTOR: Final[float] = 1.2
WINDOW_WIDTH: Final[int] = 1280
WINDOW_HEIGHT: Final[int] = 800


# ─── Data Model ───────────────────────────────────────────────────────────────

type RGBA = tuple[float, float, float, float]
type RGB = tuple[float, float, float]
type ObjectEntry = tuple[str, float, float, float, float]


@dataclass(slots=True)
class LevelData:
    """Parsed HMAP level data."""

    name: str
    grid_size: int
    terrain_scale: int
    layer_count: int
    heightmaps: list[list[list[int]]] = field(default_factory=list)
    combined_heightmap: list[list[int]] = field(default_factory=list)
    object_types: list[str] = field(default_factory=list)
    item_types: list[str] = field(default_factory=list)
    objects: list[ObjectEntry] = field(default_factory=list)

    @property
    def full_height(self) -> int:
        """Total row count across all layers."""
        return len(self.combined_heightmap)

    @property
    def scale_factor(self) -> float:
        """World-units per grid cell."""
        return self.terrain_scale / self.grid_size if self.grid_size else 1.0


# ─── HMAP Parser ──────────────────────────────────────────────────────────────


def _read_string_table(data: bytes, offset: int, count: int) -> tuple[list[str], int]:
    """Read `count` length-prefixed ASCII strings from `data` at `offset`."""
    strings: list[str] = []
    data_len: int = len(data)
    for _ in range(count):
        if offset >= data_len:
            break
        nl: int = data[offset]
        offset += 1
        if offset + nl > data_len:
            break
        strings.append(
            data[offset : offset + nl].decode("ascii", errors="replace").rstrip("\x00")
        )
        offset += nl
    return strings, offset


def parse_hmap(data: bytes, name: str = "Unknown") -> LevelData | None:
    """Parse an HMAP binary level file into a LevelData structure.

    Returns None if the data is too short or lacks the HMAP magic bytes.
    """
    if len(data) < HEADER_SIZE or data[:4] != HEADER_MAGIC:
        return None

    (
        _version,
        grid_size,
        terrain_scale,
        _object_count,
        object_type_count,
        item_type_count,
    ) = struct.unpack_from("<6I", data, 4)

    level = LevelData(
        name=name,
        grid_size=grid_size,
        terrain_scale=terrain_scale,
        layer_count=item_type_count,
    )

    offset: int = HEADER_SIZE

    # String tables
    level.object_types, offset = _read_string_table(data, offset, object_type_count)
    level.item_types, offset = _read_string_table(data, offset, item_type_count)

    # Locate first 0xFFFF marker to bound heightmap region
    first_marker: int = data.find(OBJECT_MARKER, offset)
    if first_marker == -1:
        first_marker = len(data)

    # Bulk-unpack heightmap layers
    cells_per_layer: int = grid_size * grid_size
    layer_byte_size: int = cells_per_layer * 4
    actual_layers: int = (first_marker - offset) // layer_byte_size
    level.layer_count = actual_layers

    for _ in range(actual_layers):
        if offset + layer_byte_size > len(data):
            break
        raw: tuple[int, ...] = struct.unpack_from(f"<{cells_per_layer}I", data, offset)
        hmap: list[list[int]] = [
            [min(255, raw[y * grid_size + x]) for x in range(grid_size)]
            for y in range(grid_size)
        ]
        level.heightmaps.append(hmap)
        offset += layer_byte_size

    # Flatten layers into combined heightmap
    level.combined_heightmap = [row for hmap in level.heightmaps for row in hmap]

    # Parse object entries at 0xFFFF markers
    scale: float = level.scale_factor
    num_types: int = len(level.object_types)
    search_start: int = offset

    while True:
        pos: int = data.find(OBJECT_MARKER, search_start)
        if pos == -1 or pos + OBJECT_RECORD_SIZE > len(data):
            break
        type_idx, x_grid, y_grid = struct.unpack_from("<3H", data, pos + 2)
        if type_idx < num_types:
            level.objects.append(
                (
                    level.object_types[type_idx],
                    x_grid * scale,
                    y_grid * scale,
                    0.0,
                    0.0,
                )
            )
        search_start = pos + OBJECT_RECORD_SIZE

    return level


# ─── Geometry Helpers ─────────────────────────────────────────────────────────


def height_z(h: int, hs: float) -> float:
    """Convert a heightmap value to a Z coordinate."""
    return (CLIFF_HEIGHT if h >= CLIFF_THRESHOLD else float(h)) * hs


def height_color(h: int, color_mode: int) -> RGBA:
    """Map a heightmap value to an RGBA color based on the active color mode."""
    if h >= CLIFF_THRESHOLD:
        return CLIFF_COLOR

    t: float = h / (CLIFF_THRESHOLD - 1.0)

    c: RGB
    match color_mode:
        case 0:  # terrain
            if t < 0.25:
                c = (0.35, 0.55 + t * 0.3, 0.2)
            elif t < 0.5:
                tt = (t - 0.25) * 4.0
                c = (0.3 + tt * 0.15, 0.5 - tt * 0.1, 0.15 + tt * 0.1)
            elif t < 0.75:
                tt = (t - 0.5) * 4.0
                c = (0.5 + tt * 0.15, 0.4 + tt * 0.05, 0.25 + tt * 0.1)
            else:
                tt = (t - 0.75) * 4.0
                c = (0.6 + tt * 0.25, 0.55 + tt * 0.25, 0.5 + tt * 0.3)
        case 1:  # gradient
            c = (0.2 + t * 0.6, 0.6 - t * 0.3, 0.2)
        case 2:  # contour
            band: int = int(h / 15) % 2
            bg: float = 0.4 + t * 0.3
            c = (0.3, bg + 0.1, 0.2) if band else (0.25, bg, 0.15)
        case _:  # satellite
            if t < 0.4:
                c = (0.3 + t * 0.2, 0.45 + t * 0.2, 0.2)
            else:
                tt = (t - 0.4) / 0.6
                c = (0.4 + tt * 0.35, 0.5 - tt * 0.1, 0.25 + tt * 0.2)

    return (*c, 1.0)


def obj_color(name: str) -> RGBA:
    """Return an RGBA color for an object based on keyword matching."""
    nl: str = name.lower()
    for keys, color in _OBJ_COLOR_MAP:
        if any(k in nl for k in keys):
            return color
    return _OBJ_COLOR_DEFAULT


def build_terrain(level: LevelData, hs: float, cm: int) -> GeomNode:
    """Build a colored terrain mesh from the combined heightmap."""
    hmap: list[list[int]] = level.combined_heightmap
    w: int = level.grid_size
    h: int = len(hmap)

    fmt: GeomVertexFormat = GeomVertexFormat.get_v3c4()
    vdata: GeomVertexData = GeomVertexData("terrain", fmt, Geom.UH_static)
    total_verts: int = w * h
    vdata.set_num_rows(total_verts)

    vw: GeomVertexWriter = GeomVertexWriter(vdata, "vertex")
    cw: GeomVertexWriter = GeomVertexWriter(vdata, "color")

    for y in range(h):
        row: list[int] = hmap[y]
        for x in range(w):
            hv: int = row[x]
            vw.add_data3(float(x), float(y), height_z(hv, hs))
            cw.add_data4(*height_color(hv, cm))

    tris: GeomTriangles = GeomTriangles(Geom.UH_static)
    for y in range(h - 1):
        base: int = y * w
        for x in range(w - 1):
            i: int = base + x
            # Two triangles per quad — consistent CCW winding
            tris.add_vertices(i, i + w, i + 1)
            tris.add_vertices(i + 1, i + w, i + w + 1)

    geom: Geom = Geom(vdata)
    geom.add_primitive(tris)
    node: GeomNode = GeomNode("terrain")
    node.add_geom(geom)
    return node


def build_grid(level: LevelData) -> NodePath:
    """Build a reference grid overlay."""
    w: int = level.grid_size
    h: int = level.full_height

    segs: LineSegs = LineSegs("grid")
    segs.set_color(0.3, 0.3, 0.3, 1.0)
    segs.set_thickness(1.0)

    y_max: float = float(h - 1)
    x_max: float = float(w - 1)

    # Vertical lines
    for i in range(0, w + 1, GRID_LINE_SPACING):
        fi: float = float(i)
        segs.move_to(fi, 0.0, 0.2)
        segs.draw_to(fi, y_max, 0.2)

    # Horizontal lines — every grid_size rows (layer boundaries)
    for i in range(0, h + 1, level.grid_size):
        fi: float = float(i)
        segs.move_to(0.0, fi, 0.2)
        segs.draw_to(x_max, fi, 0.2)

    return NodePath(segs.create())


def _make_cube_geom(color: RGBA, size: float = 0.25) -> GeomNode:
    """Build a solid cube with correct CCW winding for all 6 faces."""
    fmt: GeomVertexFormat = GeomVertexFormat.get_v3c4()
    vdata: GeomVertexData = GeomVertexData("cube", fmt, Geom.UH_static)
    vdata.set_num_rows(24)  # 6 faces × 4 verts

    vw: GeomVertexWriter = GeomVertexWriter(vdata, "vertex")
    cw: GeomVertexWriter = GeomVertexWriter(vdata, "color")

    s: float = size
    # 6 faces, each 4 vertices in CCW order (viewed from outside)
    faces: list[list[tuple[float, float, float]]] = [
        [(-s, -s, s), (s, -s, s), (s, s, s), (-s, s, s)],  # Top +Z
        [(-s, s, -s), (s, s, -s), (s, -s, -s), (-s, -s, -s)],  # Bottom -Z
        [(-s, -s, -s), (s, -s, -s), (s, -s, s), (-s, -s, s)],  # Front -Y
        [(s, s, -s), (-s, s, -s), (-s, s, s), (s, s, s)],  # Back +Y
        [(-s, s, -s), (-s, -s, -s), (-s, -s, s), (-s, s, s)],  # Left -X
        [(s, -s, -s), (s, s, -s), (s, s, s), (s, -s, s)],  # Right +X
    ]

    tris: GeomTriangles = GeomTriangles(Geom.UH_static)
    vi: int = 0
    for face in faces:
        for v in face:
            vw.add_data3(*v)
            cw.add_data4(*color)
        tris.add_vertices(vi, vi + 1, vi + 2)
        tris.add_vertices(vi, vi + 2, vi + 3)
        vi += 4

    geom: Geom = Geom(vdata)
    geom.add_primitive(tris)
    node: GeomNode = GeomNode("cube")
    node.add_geom(geom)
    return node


def build_objects(level: LevelData, hs: float) -> NodePath:
    """Place colored cubes at each object position, snapped to terrain height."""
    hmap: list[list[int]] = level.combined_heightmap
    w: int = level.grid_size
    h: int = level.full_height
    scale: float = level.scale_factor

    root: NodePath = NodePath("objects")

    for obj_name, xw, yw, _, _ in level.objects:
        gx: float = xw / scale
        gy: float = yw / scale
        ix: int = int(gx)
        iy: int = int(gy)

        # Use height_z for consistency with terrain rendering (bug fix)
        if 0 <= ix < w and 0 <= iy < h:
            gz: float = height_z(hmap[iy][ix], hs)
        else:
            gz = 0.0

        color: RGBA = obj_color(obj_name)
        np: NodePath = root.attach_new_node(_make_cube_geom(color))
        np.set_pos(gx, gy, gz + 0.3)

    return root


# ─── Viewer ───────────────────────────────────────────────────────────────────


class Viewer(ShowBase):
    """Interactive 3D viewer for parsed HMAP levels."""

    def __init__(self, level: LevelData, wireframe: bool = False) -> None:
        super().__init__()

        self.level: LevelData = level
        self.hs: float = DEFAULT_HEIGHT_SCALE
        self.cm: int = 0
        self.wf: bool = wireframe
        self.show_grid: bool = True
        self.show_obj: bool = True
        self.show_help: bool = True
        self.fog_on: bool = True
        self.dragging: bool = False
        self.last_m: tuple[float, float] = (0.0, 0.0)

        fh: int = level.full_height
        self.cam_yaw: float = DEFAULT_YAW
        self.cam_pitch: float = DEFAULT_PITCH
        self.cam_dist: float = max(level.grid_size, fh) * 0.8
        self.cam_ctr: LVector3 = LVector3(level.grid_size / 2.0, fh / 2.0, 0.0)

        # Window setup
        props: WindowProperties = WindowProperties()
        props.set_title(f"Level Viewer — {level.name}")
        props.set_size(WINDOW_WIDTH, WINDOW_HEIGHT)
        self.win.request_properties(props)
        self.set_background_color(0.4, 0.6, 0.85, 1.0)
        self.disable_mouse()

        # Render settings
        self.render.set_antialias(AntialiasAttrib.M_auto)
        self.render.set_attrib(CullFaceAttrib.make(CullFaceAttrib.M_cull_none))
        self.render.set_light_off()

        # Scene nodes
        self.terrain_np: NodePath | None = None
        self.grid_np: NodePath | None = None
        self.obj_np: NodePath | None = None
        self._rebuild()

        # Fog
        self.fog: Fog = Fog("fog")
        self.fog.set_color(0.5, 0.6, 0.75)
        self.fog.set_linear_range(80.0, 250.0)
        self.render.set_fog(self.fog)

        # HUD
        self.help_nodes: list[OnscreenText] = []
        self._build_hud()
        self.status: OnscreenText = OnscreenText(
            "",
            pos=(-1.3, -0.95),
            scale=0.04,
            fg=(0.6, 1.0, 0.6, 1.0),
            bg=(0.08, 0.08, 0.16, 0.7),
            align=TextNode.A_left,
            mayChange=True,
        )
        self._update_status()

        # Key bindings
        self._bind_keys()

        # Tasks
        self.taskMgr.add(self._camera_task, "camera")
        self.taskMgr.add(self._keyboard_task, "keyboard")

    # ── Scene building ──

    def _rebuild(self) -> None:
        """Tear down and rebuild all scene geometry."""
        for np in (self.terrain_np, self.grid_np, self.obj_np):
            if np is not None:
                np.remove_node()

        self.terrain_np = self.render.attach_new_node(
            build_terrain(self.level, self.hs, self.cm)
        )
        if self.wf:
            self.terrain_np.set_render_mode_wireframe()
        else:
            self.terrain_np.clear_render_mode()

        self.grid_np = build_grid(self.level)
        self.grid_np.reparent_to(self.render)
        if not self.show_grid:
            self.grid_np.hide()

        self.obj_np = build_objects(self.level, self.hs)
        self.obj_np.reparent_to(self.render)
        if not self.show_obj:
            self.obj_np.hide()

    # ── HUD ──

    def _build_hud(self) -> None:
        lines: list[str] = [
            "Mouse drag — Rotate | Scroll — Zoom | WASD — Move | +/- Height",
            "F Wire | G Grid | O Objs | C Color | V Fog | H Help | R Reset | ESC Quit",
        ]
        self.help_nodes = [
            OnscreenText(
                line,
                pos=(-1.3, 0.95 - i * 0.05),
                scale=0.038,
                fg=(0.9, 0.9, 0.9, 1.0),
                bg=(0.08, 0.08, 0.16, 0.6),
                align=TextNode.A_left,
            )
            for i, line in enumerate(lines)
        ]

    def _update_status(self) -> None:
        w: int = self.level.grid_size
        h: int = self.level.full_height
        self.status.setText(
            f"{self.level.name} | {w}×{h} | {len(self.level.objects)} objs | "
            f"{COLOR_MODES[self.cm]}"
        )

    # ── Input bindings ──

    def _bind_keys(self) -> None:
        bindings: list[tuple[str, callable]] = [
            ("escape", sys.exit),
            ("f", self._toggle_wireframe),
            ("g", self._toggle_grid),
            ("o", self._toggle_objects),
            ("c", self._cycle_color_mode),
            ("v", self._toggle_fog),
            ("h", self._toggle_help),
            ("r", self._reset_view),
            ("+", self._scale_up),
            ("=", self._scale_up),
            ("shift-=", self._scale_up),
            ("-", self._scale_down),
            ("mouse1", self._mouse_down),
            ("mouse1-up", self._mouse_up),
            ("wheel_up", self._zoom_in),
            ("wheel_down", self._zoom_out),
        ]
        for key, fn in bindings:
            self.accept(key, fn)

    # ── Camera ──

    def _apply_camera(self) -> None:
        yr: float = math.radians(self.cam_yaw)
        pr: float = math.radians(self.cam_pitch)
        d: float = self.cam_dist
        cos_pr: float = math.cos(pr)
        ex: float = self.cam_ctr.x + d * math.sin(yr) * cos_pr
        ey: float = self.cam_ctr.y - d * math.cos(yr) * cos_pr
        ez: float = self.cam_ctr.z + d * math.sin(pr)
        self.camera.set_pos(ex, ey, ez)
        self.camera.look_at(self.cam_ctr)

    def _camera_task(self, task: Task) -> int:
        if self.dragging and self.mouseWatcherNode.has_mouse():
            mx: float = self.mouseWatcherNode.get_mouse_x()
            my: float = self.mouseWatcherNode.get_mouse_y()
            dx: float = (mx - self.last_m[0]) * 150.0
            dy: float = (my - self.last_m[1]) * 150.0
            self.cam_yaw += dx
            self.cam_pitch = max(-89.0, min(89.0, self.cam_pitch - dy))
            self.last_m = (mx, my)
        self._apply_camera()
        return Task.cont

    def _keyboard_task(self, task: Task) -> int:
        r: float = math.radians(self.cam_yaw)
        sin_r: float = math.sin(r)
        cos_r: float = math.cos(r)
        isd = self.mouseWatcherNode.is_button_down

        if isd(KeyboardButton.ascii_key("w")):
            self.cam_ctr.x -= sin_r * MOVE_SPEED
            self.cam_ctr.y += cos_r * MOVE_SPEED
        if isd(KeyboardButton.ascii_key("s")):
            self.cam_ctr.x += sin_r * MOVE_SPEED
            self.cam_ctr.y -= cos_r * MOVE_SPEED
        if isd(KeyboardButton.ascii_key("a")):
            self.cam_ctr.x -= cos_r * MOVE_SPEED
            self.cam_ctr.y -= sin_r * MOVE_SPEED
        if isd(KeyboardButton.ascii_key("d")):
            self.cam_ctr.x += cos_r * MOVE_SPEED
            self.cam_ctr.y += sin_r * MOVE_SPEED

        return Task.cont

    # ── Mouse handlers ──

    def _mouse_down(self) -> None:
        self.dragging = True
        if self.mouseWatcherNode.has_mouse():
            self.last_m = (
                self.mouseWatcherNode.get_mouse_x(),
                self.mouseWatcherNode.get_mouse_y(),
            )

    def _mouse_up(self) -> None:
        self.dragging = False

    def _zoom_in(self) -> None:
        self.cam_dist = max(ZOOM_MIN, self.cam_dist - ZOOM_STEP)

    def _zoom_out(self) -> None:
        self.cam_dist = min(ZOOM_MAX, self.cam_dist + ZOOM_STEP)

    # ── Toggle / cycle handlers ──

    def _toggle_wireframe(self) -> None:
        self.wf = not self.wf
        if self.wf:
            self.terrain_np.set_render_mode_wireframe()
        else:
            self.terrain_np.clear_render_mode()

    def _toggle_grid(self) -> None:
        self.show_grid = not self.show_grid
        (self.grid_np.show if self.show_grid else self.grid_np.hide)()

    def _toggle_objects(self) -> None:
        self.show_obj = not self.show_obj
        (self.obj_np.show if self.show_obj else self.obj_np.hide)()

    def _cycle_color_mode(self) -> None:
        self.cm = (self.cm + 1) % len(COLOR_MODES)
        self._rebuild()
        self._update_status()

    def _toggle_fog(self) -> None:
        self.fog_on = not self.fog_on
        if self.fog_on:
            self.render.set_fog(self.fog)
        else:
            self.render.clear_fog()

    def _toggle_help(self) -> None:
        self.show_help = not self.show_help
        for t in self.help_nodes:
            (t.show if self.show_help else t.hide)()

    def _scale_up(self) -> None:
        self.hs *= HEIGHT_SCALE_FACTOR
        self._rebuild()

    def _scale_down(self) -> None:
        self.hs /= HEIGHT_SCALE_FACTOR
        self._rebuild()

    def _reset_view(self) -> None:
        fh: int = self.level.full_height
        self.cam_yaw = DEFAULT_YAW
        self.cam_pitch = DEFAULT_PITCH
        self.cam_dist = max(self.level.grid_size, fh) * 0.8
        self.cam_ctr = LVector3(self.level.grid_size / 2.0, fh / 2.0, 0.0)
        self.hs = DEFAULT_HEIGHT_SCALE
        self.cm = 0
        self._rebuild()
        self._update_status()


# ─── Entry Point ──────────────────────────────────────────────────────────────


def main() -> int:
    ap: argparse.ArgumentParser = argparse.ArgumentParser(
        description="AirStrike 3D Level Viewer (Panda3D)"
    )
    ap.add_argument("level", type=Path, help="Level file (.hsc)")
    ap.add_argument(
        "-w", "--wireframe", action="store_true", help="Start in wireframe mode"
    )
    args: argparse.Namespace = ap.parse_args()

    if not args.level.exists():
        print(f"Error: {args.level} not found", file=sys.stderr)
        return 1

    try:
        raw_data: bytes = args.level.read_bytes()
    except OSError as exc:
        print(f"Error reading {args.level}: {exc}", file=sys.stderr)
        return 1

    level: LevelData | None = parse_hmap(raw_data, args.level.stem)
    if level is None:
        print(
            "Error: Invalid HMAP file (missing header or magic bytes)", file=sys.stderr
        )
        return 1

    fh: int = level.full_height
    print(
        f"{level.name}: {level.grid_size}×{fh} "
        f"({level.layer_count} layer{'s' if level.layer_count != 1 else ''}), "
        f"{len(level.objects)} objects, "
        f"{len(level.object_types)} object types, "
        f"{len(level.item_types)} item types"
    )

    try:
        Viewer(level, args.wireframe).run()
    except Exception as exc:
        print(f"Viewer error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
