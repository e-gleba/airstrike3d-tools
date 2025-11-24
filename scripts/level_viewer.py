#!/usr/bin/env python3
"""
level_viewer - 3D Viewer for AirStrike 3D levels

Renders terrain heightmaps and object placements using Pygame + OpenGL.
Heightmap layers are stacked to form the full scrolling level.

Controls:
  Mouse drag     - Rotate camera
  Scroll         - Zoom in/out
  W/S            - Move forward/back
  A/D            - Move left/right
  +/-            - Adjust height scale
  F              - Toggle wireframe
  G              - Toggle grid
  C              - Cycle color modes
  O              - Toggle objects
  R              - Reset view
  ESC            - Quit
"""
import argparse
import math
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

try:
    import pygame
    from pygame.locals import *
    from OpenGL.GL import *
    from OpenGL.GLU import *
except ImportError:
    print("Required: pip install pygame PyOpenGL")
    sys.exit(1)


@dataclass
class LevelData:
    """Parsed level data for rendering."""

    name: str
    grid_size: int
    terrain_scale: int
    layer_count: int
    heightmaps: list[list[list[int]]] = field(default_factory=list)
    combined_heightmap: list[list[int]] = field(default_factory=list)  # Full terrain
    object_types: list[str] = field(default_factory=list)
    item_types: list[str] = field(default_factory=list)
    objects: list[tuple[str, float, float, float, float]] = field(default_factory=list)


def parse_hmap(data: bytes, name: str = "Unknown") -> Optional[LevelData]:
    """Parse HMAP file and extract all data for rendering."""
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

    # Parse object type names (length-prefixed strings)
    offset = 28
    for _ in range(object_type_count):
        if offset >= len(data):
            break
        name_len = data[offset]
        offset += 1
        if offset + name_len > len(data):
            break
        name_str = data[offset : offset + name_len].decode("ascii", errors="replace").rstrip("\x00")
        level.object_types.append(name_str)
        offset += name_len

    # Parse item type names
    for _ in range(item_type_count):
        if offset >= len(data):
            break
        name_len = data[offset]
        offset += 1
        if offset + name_len > len(data):
            break
        name_str = data[offset : offset + name_len].decode("ascii", errors="replace").rstrip("\x00")
        level.item_types.append(name_str)
        offset += name_len

    # Find where object data starts (first 0xFFFF marker)
    first_ffff = len(data)
    for i in range(offset, len(data) - 1):
        if data[i:i + 2] == b"\xff\xff":
            first_ffff = i
            break

    # Calculate actual number of heightmap layers from available data
    layer_size = grid_size * grid_size * 4
    heightmap_bytes = first_ffff - offset
    actual_layers = heightmap_bytes // layer_size
    level.layer_count = actual_layers

    # Parse heightmap layers
    # Each layer is grid_size × grid_size × 4 bytes (uint32)
    # Layers are stacked vertically to form the full level
    for _ in range(actual_layers):
        hmap = []
        for y in range(grid_size):
            row = []
            for x in range(grid_size):
                if offset + 4 <= len(data):
                    h = struct.unpack_from("<I", data, offset)[0]
                    # Values are 0-255 height values
                    h = min(255, h)
                else:
                    h = 128
                row.append(h)
                offset += 4
            hmap.append(row)
        level.heightmaps.append(hmap)

    # Combine all heightmap layers into one long terrain
    # Each layer adds grid_size rows in the Y direction
    for hmap in level.heightmaps:
        for row in hmap:
            level.combined_heightmap.append(row)

    # Parse object placements
    # Objects are delimited by 0xFFFF markers
    # Format: ffff(2) + type_idx(2) + x_grid(2) + y_grid(2) + extra bytes
    ffff_positions = []
    for i in range(offset, len(data) - 1):
        if data[i:i + 2] == b"\xff\xff":
            ffff_positions.append(i)

    # Parse each object record
    scale = terrain_scale / grid_size  # World units per grid cell
    for i, pos in enumerate(ffff_positions):
        if pos + 8 > len(data):
            break

        type_idx = struct.unpack_from("<H", data, pos + 2)[0]
        x_grid = struct.unpack_from("<H", data, pos + 4)[0]
        y_grid = struct.unpack_from("<H", data, pos + 6)[0]

        if type_idx >= len(level.object_types):
            continue

        # Convert grid coordinates to world coordinates
        x_world = x_grid * scale
        y_world = y_grid * scale

        type_name = level.object_types[type_idx]
        level.objects.append((type_name, x_world, y_world, 0.0, 0.0))

    return level


class Camera:
    """Orbit camera controller."""

    def __init__(self):
        self.yaw = 45.0
        self.pitch = 30.0
        self.distance = 50.0
        self.center = [16.0, 16.0, 0.0]
        self.dragging = False
        self.last_pos = (0, 0)

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN:
            if event.button == 1:  # Left click
                self.dragging = True
                self.last_pos = event.pos
            elif event.button == 4:  # Scroll up
                self.distance = max(5.0, self.distance - 3.0)
            elif event.button == 5:  # Scroll down
                self.distance = min(200.0, self.distance + 3.0)
        elif event.type == pygame.MOUSEBUTTONUP:
            if event.button == 1:
                self.dragging = False
        elif event.type == pygame.MOUSEMOTION:
            if self.dragging:
                dx = event.pos[0] - self.last_pos[0]
                dy = event.pos[1] - self.last_pos[1]
                self.yaw += dx * 0.5
                self.pitch = max(-89, min(89, self.pitch + dy * 0.5))
                self.last_pos = event.pos

    def handle_keys(self, keys):
        speed = 0.5
        rad = math.radians(self.yaw)
        if keys[K_w]:
            self.center[0] -= math.sin(rad) * speed
            self.center[1] += math.cos(rad) * speed
        if keys[K_s]:
            self.center[0] += math.sin(rad) * speed
            self.center[1] -= math.cos(rad) * speed
        if keys[K_a]:
            self.center[0] -= math.cos(rad) * speed
            self.center[1] -= math.sin(rad) * speed
        if keys[K_d]:
            self.center[0] += math.cos(rad) * speed
            self.center[1] += math.sin(rad) * speed

    def apply(self):
        glLoadIdentity()
        yaw_rad = math.radians(self.yaw)
        pitch_rad = math.radians(self.pitch)
        
        eye_x = self.center[0] + self.distance * math.sin(yaw_rad) * math.cos(pitch_rad)
        eye_y = self.center[1] - self.distance * math.cos(yaw_rad) * math.cos(pitch_rad)
        eye_z = self.center[2] + self.distance * math.sin(pitch_rad)
        
        gluLookAt(
            eye_x, eye_y, eye_z,
            self.center[0], self.center[1], self.center[2],
            0, 0, 1
        )


class TerrainRenderer:
    """Renders terrain heightmap as 3D mesh with display list optimization."""

    COLOR_MODES = ["height", "gradient", "contour", "terrain"]

    def __init__(self, level: LevelData):
        self.level = level
        self.height_scale = 0.1
        self.wireframe = False
        self.show_grid = True
        self.show_objects = True
        self.color_mode = 0
        # Full terrain dimensions
        self.width = level.grid_size
        self.height = len(level.combined_heightmap)  # grid_size * layer_count
        # Display lists for fast rendering
        self._terrain_list = None
        self._grid_list = None
        self._objects_list = None
        self._last_height_scale = None
        self._last_color_mode = None
        self._build_mesh()

    def _build_mesh(self):
        """Pre-calculate mesh data and create display lists."""
        self._rebuild_terrain()
        self._rebuild_grid()
        self._rebuild_objects()

    def _rebuild_terrain(self):
        """Build terrain display list."""
        if self._terrain_list:
            glDeleteLists(self._terrain_list, 1)
        
        self._terrain_list = glGenLists(1)
        glNewList(self._terrain_list, GL_COMPILE)
        self._render_terrain_immediate()
        glEndList()
        self._last_height_scale = self.height_scale
        self._last_color_mode = self.color_mode

    def _rebuild_grid(self):
        """Build grid display list."""
        if self._grid_list:
            glDeleteLists(self._grid_list, 1)
        
        hmap = self.level.combined_heightmap
        width = self.width
        height = self.height
        
        self._grid_list = glGenLists(1)
        glNewList(self._grid_list, GL_COMPILE)
        glColor3f(0.3, 0.3, 0.3)
        glBegin(GL_LINES)
        for i in range(0, width + 1, 8):
            glVertex3f(i, 0, 0.2)
            glVertex3f(i, height - 1, 0.2)
        for i in range(0, height + 1, self.level.grid_size):
            glVertex3f(0, i, 0.2)
            glVertex3f(width - 1, i, 0.2)
        glEnd()
        glEndList()

    def _rebuild_objects(self):
        """Build objects display list."""
        if self._objects_list:
            glDeleteLists(self._objects_list, 1)
        
        self._objects_list = glGenLists(1)
        glNewList(self._objects_list, GL_COMPILE)
        self._render_objects_immediate()
        glEndList()

    def _get_height(self, h: int) -> float:
        """Convert raw height value to rendered height. 255 = road (flat)."""
        if h >= 250:  # Road - render flat at low level
            return 8.0 * self.height_scale
        return h * self.height_scale

    def _get_color(self, h: int, x: int, y: int) -> tuple[float, float, float]:
        """Get color for height value based on current mode."""
        # 255 = road, render as dirt/gravel
        if h >= 250:
            return (0.55, 0.45, 0.35)  # Dirt road color

        t = h / 249.0  # Normalize to 0-249 range (not 255)

        if self.color_mode == 0:  # Natural terrain colors
            if t < 0.25:  # Low grass - bright green
                return (0.35, 0.55 + t * 0.3, 0.2)
            elif t < 0.5:  # Medium grass/shrubs - darker green
                tt = (t - 0.25) / 0.25
                return (0.3 + tt * 0.15, 0.5 - tt * 0.1, 0.15 + tt * 0.1)
            elif t < 0.75:  # Hills - brown/tan
                tt = (t - 0.5) / 0.25
                return (0.5 + tt * 0.15, 0.4 + tt * 0.05, 0.25 + tt * 0.1)
            else:  # Mountains - rocky gray
                tt = (t - 0.75) / 0.25
                return (0.6 + tt * 0.25, 0.55 + tt * 0.25, 0.5 + tt * 0.3)
        elif self.color_mode == 1:  # Height gradient
            return (0.2 + t * 0.6, 0.6 - t * 0.3, 0.2)
        elif self.color_mode == 2:  # Contour lines
            band = int(h / 15) % 2
            base_g = 0.4 + t * 0.3
            if band:
                return (0.3, base_g + 0.1, 0.2)
            else:
                return (0.25, base_g, 0.15)
        else:  # Satellite-style
            if t < 0.4:
                return (0.3 + t * 0.2, 0.45 + t * 0.2, 0.2)
            else:
                tt = (t - 0.4) / 0.6
                return (0.4 + tt * 0.35, 0.5 - tt * 0.1, 0.25 + tt * 0.2)

    def _render_terrain_immediate(self):
        """Render terrain using immediate mode (for display list)."""
        hmap = self.level.combined_heightmap
        width = self.width
        height = self.height

        # Use triangle strips for better performance
        for y in range(height - 1):
            glBegin(GL_TRIANGLE_STRIP)
            for x in range(width):
                h0 = hmap[y][x]
                h1 = hmap[y + 1][x]
                
                z0 = self._get_height(h0)
                z1 = self._get_height(h1)
                
                c0 = self._get_color(h0, x, y)
                c1 = self._get_color(h1, x, y + 1)
                
                glColor3f(*c0)
                glVertex3f(x, y, z0)
                glColor3f(*c1)
                glVertex3f(x, y + 1, z1)
            glEnd()

    def render(self):
        """Render terrain using display lists for performance."""
        if not self.level.combined_heightmap:
            return

        # Rebuild if settings changed
        if (self._last_height_scale != self.height_scale or 
            self._last_color_mode != self.color_mode):
            self._rebuild_terrain()
            self._rebuild_objects()

        if self.wireframe:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
        else:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)

        # Render terrain from display list
        if self._terrain_list:
            glCallList(self._terrain_list)

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)

        # Render grid
        if self.show_grid and self._grid_list:
            glCallList(self._grid_list)

        # Render objects
        if self.show_objects and self._objects_list:
            glCallList(self._objects_list)

    def _render_objects_immediate(self):
        """Render object markers (for display list)."""
        hmap = self.level.combined_heightmap
        width = self.width
        height = self.height
        scale = self.level.terrain_scale / self.level.grid_size  # World units per grid cell

        for obj_name, x_world, y_world, z, rot in self.level.objects:
            # Convert world coords to grid coords for rendering
            gx = x_world / scale
            gy = y_world / scale

            # Get terrain height at object position from combined heightmap
            gz = 0.0
            ix, iy = int(gx), int(gy)
            if hmap and 0 <= ix < width and 0 <= iy < height:
                gz = hmap[iy][ix] * self.height_scale

            # Color by object type
            name_lower = obj_name.lower()
            if "helic" in name_lower or "tank" in name_lower or "turret" in name_lower or "btr" in name_lower:
                color = (1.0, 0.2, 0.2)  # Red - enemies
            elif "item" in name_lower:
                color = (1.0, 1.0, 0.2)  # Yellow - items
            elif "tree" in name_lower or "grass" in name_lower or "kust" in name_lower or "kamni" in name_lower:
                color = (0.2, 0.8, 0.2)  # Green - nature
            elif "kolhoz" in name_lower or "dom" in name_lower or "angar" in name_lower or "zabor" in name_lower:
                color = (0.8, 0.5, 0.2)  # Orange - buildings
            elif "jeep" in name_lower or "uaz" in name_lower or "gruzovik" in name_lower:
                color = (0.9, 0.6, 0.1)  # Yellow-orange - vehicles
            else:
                color = (0.6, 0.6, 0.6)  # Gray - misc

            # Draw marker
            glColor3f(*color)
            glPushMatrix()
            glTranslatef(gx, gy, gz + 0.3)
            
            # Small cube marker
            s = 0.25
            glBegin(GL_QUADS)
            # Top
            glVertex3f(-s, -s, s)
            glVertex3f(s, -s, s)
            glVertex3f(s, s, s)
            glVertex3f(-s, s, s)
            # Bottom
            glVertex3f(-s, -s, -s)
            glVertex3f(s, -s, -s)
            glVertex3f(s, s, -s)
            glVertex3f(-s, s, -s)
            # Front
            glVertex3f(-s, -s, -s)
            glVertex3f(s, -s, -s)
            glVertex3f(s, -s, s)
            glVertex3f(-s, -s, s)
            # Back
            glVertex3f(-s, s, -s)
            glVertex3f(s, s, -s)
            glVertex3f(s, s, s)
            glVertex3f(-s, s, s)
            # Left
            glVertex3f(-s, -s, -s)
            glVertex3f(-s, -s, s)
            glVertex3f(-s, s, s)
            glVertex3f(-s, s, -s)
            # Right
            glVertex3f(s, -s, -s)
            glVertex3f(s, -s, s)
            glVertex3f(s, s, s)
            glVertex3f(s, s, -s)
            glEnd()
            glPopMatrix()


def main():
    parser = argparse.ArgumentParser(description="3D Level Viewer for AirStrike 3D")
    parser.add_argument("level", type=Path, help="Level file (.hsc)")
    parser.add_argument("-w", "--wireframe", action="store_true", help="Start in wireframe mode")
    args = parser.parse_args()

    if not args.level.exists():
        print(f"Error: File not found: {args.level}")
        return 1

    # Parse level
    data = args.level.read_bytes()
    level = parse_hmap(data, args.level.stem)
    if not level:
        print("Error: Invalid HMAP file")
        return 1

    print(f"Level: {level.name}")
    full_height = level.grid_size * level.layer_count
    print(f"Terrain: {level.grid_size}x{full_height} (from {level.layer_count} layers)")
    if level.combined_heightmap:
        heights = [h for row in level.combined_heightmap for h in row]
        print(f"Heights: {min(heights)}-{max(heights)} ({len(set(heights))} unique)")
    print(f"Objects: {len(level.objects)} placed")

    # Initialize Pygame
    pygame.init()
    display = (1024, 768)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    pygame.display.set_caption(f"Level Viewer - {level.name}")

    # OpenGL setup
    glEnable(GL_DEPTH_TEST)
    glClearColor(0.4, 0.6, 0.85, 1.0)  # Sky blue background
    glMatrixMode(GL_PROJECTION)
    gluPerspective(60, display[0] / display[1], 0.1, 500.0)
    glMatrixMode(GL_MODELVIEW)
    
    # Enable fog for depth effect
    glEnable(GL_FOG)
    glFogi(GL_FOG_MODE, GL_LINEAR)
    glFogfv(GL_FOG_COLOR, (0.5, 0.6, 0.75, 1.0))
    glFogf(GL_FOG_START, 80.0)
    glFogf(GL_FOG_END, 250.0)

    camera = Camera()
    # Center camera on the full terrain (which is grid_size × (grid_size * layer_count))
    full_height = level.grid_size * level.layer_count
    camera.center = [level.grid_size / 2, full_height / 2, 0]
    camera.distance = max(level.grid_size, full_height) * 0.8

    renderer = TerrainRenderer(level)
    if args.wireframe:
        renderer.wireframe = True

    clock = pygame.time.Clock()
    running = True
    show_help = True
    fog_enabled = True
    font = pygame.font.SysFont("monospace", 14)
    font_big = pygame.font.SysFont("monospace", 16, bold=True)

    # Help text
    help_lines = [
        "CONTROLS:",
        "Mouse      - Rotate view",
        "Scroll     - Zoom in/out",
        "W/S/A/D    - Move camera",
        "+/-        - Height scale",
        "F          - Wireframe",
        "G          - Grid lines",
        "O          - Object markers",
        "C          - Color mode",
        "V          - Fog/haze",
        "H          - This help",
        "R          - Reset view",
        "ESC        - Quit",
    ]

    while running:
        for event in pygame.event.get():
            if event.type == QUIT:
                running = False
            elif event.type == KEYDOWN:
                if event.key == K_ESCAPE:
                    running = False
                elif event.key == K_f:
                    renderer.wireframe = not renderer.wireframe
                elif event.key == K_g:
                    renderer.show_grid = not renderer.show_grid
                elif event.key == K_o:
                    renderer.show_objects = not renderer.show_objects
                elif event.key == K_c:
                    renderer.color_mode = (renderer.color_mode + 1) % len(TerrainRenderer.COLOR_MODES)
                elif event.key == K_h:
                    show_help = not show_help
                elif event.key == K_v:
                    fog_enabled = not fog_enabled
                    if fog_enabled:
                        glEnable(GL_FOG)
                    else:
                        glDisable(GL_FOG)
                elif event.key == K_PLUS or event.key == K_EQUALS:
                    renderer.height_scale *= 1.2
                elif event.key == K_MINUS:
                    renderer.height_scale /= 1.2
                elif event.key == K_r:
                    camera = Camera()
                    full_height = level.grid_size * level.layer_count
                    camera.center = [level.grid_size / 2, full_height / 2, 0]
                    camera.distance = max(level.grid_size, full_height) * 0.8
                    renderer.height_scale = 0.1
            camera.handle_event(event)

        keys = pygame.key.get_pressed()
        camera.handle_keys(keys)

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        camera.apply()
        renderer.render()

        # Render 2D HUD overlay using texture
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        glOrtho(0, display[0], display[1], 0, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()
        glDisable(GL_DEPTH_TEST)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glEnable(GL_TEXTURE_2D)

        def draw_text(text, x, y, is_title=False, cache={}):
            """Draw text at x,y using cached OpenGL textures."""
            cache_key = (text, is_title)
            
            if cache_key not in cache:
                if is_title:
                    text_surface = font_big.render(text, True, (255, 255, 100), (20, 20, 40))
                else:
                    text_surface = font.render(text, True, (220, 220, 220), (20, 20, 40))
                
                w, h = text_surface.get_size()
                text_surface = pygame.transform.flip(text_surface, False, True)
                text_data = pygame.image.tostring(text_surface, "RGBA", True)
                
                tex_id = glGenTextures(1)
                glBindTexture(GL_TEXTURE_2D, tex_id)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, text_data)
                
                cache[cache_key] = (tex_id, w, h)
            
            tex_id, w, h = cache[cache_key]
            glBindTexture(GL_TEXTURE_2D, tex_id)
            glColor4f(1, 1, 1, 1)
            glBegin(GL_QUADS)
            glTexCoord2f(0, 0); glVertex2f(x, y)
            glTexCoord2f(1, 0); glVertex2f(x + w, y)
            glTexCoord2f(1, 1); glVertex2f(x + w, y + h)
            glTexCoord2f(0, 1); glVertex2f(x, y + h)
            glEnd()
            return h

        # Draw help text
        if show_help:
            y_pos = 10
            for i, line in enumerate(help_lines):
                h = draw_text(line, 10, y_pos, is_title=(i == 0))
                y_pos += h + 2

        # Draw status line (not cached - changes)
        glDisable(GL_TEXTURE_2D)
        status = f"{level.name} | {renderer.width}x{renderer.height} | {len(level.objects)} objs"
        status_surface = font.render(status, True, (150, 255, 150), (20, 20, 40))
        status_surface = pygame.transform.flip(status_surface, False, True)
        w, h = status_surface.get_size()
        
        glEnable(GL_TEXTURE_2D)
        tex_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, tex_id)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        text_data = pygame.image.tostring(status_surface, "RGBA", True)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, text_data)
        
        glColor4f(1, 1, 1, 1)
        glBegin(GL_QUADS)
        glTexCoord2f(0, 0); glVertex2f(10, display[1] - 25)
        glTexCoord2f(1, 0); glVertex2f(10 + w, display[1] - 25)
        glTexCoord2f(1, 1); glVertex2f(10 + w, display[1] - 25 + h)
        glTexCoord2f(0, 1); glVertex2f(10, display[1] - 25 + h)
        glEnd()
        glDeleteTextures([tex_id])

        glDisable(GL_TEXTURE_2D)
        glDisable(GL_BLEND)
        glEnable(GL_DEPTH_TEST)
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        glPopMatrix()

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
