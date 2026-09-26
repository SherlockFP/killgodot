"""Board tables (SPRINT-036, Docs/Design/Tabletop_Games.md section 2): two AKGBoardTable actors with their stools.

  * inn      - the Latecomer's beer garden, north-east pocket of Fountain Square (chess, Blitz 3+2)
  * square   - the west flank by the Bakery front, looking at the fountain (draughts, Bullet 1+1)

Zone "tabletop" is not in kg_dress.V2_ORDER, so kg_dress.zones() runs it last (after "square" has claimed its
tables and stools); C.find() spirals to the nearest free spot. Registering it explicitly = one entry appended to
V2_ORDER in Tools/Unreal/kg_dress.py: `"tabletop"` (kept out of this file's hands: the v2 builder owns that list).

Each table: one Kitchen_Square_Table mesh on the actor (TableMesh, set in the editor so captures show it), two
KG_Props stools as AKGSeat (C.seat) facing each other across the board; seat_white / seat_black wired so the runtime
does not spawn its own furniture (b_spawn_seats False). The board and the placeholder pieces are built by the actor.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

P, KI = C.P, C.K
FS = C.xy("fountain")
TABLE_MESH = KI + "Square_Table"
SEAT_GAP = 72.0       # stool centre from the table centre (the kitchen table is ~100 wide)
FOOT = 120.0          # claimed radius (table + two stools)

try:
    import unreal
except ImportError:  # dry run
    unreal = None


def m(xm, ym):
    return xm * C.M, ym * C.M


def board_table(x, y, yaw, game, clock, place):
    """AKGBoardTable at (x, y) with White's stool at -X of the table (local), Black's at +X; White looks along yaw."""
    z = C.ground(x, y)
    t = C.spawn_class("/Script/KillGodot.KGBoardTable", x, y, z, yaw, "Tables")
    if t is None:
        return None
    dx, dy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    white = C.seat(P + "Stool", x - dx * SEAT_GAP, y - dy * SEAT_GAP, yaw, z=z)
    black = C.seat(P + "Stool", x + dx * SEAT_GAP, y + dy * SEAT_GAP, yaw + 180.0, z=z)
    if unreal and C.UE:
        try:
            t.set_editor_property("game", getattr(unreal.KGTableGame, game))
            t.set_editor_property("clock_preset", getattr(unreal.KGTableClock, clock))
            t.set_editor_property("place_name", unreal.Text(place))
            t.set_editor_property("b_spawn_seats", False)
            t.set_editor_property("b_spawn_table_mesh", False)
            if white and black:
                t.set_editor_property("seat_white", white)
                t.set_editor_property("seat_black", black)
            mesh = C.mesh(TABLE_MESH)
            if mesh:
                t.get_editor_property("table_mesh").set_static_mesh(mesh)
            t.set_actor_label(f"KGBoardTable_{game}_{place.replace(' ', '')}")
        except Exception as e:
            unreal.log_warning(f"KG_DRESS_V2 tabletop: {e}")
    C.claim(x, y, FOOT)
    C.stats["tables"] = C.stats.get("tables", 0) + 1
    return t


def inn_table():
    """Chess in the beer garden: between the inn's garden tables, White looking at the inn front."""
    inn = C.building("inn")
    for xm, ym in ((14.2, -6.4), (13.2, -8.4), (16.2, -7.4)):
        s = C.find(*m(xm, ym), FOOT, reach=320.0, step=40.0, zone=False)
        if s:
            fx, fy = inn.at("front", inn.door_t, 0.0)
            board_table(s[0], s[1], K.face_dir(s[0], s[1], fx, fy), "CHESS", "BLITZ", "The Latecomer")
            return


def square_table():
    """Draughts on the west flank, a few steps from the Bakery, the board turned towards the fountain."""
    for xm, ym in ((-8.6, 2.4), (-9.4, -1.6), (-7.6, 5.0)):
        s = C.find(*m(xm, ym), FOOT, reach=320.0, step=40.0, zone=False)
        if s:
            board_table(s[0], s[1], K.face_dir(s[0], s[1], FS[0], FS[1]) + 90.0, "DRAUGHTS", "BULLET", "Fountain Square")
            return


def dress():
    K.reset()
    for name, fn in (("inn", inn_table), ("square", square_table)):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
