#!/usr/bin/env python3
"""
level_viewer - AirStrike 3D level 3D viewer

Interactive 3D visualization of game levels using pygame + PyOpenGL.
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
except ImportError:
    print("pygame not found. Install with: pip install pygame")
    sys.exit(1)

try:
    from OpenGL.GL import *
    from OpenGL.GLU import *
except ImportError:
    print("PyOpenGL not found. Install with: pip install PyOpenGL PyOpenGL_accelerate")
    sys.exit(1)

__version__ = "1.0.0"

# HMAP Format
HMAP_MAGIC = b"HMAP"


@dataclass
class HMapLevel:
    """Parsed HMAP level data."""

    grid_size: int = 32
    terrain_scale: int = 256
    layer_count: int = 7
    object_type_count: int = 0
    object_count: int = 0
    object_types: list[str] = field(default_factory=list)
    heightmaps: list[list[list[int]]] = field(default_factory=list)
    name: str = "Unknown"


def parse_hmap(data: bytes, name: str = "Unknown") -> Optional[HMapLevel]:
    """Parse HMAP level file."""
    if len(data) < 28 or data[:4] != HMAP_MAGIC:
        return None

    (
        version,
        grid_size,
        terrain_scale,
        object_count,
        object_type_count,
        layer_count,
    ) = struct.unpack_from("<6I", data, 4)

    level = HMapLevel(
        grid_size=grid_size,
        terrain_scale=terrain_scale,
        layer_count=layer_count,
        object_type_count=object_type_count,
        object_count=object_count,
        name=name,
    )

    # Parse object type strings - keep parsing until we hit heightmap data
    # The heightmap starts when we see consistent uint32 values in 0-255 range
    offset = 28
    while offset < len(data) - 4:
        name_len = data[offset]
        # Check if this looks like a valid string length (1-64 chars)
        if name_len == 0 or name_len > 64:
            break
        # Check if next bytes look like ASCII string data
        if offset + 1 + name_len > len(data):
            break
        # Verify it's printable ASCII
        chunk = data[offset + 1 : offset + 1 + name_len]
        if not all(32 <= b < 127 or b == 0 for b in chunk):
            break
        offset += 1
        obj_name = chunk.decode("ascii", errors="replace")
        level.object_types.append(obj_name.rstrip("\x00"))
        offset += name_len

    level.object_type_count = len(level.object_types)

    # Parse heightmap layers - heights are uint32 with values 0-255
    for layer in range(layer_count):
        hmap = []
        for y in range(grid_size):
            row = []
            for x in range(grid_size):
                if offset + 4 > len(data):
                    h = 128
                else:
                    h = struct.unpack_from("<I", data, offset)[0]
                    h = min(255, h)  # Clamp to byte range
                row.append(h)
                offset += 4
            hmap.append(row)
        level.heightmaps.append(hmap)

    return level


class Camera:
    """Simple orbit camera."""

    def __init__(self):
        self.distance = 60.0
        self.rot_x = 35.0  # Pitch (angle down from horizontal)
        self.rot_y = 45.0  # Yaw (rotation around Y axis)
        self.target = [16.0, 10.0, 16.0]  # Look at center, raised a bit
        self.move_speed = 0.5
        self.rotate_speed = 0.3
        self.zoom_speed = 3.0

    def apply(self):
        """Apply camera transformation."""
        glLoadIdentity()

        # Position camera
        eye_x = self.target[0] + self.distance * math.sin(math.radians(self.rot_y)) * math.cos(math.radians(self.rot_x))
        eye_y = self.target[1] + self.distance * math.sin(math.radians(self.rot_x))
        eye_z = self.target[2] + self.distance * math.cos(math.radians(self.rot_y)) * math.cos(math.radians(self.rot_x))

        gluLookAt(
            eye_x, eye_y, eye_z,
            self.target[0], self.target[1], self.target[2],
            0.0, 1.0, 0.0
        )


class LevelViewer:
    """3D Level viewer using pygame + OpenGL."""

    def __init__(self, level: HMapLevel, width: int = 1280, height: int = 720):
        self.level = level
        self.width = width
        self.height = height
        self.camera = Camera()
        self.running = True
        self.wireframe = False
        self.show_grid = True
        self.current_layer = 0
        self.height_scale = 0.15  # Scale heights for better visualization
        self.terrain_color_mode = 0  # 0=height, 1=gradient, 2=flat

        # Mouse state
        self.mouse_drag = False
        self.last_mouse = (0, 0)

        # Initialize pygame and OpenGL
        pygame.init()
        pygame.display.set_caption(f"AirStrike 3D Level Viewer - {level.name}")
        self.screen = pygame.display.set_mode(
            (width, height), DOUBLEBUF | OPENGL | RESIZABLE
        )
        self.clock = pygame.time.Clock()

        self._init_gl()
        self._build_terrain_display_list()

    def _init_gl(self):
        """Initialize OpenGL settings."""
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glEnable(GL_COLOR_MATERIAL)
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)

        # Light setup
        glLightfv(GL_LIGHT0, GL_POSITION, (1.0, 1.0, 1.0, 0.0))
        glLightfv(GL_LIGHT0, GL_AMBIENT, (0.3, 0.3, 0.3, 1.0))
        glLightfv(GL_LIGHT0, GL_DIFFUSE, (0.8, 0.8, 0.8, 1.0))

        # Background color - sky blue
        glClearColor(0.4, 0.6, 0.9, 1.0)

        self._setup_projection()

    def _setup_projection(self):
        """Setup perspective projection."""
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(60.0, self.width / self.height, 0.1, 500.0)
        glMatrixMode(GL_MODELVIEW)

    def _height_to_color(self, h: int) -> tuple:
        """Convert height value to RGB color."""
        if h >= 255:  # Water marker
            return (0.2, 0.4, 0.8)  # Blue for water

        t = h / 255.0

        if self.terrain_color_mode == 0:
            # Height-based gradient: green -> brown -> white
            if t < 0.3:
                return (0.2, 0.5 + t, 0.2)  # Green lowlands
            elif t < 0.6:
                r = 0.4 + (t - 0.3) * 1.5
                g = 0.5 - (t - 0.3) * 0.5
                return (r, g, 0.2)  # Brown hills
            else:
                gray = 0.6 + (t - 0.6) * 0.8
                return (gray, gray, gray)  # Gray/white peaks
        elif self.terrain_color_mode == 1:
            # Simple gradient
            return (t, 1.0 - t * 0.5, 0.3)
        else:
            # Flat green
            return (0.3, 0.6, 0.3)

    def _build_terrain_display_list(self):
        """Build OpenGL display list for terrain."""
        self.terrain_list = glGenLists(1)
        glNewList(self.terrain_list, GL_COMPILE)

        if not self.level.heightmaps:
            glEndList()
            return

        hmap = self.level.heightmaps[self.current_layer]
        grid = self.level.grid_size

        # Draw terrain as triangle strips
        for z in range(grid - 1):
            glBegin(GL_TRIANGLE_STRIP)
            for x in range(grid):
                for dz in [0, 1]:
                    h = hmap[z + dz][x]
                    y = h * self.height_scale

                    # Color based on height
                    color = self._height_to_color(h)
                    glColor3f(*color)

                    # Normal calculation (simple approximation)
                    nx, ny, nz = 0.0, 1.0, 0.0
                    if x > 0 and x < grid - 1 and z + dz > 0 and z + dz < grid - 1:
                        dx = (hmap[z + dz][x + 1] - hmap[z + dz][x - 1]) * self.height_scale
                        dz_val = (hmap[z + dz + 1][x] - hmap[z + dz - 1][x]) * self.height_scale if z + dz + 1 < grid and z + dz - 1 >= 0 else 0
                        length = math.sqrt(dx * dx + 4.0 + dz_val * dz_val)
                        nx, ny, nz = -dx / length, 2.0 / length, -dz_val / length

                    glNormal3f(nx, ny, nz)
                    glVertex3f(float(x), y, float(z + dz))
            glEnd()

        glEndList()

    def _draw_grid(self):
        """Draw reference grid."""
        glDisable(GL_LIGHTING)
        glColor3f(0.5, 0.5, 0.5)
        glLineWidth(1.0)

        grid = self.level.grid_size

        glBegin(GL_LINES)
        for i in range(0, grid + 1, 4):
            glVertex3f(float(i), 0, 0)
            glVertex3f(float(i), 0, float(grid))
            glVertex3f(0, 0, float(i))
            glVertex3f(float(grid), 0, float(i))
        glEnd()

        glEnable(GL_LIGHTING)

    def _draw_axes(self):
        """Draw coordinate axes."""
        glDisable(GL_LIGHTING)
        glLineWidth(2.0)

        glBegin(GL_LINES)
        # X axis - red
        glColor3f(1.0, 0.0, 0.0)
        glVertex3f(0, 0, 0)
        glVertex3f(5, 0, 0)
        # Y axis - green
        glColor3f(0.0, 1.0, 0.0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 5, 0)
        # Z axis - blue
        glColor3f(0.0, 0.0, 1.0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 0, 5)
        glEnd()

        glEnable(GL_LIGHTING)

    def _handle_events(self):
        """Handle pygame events."""
        for event in pygame.event.get():
            if event.type == QUIT:
                self.running = False

            elif event.type == KEYDOWN:
                if event.key == K_ESCAPE:
                    self.running = False
                elif event.key == K_g:
                    self.show_grid = not self.show_grid
                elif event.key == K_f:
                    self.wireframe = not self.wireframe
                elif event.key == K_c:
                    self.terrain_color_mode = (self.terrain_color_mode + 1) % 3
                    self._build_terrain_display_list()
                elif event.key == K_r:
                    self.camera = Camera()
                    self.camera.target = [self.level.grid_size / 2, 0, self.level.grid_size / 2]
                elif event.key == K_q:
                    self.current_layer = max(0, self.current_layer - 1)
                    self._build_terrain_display_list()
                elif event.key == K_e:
                    self.current_layer = min(self.level.layer_count - 1, self.current_layer + 1)
                    self._build_terrain_display_list()
                elif event.key == K_PLUS or event.key == K_EQUALS:
                    self.height_scale *= 1.2
                    self._build_terrain_display_list()
                elif event.key == K_MINUS:
                    self.height_scale /= 1.2
                    self._build_terrain_display_list()

            elif event.type == MOUSEBUTTONDOWN:
                if event.button == 1:  # Left click
                    self.mouse_drag = True
                    self.last_mouse = event.pos
                elif event.button == 4:  # Scroll up
                    self.camera.distance = max(5, self.camera.distance - self.camera.zoom_speed)
                elif event.button == 5:  # Scroll down
                    self.camera.distance = min(200, self.camera.distance + self.camera.zoom_speed)

            elif event.type == MOUSEBUTTONUP:
                if event.button == 1:
                    self.mouse_drag = False

            elif event.type == MOUSEMOTION:
                if self.mouse_drag:
                    dx = event.pos[0] - self.last_mouse[0]
                    dy = event.pos[1] - self.last_mouse[1]
                    self.camera.rot_y += dx * self.camera.rotate_speed
                    self.camera.rot_x += dy * self.camera.rotate_speed
                    self.camera.rot_x = max(-89, min(89, self.camera.rot_x))
                    self.last_mouse = event.pos

            elif event.type == VIDEORESIZE:
                self.width, self.height = event.size
                self.screen = pygame.display.set_mode(
                    (self.width, self.height), DOUBLEBUF | OPENGL | RESIZABLE
                )
                glViewport(0, 0, self.width, self.height)
                self._setup_projection()

        # Continuous key handling
        keys = pygame.key.get_pressed()
        move_x, move_z = 0, 0
        if keys[K_w]:
            move_z -= self.camera.move_speed
        if keys[K_s]:
            move_z += self.camera.move_speed
        if keys[K_a]:
            move_x -= self.camera.move_speed
        if keys[K_d]:
            move_x += self.camera.move_speed

        if move_x or move_z:
            # Rotate movement by camera yaw
            rad = math.radians(self.camera.rot_y)
            self.camera.target[0] += move_x * math.cos(rad) + move_z * math.sin(rad)
            self.camera.target[2] += -move_x * math.sin(rad) + move_z * math.cos(rad)

    def render(self):
        """Render one frame."""
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        self.camera.apply()

        # Wireframe mode
        if self.wireframe:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
        else:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)

        # Draw terrain
        glCallList(self.terrain_list)

        # Draw grid
        if self.show_grid:
            self._draw_grid()

        # Draw axes
        self._draw_axes()

        pygame.display.flip()

    def run(self):
        """Main loop."""
        # Center camera on terrain
        self.camera.target = [self.level.grid_size / 2, 0, self.level.grid_size / 2]

        print("\n" + "=" * 50)
        print("AirStrike 3D Level Viewer")
        print("=" * 50)
        print(f"Level: {self.level.name}")
        print(f"Grid: {self.level.grid_size}x{self.level.grid_size}")
        print(f"Layers: {self.level.layer_count}")
        print(f"Objects: {self.level.object_count}")
        print("\nControls:")
        print("  Mouse drag - Rotate camera")
        print("  Scroll     - Zoom in/out")
        print("  WASD       - Move camera")
        print("  Q/E        - Change terrain layer")
        print("  +/-        - Adjust height scale")
        print("  G          - Toggle grid")
        print("  F          - Toggle wireframe")
        print("  C          - Change color mode")
        print("  R          - Reset camera")
        print("  ESC        - Quit")
        print("=" * 50 + "\n")

        while self.running:
            self._handle_events()
            self.render()
            self.clock.tick(60)

        pygame.quit()


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="AirStrike 3D level 3D viewer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  %(prog)s level1_tutor.hsc           # View level file directly
  %(prog)s level.hsc --wireframe      # Start in wireframe mode
        """,
    )
    parser.add_argument("level", type=Path, help="Level file (.hsc)")
    parser.add_argument("--wireframe", "-w", action="store_true", help="Start in wireframe mode")
    parser.add_argument("--width", type=int, default=1280, help="Window width")
    parser.add_argument("--height", type=int, default=720, help="Window height")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")

    if len(sys.argv) == 1:
        parser.print_help()
        return 1

    args = parser.parse_args()

    if not args.level.exists():
        print(f"Error: Level file not found: {args.level}")
        return 1

    # Load level
    data = args.level.read_bytes()
    level = parse_hmap(data, args.level.stem)

    if not level:
        print(f"Error: Failed to parse level file: {args.level}")
        return 1

    # Create and run viewer
    try:
        viewer = LevelViewer(level, args.width, args.height)
        if args.wireframe:
            viewer.wireframe = True
        viewer.run()
    except Exception as e:
        print(f"Error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

