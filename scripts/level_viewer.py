#!/usr/bin/env python3
"""AirStrike 3D level viewer — Panda3D rewrite."""

import argparse, math, struct, sys
from dataclasses import dataclass, field
from pathlib import Path
from direct.showbase.ShowBase import ShowBase
from direct.gui.OnscreenText import OnscreenText
from direct.task import Task
from panda3d.core import (
    GeomVertexFormat,
    GeomVertexData,
    GeomVertexWriter,
    Geom,
    GeomTriangles,
    GeomNode,
    NodePath,
    LVector3,
    Fog,
    TextNode,
    WindowProperties,
    LineSegs,
    KeyboardButton,
    AntialiasAttrib,
    CullFaceAttrib,
    LightAttrib,
    ColorAttrib,
)


@dataclass
class LevelData:
    name: str
    grid_size: int
    terrain_scale: int
    layer_count: int
    heightmaps: list = field(default_factory=list)
    combined_heightmap: list = field(default_factory=list)
    object_types: list = field(default_factory=list)
    item_types: list = field(default_factory=list)
    objects: list = field(default_factory=list)


def parse_hmap(data: bytes, name: str = "Unknown"):
    if len(data) < 28 or data[:4] != b"HMAP":
        return None
    (
        version,
        grid_size,
        terrain_scale,
        object_count,
        object_type_count,
        item_type_count,
    ) = struct.unpack_from("<6I", data, 4)
    level = LevelData(
        name=name,
        grid_size=grid_size,
        terrain_scale=terrain_scale,
        layer_count=item_type_count,
    )
    offset = 28
    for _ in range(object_type_count):
        if offset >= len(data):
            break
        nl = data[offset]
        offset += 1
        if offset + nl > len(data):
            break
        level.object_types.append(
            data[offset : offset + nl].decode("ascii", errors="replace").rstrip("\x00")
        )
        offset += nl
    for _ in range(item_type_count):
        if offset >= len(data):
            break
        nl = data[offset]
        offset += 1
        if offset + nl > len(data):
            break
        level.item_types.append(
            data[offset : offset + nl].decode("ascii", errors="replace").rstrip("\x00")
        )
        offset += nl
    first_ffff = len(data)
    for i in range(offset, len(data) - 1):
        if data[i : i + 2] == b"\xff\xff":
            first_ffff = i
            break
    layer_size = grid_size * grid_size * 4
    actual_layers = (first_ffff - offset) // layer_size
    level.layer_count = actual_layers
    for _ in range(actual_layers):
        hmap = []
        for y in range(grid_size):
            row = []
            for x in range(grid_size):
                h = (
                    min(255, struct.unpack_from("<I", data, offset)[0])
                    if offset + 4 <= len(data)
                    else 128
                )
                row.append(h)
                offset += 4
            hmap.append(row)
        level.heightmaps.append(hmap)
    for hmap in level.heightmaps:
        for row in hmap:
            level.combined_heightmap.append(row)
    ffff_positions = []
    for i in range(offset, len(data) - 1):
        if data[i : i + 2] == b"\xff\xff":
            ffff_positions.append(i)
    scale = terrain_scale / grid_size
    for pos in ffff_positions:
        if pos + 8 > len(data):
            break
        type_idx = struct.unpack_from("<H", data, pos + 2)[0]
        x_grid = struct.unpack_from("<H", data, pos + 4)[0]
        y_grid = struct.unpack_from("<H", data, pos + 6)[0]
        if type_idx >= len(level.object_types):
            continue
        level.objects.append(
            (level.object_types[type_idx], x_grid * scale, y_grid * scale, 0.0, 0.0)
        )
    return level


COLOR_MODES = ["terrain", "gradient", "contour", "satellite"]


def height_z(h, hs):
    return (8.0 if h >= 250 else h) * hs


def height_color(h, color_mode):
    if h >= 250:
        return (0.55, 0.45, 0.35, 1.0)
    t = h / 249.0
    if color_mode == 0:
        if t < 0.25:
            c = (0.35, 0.55 + t * 0.3, 0.2)
        elif t < 0.5:
            tt = (t - 0.25) / 0.25
            c = (0.3 + tt * 0.15, 0.5 - tt * 0.1, 0.15 + tt * 0.1)
        elif t < 0.75:
            tt = (t - 0.5) / 0.25
            c = (0.5 + tt * 0.15, 0.4 + tt * 0.05, 0.25 + tt * 0.1)
        else:
            tt = (t - 0.75) / 0.25
            c = (0.6 + tt * 0.25, 0.55 + tt * 0.25, 0.5 + tt * 0.3)
    elif color_mode == 1:
        c = (0.2 + t * 0.6, 0.6 - t * 0.3, 0.2)
    elif color_mode == 2:
        band = int(h / 15) % 2
        bg = 0.4 + t * 0.3
        c = (0.3, bg + 0.1, 0.2) if band else (0.25, bg, 0.15)
    else:
        if t < 0.4:
            c = (0.3 + t * 0.2, 0.45 + t * 0.2, 0.2)
        else:
            tt = (t - 0.4) / 0.6
            c = (0.4 + tt * 0.35, 0.5 - tt * 0.1, 0.25 + tt * 0.2)
    return (*c, 1.0)


def obj_color(name):
    nl = name.lower()
    if any(k in nl for k in ("helic", "tank", "turret", "btr")):
        return (1.0, 0.2, 0.2, 1.0)
    if "item" in nl:
        return (1.0, 1.0, 0.2, 1.0)
    if any(k in nl for k in ("tree", "grass", "kust", "kamni")):
        return (0.2, 0.8, 0.2, 1.0)
    if any(k in nl for k in ("kolhoz", "dom", "angar", "zabor")):
        return (0.8, 0.5, 0.2, 1.0)
    if any(k in nl for k in ("jeep", "uaz", "gruzovik")):
        return (0.9, 0.6, 0.1, 1.0)
    return (0.6, 0.6, 0.6, 1.0)


def build_terrain(level, hs, cm):
    hmap = level.combined_heightmap
    w, h = level.grid_size, len(hmap)
    fmt = GeomVertexFormat.get_v3c4()
    vdata = GeomVertexData("terrain", fmt, Geom.UH_static)
    vdata.set_num_rows(w * h)
    vw = GeomVertexWriter(vdata, "vertex")
    cw = GeomVertexWriter(vdata, "color")
    for y in range(h):
        for x in range(w):
            hv = hmap[y][x]
            vw.add_data3(float(x), float(y), height_z(hv, hs))
            cw.add_data4(*height_color(hv, cm))
    tris = GeomTriangles(Geom.UH_static)
    for y in range(h - 1):
        for x in range(w - 1):
            i = y * w + x
            # Two triangles per quad, consistent CCW winding
            tris.add_vertices(i, i + w, i + 1)
            tris.add_vertices(i + 1, i + w, i + w + 1)
    geom = Geom(vdata)
    geom.add_primitive(tris)
    node = GeomNode("terrain")
    node.add_geom(geom)
    return node


def build_grid(level):
    hmap = level.combined_heightmap
    w, h = level.grid_size, len(hmap)
    segs = LineSegs("grid")
    segs.set_color(0.3, 0.3, 0.3, 1.0)
    segs.set_thickness(1.0)
    for i in range(0, w + 1, 8):
        segs.move_to(float(i), 0.0, 0.2)
        segs.draw_to(float(i), float(h - 1), 0.2)
    for i in range(0, h + 1, level.grid_size):
        segs.move_to(0.0, float(i), 0.2)
        segs.draw_to(float(w - 1), float(i), 0.2)
    return NodePath(segs.create())


def make_cube_geom(color, size=0.25):
    """Build a proper solid cube with correct winding for all 6 faces."""
    fmt = GeomVertexFormat.get_v3c4()
    vdata = GeomVertexData("cube", fmt, Geom.UH_static)
    vdata.set_num_rows(24)  # 6 faces × 4 verts
    vw = GeomVertexWriter(vdata, "vertex")
    cw = GeomVertexWriter(vdata, "color")
    s = size
    # Define 6 faces, each with 4 vertices in CCW order (viewed from outside)
    faces = [
        # Top (+Z)
        [(-s, -s, s), (s, -s, s), (s, s, s), (-s, s, s)],
        # Bottom (-Z)
        [(-s, s, -s), (s, s, -s), (s, -s, -s), (-s, -s, -s)],
        # Front (-Y)
        [(-s, -s, -s), (s, -s, -s), (s, -s, s), (-s, -s, s)],
        # Back (+Y)
        [(s, s, -s), (-s, s, -s), (-s, s, s), (s, s, s)],
        # Left (-X)
        [(-s, s, -s), (-s, -s, -s), (-s, -s, s), (-s, s, s)],
        # Right (+X)
        [(s, -s, -s), (s, s, -s), (s, s, s), (s, -s, s)],
    ]
    tris = GeomTriangles(Geom.UH_static)
    vi = 0
    for face in faces:
        for v in face:
            vw.add_data3(*v)
            cw.add_data4(*color)
        tris.add_vertices(vi, vi + 1, vi + 2)
        tris.add_vertices(vi, vi + 2, vi + 3)
        vi += 4
    geom = Geom(vdata)
    geom.add_primitive(tris)
    node = GeomNode("cube")
    node.add_geom(geom)
    return node


def build_objects(level, hs):
    hmap = level.combined_heightmap
    w, h = level.grid_size, len(hmap)
    scale = level.terrain_scale / level.grid_size
    root = NodePath("objects")
    for name, xw, yw, _, _ in level.objects:
        gx, gy = xw / scale, yw / scale
        ix, iy = int(gx), int(gy)
        gz = hmap[iy][ix] * hs if 0 <= ix < w and 0 <= iy < h else 0.0
        color = obj_color(name)
        np = root.attach_new_node(make_cube_geom(color))
        np.set_pos(gx, gy, gz + 0.3)
    return root


class Viewer(ShowBase):
    def __init__(self, level, wireframe=False):
        super().__init__()
        self.level = level
        self.hs = 0.1
        self.cm = 0
        self.wf = wireframe
        self.show_grid = True
        self.show_obj = True
        self.show_help = True
        self.fog_on = True
        self.dragging = False
        self.last_m = (0, 0)
        fh = level.grid_size * level.layer_count
        self.cam_yaw = 45.0
        self.cam_pitch = 30.0
        self.cam_dist = max(level.grid_size, fh) * 0.8
        self.cam_ctr = LVector3(level.grid_size / 2.0, fh / 2.0, 0.0)

        props = WindowProperties()
        props.set_title(f"Level Viewer — {level.name}")
        props.set_size(1280, 800)
        self.win.request_properties(props)
        self.set_background_color(0.4, 0.6, 0.85, 1.0)
        self.disable_mouse()
        self.render.set_antialias(AntialiasAttrib.M_auto)
        # Disable backface culling globally so terrain is visible from both sides
        self.render.set_attrib(CullFaceAttrib.make(CullFaceAttrib.M_cull_none))
        # Disable default lighting so vertex colors show correctly
        self.render.set_light_off()

        self.terrain_np = None
        self.grid_np = None
        self.obj_np = None
        self._rebuild()

        self.fog = Fog("fog")
        self.fog.set_color(0.5, 0.6, 0.75)
        self.fog.set_linear_range(80.0, 250.0)
        self.render.set_fog(self.fog)

        self.help_nodes = []
        self._build_hud()
        self.status = OnscreenText(
            "",
            pos=(-1.3, -0.95),
            scale=0.04,
            fg=(0.6, 1, 0.6, 1),
            bg=(0.08, 0.08, 0.16, 0.7),
            align=TextNode.A_left,
            mayChange=True,
        )
        self._upd_status()

        for key, fn in [
            ("escape", sys.exit),
            ("f", self._tog_wf),
            ("g", self._tog_grid),
            ("o", self._tog_obj),
            ("c", self._cyc_cm),
            ("v", self._tog_fog),
            ("h", self._tog_help),
            ("r", self._reset),
            ("+", self._su),
            ("=", self._su),
            ("shift-=", self._su),
            ("-", self._sd),
            ("mouse1", self._md),
            ("mouse1-up", self._mu),
            ("wheel_up", self._zi),
            ("wheel_down", self._zo),
        ]:
            self.accept(key, fn)

        self.taskMgr.add(self._cam_task, "cam")
        self.taskMgr.add(self._key_task, "keys")

    def _rebuild(self):
        for np in (self.terrain_np, self.grid_np, self.obj_np):
            if np:
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

    def _build_hud(self):
        lines = [
            "Mouse drag - Rotate | Scroll - Zoom | WASD - Move | +/- Height",
            "F Wire | G Grid | O Objs | C Color | V Fog | H Help | R Reset | ESC Quit",
        ]
        self.help_nodes = []
        for i, l in enumerate(lines):
            t = OnscreenText(
                l,
                pos=(-1.3, 0.95 - i * 0.05),
                scale=0.038,
                fg=(0.9, 0.9, 0.9, 1),
                bg=(0.08, 0.08, 0.16, 0.6),
                align=TextNode.A_left,
            )
            self.help_nodes.append(t)

    def _upd_status(self):
        w, h = self.level.grid_size, len(self.level.combined_heightmap)
        self.status.setText(
            f"{self.level.name} | {w}x{h} | {len(self.level.objects)} objs | {COLOR_MODES[self.cm]}"
        )

    def _apply_cam(self):
        yr, pr = math.radians(self.cam_yaw), math.radians(self.cam_pitch)
        d = self.cam_dist
        ex = self.cam_ctr.x + d * math.sin(yr) * math.cos(pr)
        ey = self.cam_ctr.y - d * math.cos(yr) * math.cos(pr)
        ez = self.cam_ctr.z + d * math.sin(pr)
        self.camera.set_pos(ex, ey, ez)
        self.camera.look_at(self.cam_ctr)

    def _cam_task(self, task):
        if self.dragging and self.mouseWatcherNode.has_mouse():
            mx = self.mouseWatcherNode.get_mouse_x()
            my = self.mouseWatcherNode.get_mouse_y()
            dx, dy = (mx - self.last_m[0]) * 300, (my - self.last_m[1]) * 300
            self.cam_yaw += dx * 0.5
            self.cam_pitch = max(-89, min(89, self.cam_pitch - dy * 0.5))
            self.last_m = (mx, my)
        self._apply_cam()
        return Task.cont

    def _key_task(self, task):
        sp, r = 0.5, math.radians(self.cam_yaw)
        isd = self.mouseWatcherNode.is_button_down
        if isd(KeyboardButton.ascii_key("w")):
            self.cam_ctr.x -= math.sin(r) * sp
            self.cam_ctr.y += math.cos(r) * sp
        if isd(KeyboardButton.ascii_key("s")):
            self.cam_ctr.x += math.sin(r) * sp
            self.cam_ctr.y -= math.cos(r) * sp
        if isd(KeyboardButton.ascii_key("a")):
            self.cam_ctr.x -= math.cos(r) * sp
            self.cam_ctr.y -= math.sin(r) * sp
        if isd(KeyboardButton.ascii_key("d")):
            self.cam_ctr.x += math.cos(r) * sp
            self.cam_ctr.y += math.sin(r) * sp
        return Task.cont

    def _md(self):
        self.dragging = True
        if self.mouseWatcherNode.has_mouse():
            self.last_m = (
                self.mouseWatcherNode.get_mouse_x(),
                self.mouseWatcherNode.get_mouse_y(),
            )

    def _mu(self):
        self.dragging = False

    def _zi(self):
        self.cam_dist = max(5, self.cam_dist - 3)

    def _zo(self):
        self.cam_dist = min(500, self.cam_dist + 3)

    def _tog_wf(self):
        self.wf = not self.wf
        if self.wf:
            self.terrain_np.set_render_mode_wireframe()
        else:
            self.terrain_np.clear_render_mode()

    def _tog_grid(self):
        self.show_grid = not self.show_grid
        self.grid_np.show() if self.show_grid else self.grid_np.hide()

    def _tog_obj(self):
        self.show_obj = not self.show_obj
        self.obj_np.show() if self.show_obj else self.obj_np.hide()

    def _cyc_cm(self):
        self.cm = (self.cm + 1) % len(COLOR_MODES)
        self._rebuild()
        self._upd_status()

    def _tog_fog(self):
        self.fog_on = not self.fog_on
        self.render.set_fog(self.fog) if self.fog_on else self.render.clear_fog()

    def _tog_help(self):
        self.show_help = not self.show_help
        for t in self.help_nodes:
            t.show() if self.show_help else t.hide()

    def _su(self):
        self.hs *= 1.2
        self._rebuild()

    def _sd(self):
        self.hs /= 1.2
        self._rebuild()

    def _reset(self):
        fh = self.level.grid_size * self.level.layer_count
        self.cam_yaw, self.cam_pitch = 45.0, 30.0
        self.cam_dist = max(self.level.grid_size, fh) * 0.8
        self.cam_ctr = LVector3(self.level.grid_size / 2.0, fh / 2.0, 0.0)
        self.hs = 0.1
        self.cm = 0
        self._rebuild()
        self._upd_status()


def main():
    ap = argparse.ArgumentParser(description="AirStrike 3D Level Viewer (Panda3D)")
    ap.add_argument("level", type=Path, help="Level file (.hsc)")
    ap.add_argument("-w", "--wireframe", action="store_true")
    args = ap.parse_args()
    if not args.level.exists():
        print(f"Error: {args.level} not found")
        return 1
    level = parse_hmap(args.level.read_bytes(), args.level.stem)
    if not level:
        print("Error: Invalid HMAP file")
        return 1
    fh = level.grid_size * level.layer_count
    print(
        f"{level.name}: {level.grid_size}x{fh} ({level.layer_count} layers), {len(level.objects)} objects"
    )
    Viewer(level, args.wireframe).run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
