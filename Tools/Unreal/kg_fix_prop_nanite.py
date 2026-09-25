"""Disable Nanite on small first-person props (QA-001 P1: 'missing usage flag Nanite' on the pan materials)."""
import unreal
eal = unreal.EditorAssetLibrary
for path in eal.list_assets("/Game/KillGodot/Items", recursive=True):
    asset = unreal.load_asset(path)
    if isinstance(asset, unreal.StaticMesh):
        settings = asset.get_editor_property("nanite_settings")
        if settings.get_editor_property("enabled"):
            settings.set_editor_property("enabled", False)
            asset.set_editor_property("nanite_settings", settings)
            eal.save_loaded_asset(asset)
            unreal.log(f"KG_NANITE off: {path}")
