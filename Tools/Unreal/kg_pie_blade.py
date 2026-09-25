"""In PIE: toggle the player's assassin blade (same as the B dev key), let the draw clip finish, then shoot."""
import unreal
w = unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
unreal.SystemLibrary.execute_console_command(w, "kg.Act blade")
print("KG_BLADE", pawn.get_editor_property("holding_assassin_blade"))
