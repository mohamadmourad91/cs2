# -*- coding: utf-8 -*-
"""
Umayyad Square (ساحة الأمويين) - Damascus : procedural level builder for Unreal Engine 5.5+

Run inside the editor:  Tools > Execute Python Script...  -> pick this file
(or in the Output Log Python console:  py "Content/Python/build_umayyad_square.py")

It creates /Game/Maps/UmayyadSquare with:
  * Real-time lighting stack: Sky Atmosphere, Volumetric Clouds, Lumen, golden-hour sun, height fog
  * The Damascene Sword monument (mid), fountain rings and the roundabout island
  * Opera House (Site A), Radio & TV building (Site B), General Staff compound, hotel block
  * Radial avenues, sidewalks, date palms, cypresses, lamp posts, buses, cars, barriers (cover)
  * Mount Qasioun backdrop with city lights, invisible map boundary
  * Gameplay actors from the C++ module: PlayerStarts (tagged), BombSites A/B, BuyZones

Axes: +X = North (towards Qasioun), +Y = East (towards the old city), units = cm.
The layout is a gameplay-oriented interpretation of the real square; tune positions against
satellite imagery (approx. 33.5130 N, 36.2765 E) during the art pass.
"""
import math
import zlib
import unreal

MAP_PATH = "/Game/Maps/UmayyadSquare"
MAT_DIR = "/Game/Umayyad/Materials"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

CUBE = "/Engine/BasicShapes/Cube.Cube"          # 100 x 100 x 100, pivot centre
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"   # d=100, h=100, pivot centre
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CONE = "/Engine/BasicShapes/Cone.Cone"
PLANE = "/Engine/BasicShapes/Plane.Plane"

_mesh_cache = {}


def h(text):
    """Stable pseudo-random int (Python's h() is salted per process)."""
    return zlib.crc32(text.encode("utf-8"))


def mkrot(pitch=0.0, yaw=0.0, roll=0.0):
    # NB: unreal.Rotator's positional order is (roll, pitch, yaw) - always use keywords
    return unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll)

_mat_cache = {}


def mesh(path):
    if path not in _mesh_cache:
        _mesh_cache[path] = unreal.load_asset(path)
    return _mesh_cache[path]


# --------------------------------------------------------------------------- materials
def ensure_master_material():
    path = MAT_DIR + "/M_UmayyadMaster"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = asset_tools.create_asset("M_UmayyadMaster", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def param(cls, name, x, default):
        node = mel.create_material_expression(m, cls, x, 0)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    base = param(unreal.MaterialExpressionVectorParameter, "BaseColor", -600, unreal.LinearColor(0.8, 0.8, 0.8, 1))
    rough = param(unreal.MaterialExpressionScalarParameter, "Roughness", -600, 0.7)
    metal = param(unreal.MaterialExpressionScalarParameter, "Metallic", -600, 0.0)
    emis = param(unreal.MaterialExpressionVectorParameter, "EmissiveColor", -600, unreal.LinearColor(0, 0, 0, 1))
    emis_k = param(unreal.MaterialExpressionScalarParameter, "EmissiveStrength", -600, 0.0)

    # Subtle large-scale variation so blockout surfaces don't read as flat CG
    noise = mel.create_material_expression(m, unreal.MaterialExpressionNoise, -900, 200)
    noise.set_editor_property("scale", 0.002)
    noise.set_editor_property("output_min", 0.85)
    noise.set_editor_property("output_max", 1.05)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1100, 200)
    mel.connect_material_expressions(wp, "", noise, "Position")
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 0)
    mel.connect_material_expressions(base, "", mul, "A")
    mel.connect_material_expressions(noise, "", mul, "B")

    emul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 300)
    mel.connect_material_expressions(emis, "", emul, "A")
    mel.connect_material_expressions(emis_k, "", emul, "B")

    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def ensure_translucent_master():
    path = MAT_DIR + "/M_UmayyadGlass"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = asset_tools.create_asset("M_UmayyadGlass", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception:
        pass
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "BaseColor")
    col.set_editor_property("default_value", unreal.LinearColor(0.1, 0.3, 0.35, 1))
    op = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 200)
    op.set_editor_property("parameter_name", "Opacity")
    op.set_editor_property("default_value", 0.35)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 300)
    r.set_editor_property("r", 0.03)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


# name: (base colour, roughness, metallic, emissive colour, emissive strength)
PALETTE = {
    "Limestone":   ((0.78, 0.70, 0.57), 0.75, 0.0, None, 0),   # Damascene beige limestone
    "Basalt":      ((0.07, 0.07, 0.075), 0.65, 0.0, None, 0),   # Hauran black basalt (ablaq)
    "Marble":      ((0.92, 0.90, 0.86), 0.25, 0.0, None, 0),
    "Travertine":  ((0.70, 0.60, 0.46), 0.8, 0.0, None, 0),
    "Asphalt":     ((0.045, 0.045, 0.05), 0.85, 0.0, None, 0),
    "LaneWhite":   ((0.85, 0.85, 0.82), 0.6, 0.0, None, 0),
    "Sidewalk":    ((0.55, 0.52, 0.48), 0.8, 0.0, None, 0),
    "Grass":       ((0.10, 0.22, 0.06), 0.9, 0.0, None, 0),
    "Water":       ((0.02, 0.07, 0.09), 0.02, 0.0, None, 0),
    "Bronze":      ((0.55, 0.36, 0.18), 0.35, 1.0, None, 0),
    "Steel":       ((0.62, 0.63, 0.65), 0.3, 1.0, None, 0),
    "Gold":        ((1.0, 0.77, 0.34), 0.2, 1.0, None, 0),
    "Concrete":    ((0.50, 0.49, 0.46), 0.85, 0.0, None, 0),
    "Mosaic":      ((0.12, 0.42, 0.33), 0.35, 0.0, None, 0),     # Umayyad mosque mosaic green
    "PalmTrunk":   ((0.28, 0.20, 0.12), 0.9, 0.0, None, 0),
    "PalmLeaf":    ((0.14, 0.26, 0.08), 0.7, 0.0, None, 0),
    "Cypress":     ((0.04, 0.12, 0.05), 0.8, 0.0, None, 0),
    "Mountain":    ((0.42, 0.36, 0.30), 0.95, 0.0, None, 0),
    "BusRed":      ((0.45, 0.05, 0.04), 0.35, 0.2, None, 0),
    "TaxiYellow":  ((0.85, 0.62, 0.05), 0.3, 0.1, None, 0),
    "CarWhite":    ((0.80, 0.80, 0.80), 0.3, 0.1, None, 0),
    "CarGrey":     ((0.25, 0.26, 0.28), 0.3, 0.3, None, 0),
    "Tire":        ((0.02, 0.02, 0.02), 0.9, 0.0, None, 0),
    "LampGlow":    ((1.0, 0.8, 0.55), 0.5, 0.0, (1.0, 0.72, 0.42), 40),
    "CityLights":  ((0.2, 0.18, 0.15), 0.9, 0.0, (1.0, 0.75, 0.45), 8),
    "WindowWarm":  ((0.1, 0.08, 0.05), 0.2, 0.0, (1.0, 0.7, 0.4), 3),
    "Blocker":     ((1, 0, 1), 1.0, 0.0, None, 0),
}


def mat(name):
    if name in _mat_cache:
        return _mat_cache[name]
    path = MAT_DIR + "/MI_" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        parent = ensure_master_material()
        mi = asset_tools.create_asset("MI_" + name, MAT_DIR, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        col, rough, metal, ecol, ek = PALETTE[name]
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(col[0], col[1], col[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
        if ecol:
            mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(ecol[0], ecol[1], ecol[2], 1))
            mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", ek)
        eal.save_loaded_asset(mi)
    _mat_cache[name] = mi
    return mi


def glass():
    if "Glass" not in _mat_cache:
        _mat_cache["Glass"] = ensure_translucent_master()
    return _mat_cache["Glass"]


# --------------------------------------------------------------------------- spawning helpers
_spawned = 0


def _folder(actor, folder):
    actor.set_folder_path(unreal.Name("Umayyad/" + folder))


def block(folder, label, shape, loc, size, material, rot=(0, 0, 0), collide=True, shadow=True):
    """Spawn a basic-shape static mesh. `size` is in centimetres (full extents)."""
    global _spawned
    a = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), mkrot(rot[0], rot[1], rot[2]))
    a.set_actor_label(label)
    smc = a.static_mesh_component
    smc.set_static_mesh(mesh(shape))
    smc.set_world_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    smc.set_material(0, material if isinstance(material, unreal.MaterialInterface) else mat(material))
    smc.set_editor_property("cast_shadow", shadow)
    if not collide:
        smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    _folder(a, folder)
    _spawned += 1
    return a


def box(folder, label, center, size, material, yaw=0, **kw):
    return block(folder, label, CUBE, center, size, material, rot=(0, yaw, 0), **kw)


def box_on_ground(folder, label, x, y, size, material, yaw=0, z0=0, **kw):
    return box(folder, label, (x, y, z0 + size[2] / 2.0), size, material, yaw, **kw)


def cyl(folder, label, x, y, z0, diameter, height, material, **kw):
    return block(folder, label, CYL, (x, y, z0 + height / 2.0), (diameter, diameter, height), material, **kw)


def rotate2d(x, y, yaw_deg):
    r = math.radians(yaw_deg)
    return x * math.cos(r) - y * math.sin(r), x * math.sin(r) + y * math.cos(r)


def local_box(folder, label, origin, yaw, lx, ly, lz0, size, material, **kw):
    """Box positioned in a building's local frame (origin + yaw), bottom at lz0."""
    dx, dy = rotate2d(lx, ly, yaw)
    return box(folder, label, (origin[0] + dx, origin[1] + dy, lz0 + size[2] / 2.0), size, material, yaw, **kw)


def spawn(cls, loc, rot=(0, 0, 0), label=None, folder="Lighting"):
    a = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), mkrot(rot[0], rot[1], rot[2]))
    if label:
        a.set_actor_label(label)
    _folder(a, folder)
    return a


# --------------------------------------------------------------------------- lighting & atmosphere
def build_environment():
    """Golden hour over Damascus: warm low sun from the west, Qasioun catching the light."""
    sun = spawn(unreal.DirectionalLight, (0, 0, 20000), (-9.0, 75.0, 0), "Sun_GoldenHour")
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property("intensity", 8.5)                       # lux, physically based with auto-exposure
    lc.set_editor_property("light_color", unreal.Color(255, 214, 170, 255))
    lc.set_editor_property("atmosphere_sun_light", True)
    lc.set_editor_property("cast_cloud_shadows", True)
    lc.set_editor_property("light_source_angle", 0.9)             # soft contact shadows
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)

    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")

    sky = spawn(unreal.SkyLight, (0, 0, 1000), label="SkyLight_RealTime")
    slc = sky.get_component_by_class(unreal.SkyLightComponent)
    slc.set_editor_property("real_time_capture", True)
    slc.set_mobility(unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property("intensity", 1.0)

    clouds = spawn(unreal.VolumetricCloud, (0, 0, 0), label="VolumetricClouds")

    fog = spawn(unreal.ExponentialHeightFog, (0, 0, -200), label="HeightFog_Dust")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.012)
    fc.set_editor_property("fog_height_falloff", 0.12)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.55, 0.45, 0.35, 1))
    fc.set_editor_property("volumetric_fog", True)
    fc.set_editor_property("volumetric_fog_scattering_distribution", 0.6)
    fc.set_editor_property("volumetric_fog_albedo", unreal.Color(255, 236, 214, 255))

    ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), label="PostProcess_Cinematic")
    ppv.set_editor_property("unbound", True)
    s = ppv.settings

    def pp(name, value):
        try:
            s.set_editor_property("override_" + name, True)
            s.set_editor_property(name, value)
        except Exception as e:  # property names drift between engine versions
            unreal.log_warning("PostProcess: skipped %s (%s)" % (name, e))

    pp("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
    pp("auto_exposure_bias", 0.3)
    pp("auto_exposure_min_brightness", -1.0)
    pp("auto_exposure_max_brightness", 12.0)
    pp("bloom_intensity", 0.45)
    pp("lens_flare_intensity", 0.25)
    pp("vignette_intensity", 0.28)
    pp("film_grain_intensity", 0.08)
    pp("scene_fringe_intensity", 0.15)                # subtle chromatic aberration
    pp("white_temp", 6100.0)
    pp("color_saturation", unreal.Vector4(1.05, 1.02, 0.98, 1.0))
    pp("color_contrast", unreal.Vector4(1.06, 1.06, 1.06, 1.0))
    pp("color_gain_shadows", unreal.Vector4(0.95, 0.98, 1.05, 1.0))   # teal shadows
    pp("color_gain_highlights", unreal.Vector4(1.05, 1.0, 0.93, 1.0)) # warm highlights
    pp("motion_blur_amount", 0.25)
    pp("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN)
    pp("reflection_method", unreal.ReflectionMethod.LUMEN)
    pp("lumen_final_gather_quality", 2.0)
    pp("lumen_scene_lighting_quality", 2.0)
    pp("lumen_reflection_quality", 2.0)
    pp("lumen_scene_detail", 2.0)
    pp("lumen_max_trace_distance", 40000.0)
    pp("ambient_occlusion_intensity", 0.5)
    pp("local_exposure_highlight_contrast_scale", 0.8)
    pp("local_exposure_shadow_contrast_scale", 0.75)
    ppv.set_editor_property("settings", s)
    return sun


# --------------------------------------------------------------------------- ground & roads
ISLAND_R = 4200      # central island radius
ROAD_OUT_R = 7600    # outer edge of the roundabout carriageway
PLAZA_R = 9400       # sidewalks / plazas beyond


def ring(folder, label, radius, width, height, material, segments=48, z0=0, gaps=(), collide=True):
    """Approximate a ring with box segments. `gaps` = list of (center_deg, half_width_deg) to leave open."""
    seg_len = 2 * math.pi * radius / segments * 1.04
    for i in range(segments):
        ang = 360.0 * i / segments
        if any(abs(((ang - c + 180) % 360) - 180) < hw for c, hw in gaps):
            continue
        x, y = rotate2d(radius, 0, ang)
        box(folder, "%s_%02d" % (label, i), (x, y, z0 + height / 2.0), (width, seg_len, height), material, yaw=ang, collide=collide)


def disc(folder, label, x, y, z0, radius, height, material, **kw):
    return cyl(folder, label, x, y, z0, radius * 2, height, material, **kw)


# Radial avenues (bearing in degrees from +X/north, clockwise toward +Y/east)
AVENUES = [
    ("ShukriQuwatli", 95, 2600),    # east -> Baramkeh / old city (Attacker side)
    ("BeirutRoad", 250, 2400),      # west-south-west -> Mezzeh highway
    ("Malki", 50, 1800),            # north-east -> Abu Rummaneh / Malki
    ("Adawi", 330, 1800),           # north-west
    ("Fair", 170, 2000),            # south -> old Fair grounds / Barada
]


def build_ground():
    F = "Ground"
    # Everything sits on a huge asphalt slab; plazas & island are raised kerbs
    box(F, "Asphalt_Base", (0, 0, -25), (60000, 60000, 50), "Asphalt")

    # Sidewalk/plaza ring around the roundabout, broken by avenues
    gaps = [(b, math.degrees(w / 2.0 / 8500) + 2) for _, b, w in AVENUES]
    ring(F, "Sidewalk", 8500, 1800, 18, "Sidewalk", segments=64, gaps=gaps)
    ring(F, "Kerb_Out", ROAD_OUT_R, 40, 22, "Travertine", segments=72, gaps=gaps)

    # Lane markings: dashed rings on the carriageway
    for r in (5300, 6450):
        for i in range(0, 90, 2):
            ang = i * 4.0
            x, y = rotate2d(r, 0, ang)
            box(F, "Lane_%d_%02d" % (r, i), (x, y, 1), (15, 260, 2), "LaneWhite", yaw=ang, collide=False, shadow=False)

    # Avenues: long strips with central median + palms
    for name, bearing, width in AVENUES:
        # carriageway direction vector (bearing measured from +X toward +Y, same as UE yaw)
        dirx, diry = math.cos(math.radians(bearing)), math.sin(math.radians(bearing))
        nx, ny = -diry, dirx
        mid = 7600 + 7000
        # median
        box(F, "Median_" + name, (dirx * mid, diry * mid, 12), (14000, 220, 24), "Travertine", yaw=bearing)
        for k in range(8):
            d = 8800 + k * 1700
            palm("Vegetation", "Palm_%s_%d" % (name, k), dirx * d, diry * d, 24)
        # sidewalks both sides
        for side in (-1, 1):
            o = side * (width / 2.0 + 500)
            box(F, "Walk_%s_%d" % (name, side), (dirx * mid + nx * o, diry * mid + ny * o, 9), (14000, 900, 18), "Sidewalk", yaw=bearing)


def build_island():
    """Central island: lawn, fountain rings and the Damascene Sword monument (mid)."""
    F = "Monument"
    disc(F, "Island_Kerb", 0, 0, 0, ISLAND_R, 30, "Travertine")
    disc(F, "Island_Lawn", 0, 0, 30, ISLAND_R - 120, 6, "Grass")
    # Marble promenade ring with low walls = mid-map cover
    ring(F, "Promenade", 3200, 700, 40, "Marble", segments=48)
    ring(F, "CoverWall", 3600, 60, 110, "Limestone", segments=24, gaps=[(0, 5), (90, 5), (180, 5), (270, 5), (45, 4), (135, 4), (225, 4), (315, 4)], z0=30)

    # Fountain basin, steps and water
    disc(F, "Basin_Outer", 0, 0, 30, 2800, 70, "Basalt")
    disc(F, "Basin_Water", 0, 0, 30, 2700, 60, "Water", collide=False)
    for i, (r, h) in enumerate([(2000, 110), (1400, 150), (900, 200)]):
        disc(F, "FountainTier_%d" % i, 0, 0, 30, r, h, "Marble" if i % 2 == 0 else "Basalt")
        disc(F, "FountainWater_%d" % i, 0, 0, 30 + h, r - 80, 4, "Water", collide=False, shadow=False)
    # Jets (visual placeholders for the Niagara fountain system NS_UmayyadFountain)
    for i in range(16):
        x, y = rotate2d(2400, 0, i * 22.5)
        cyl(F, "Jet_%02d" % i, x, y, 90, 18, 420, "Water", collide=False, shadow=False)

    # Octagonal plinth + shaft
    z = 30
    for i, (r, h, m) in enumerate([(650, 260, "Basalt"), (520, 180, "Marble"), (420, 120, "Mosaic")]):
        cyl(F, "Plinth_%d" % i, 0, 0, z, r * 2, h, m)
        z += h
    shaft_z = z
    # Ablaq-striped column (black basalt / white marble - the Damascene signature)
    stripes = 22
    for i in range(stripes):
        cyl(F, "Shaft_%02d" % i, 0, 0, shaft_z + i * 90, 260 - i * 3, 90, "Basalt" if i % 2 else "Marble")
    top = shaft_z + stripes * 90
    cyl(F, "Capital", 0, 0, top, 360, 70, "Gold")

    # The Damascene sword: steel blade, gold guard & hilt
    blade_h = 1300
    box(F, "Sword_Blade", (0, 0, top + 70 + 260 + blade_h / 2.0), (22, 120, blade_h), "Steel")
    block(F, "Sword_Tip", CONE, (0, 0, top + 70 + 260 + blade_h + 60), (120, 22, 120), "Steel")
    box(F, "Sword_Guard", (0, 0, top + 70 + 240), (60, 520, 50), "Gold")
    cyl(F, "Sword_Grip", 0, 0, top + 70, 60, 220, "Bronze")
    block(F, "Sword_Pommel", SPHERE, (0, 0, top + 60), (110, 110, 110), "Gold")
    # Uplights at the base (warm)
    for i in range(8):
        x, y = rotate2d(700, 0, i * 45 + 22.5)
        l = spawn(unreal.SpotLight, (x, y, 60), (80, i * 45 + 202.5, 0), "Uplight_%d" % i, folder="Monument")
        c = l.get_component_by_class(unreal.SpotLightComponent)
        c.set_editor_property("intensity", 60000.0)
        c.set_editor_property("light_color", unreal.Color(255, 196, 140, 255))
        c.set_editor_property("attenuation_radius", 5000.0)
        c.set_editor_property("outer_cone_angle", 18.0)


# --------------------------------------------------------------------------- landmark buildings
def ablaq_band(folder, label, origin, yaw, lx, ly, z0, length, depth, rows=6, row_h=35):
    """Alternating basalt / limestone courses - the striped masonry of Damascus."""
    for r in range(rows):
        local_box(folder, "%s_%d" % (label, r), origin, yaw, lx, ly, z0 + r * row_h,
                  (depth, length, row_h), "Basalt" if r % 2 == 0 else "Limestone")


def arcade(folder, label, origin, yaw, lx, y_from, y_to, count, height, depth=120, pillar=110):
    """Row of pillars with a continuous lintel and pointed-arch hint (rotated cube)."""
    step = (y_to - y_from) / float(count)
    for i in range(count + 1):
        ly = y_from + i * step
        local_box(folder, "%s_Pillar_%02d" % (label, i), origin, yaw, lx, ly, 0, (pillar, pillar, height), "Limestone")
    for i in range(count):
        ly = y_from + (i + 0.5) * step
        dx, dy = rotate2d(lx, ly, yaw)
        # 45-degree rotated cube reads as a pointed arch crown at a distance
        block(folder, "%s_Arch_%02d" % (label, i), CUBE, (origin[0] + dx, origin[1] + dy, height - 20),
              (depth, step * 0.62, step * 0.62), "Limestone", rot=(45, yaw, 0))
    local_box(folder, label + "_Lintel", origin, yaw, lx, (y_from + y_to) / 2.0, height, (depth + 20, y_to - y_from + pillar, 140), "Limestone")


def stairs(folder, label, origin, yaw, lx_bottom, lx_top, ly, width, steps):
    """Flight rising from lx_bottom (low) to lx_top (against the building). Each step is a solid block."""
    run = (lx_bottom - lx_top) / float(steps)
    rise = 120.0 / steps
    for i in range(steps):
        x_front = lx_bottom - i * run
        length = x_front - lx_top
        local_box(folder, "%s_%02d" % (label, i), origin, yaw, x_front - length / 2.0, ly, 0,
                  (length, width, rise * (i + 1)), "Travertine")


def build_opera_house():
    """Damascus Opera House (دار الأوبرا) - SITE A. Faces the square (north-east)."""
    F = "OperaHouse"
    o = (-15500.0, -10500.0)
    yaw = 40.0             # local +X points toward the square
    # Main hall & fly tower
    local_box(F, "Hall", o, yaw, -3500, 0, 0, (5200, 7600, 2200), "Limestone")
    local_box(F, "FlyTower", o, yaw, -4500, 0, 2200, (2400, 3600, 1400), "Travertine")
    local_box(F, "Roof_Cornice", o, yaw, -3500, 0, 2200, (5400, 7800, 80), "Basalt")
    # Grand portico
    local_box(F, "Portico_Floor", o, yaw, -300, 0, 0, (1400, 7000, 120), "Marble")
    arcade(F, "Portico", o, yaw, 350, -3300, 3300, 9, 1500)
    local_box(F, "Portico_Roof", o, yaw, -300, 0, 1640, (1400, 7000, 160), "Limestone")
    ablaq_band(F, "Ablaq", o, yaw, -870, 0, 1100, 7400, 40)
    # Mashrabiya-like glass curtain behind the columns
    local_box(F, "GlassFront", o, yaw, -900, 0, 120, (30, 6400, 1400), glass(), collide=True)
    # Front steps (the plant spot: elevated, with good angles and cover from pillars)
    stairs(F, "Steps", o, yaw, 1050, 400, 0, 7000, 7)
    # Forecourt (Site A) with planters as cover
    local_box(F, "Forecourt", o, yaw, 2600, 0, 0, (3000, 8000, 10), "Marble")
    for i, ly in enumerate((-2800, -1000, 1000, 2800)):
        local_box(F, "Planter_%d" % i, o, yaw, 2300, ly, 0, (500, 500, 95), "Basalt")
        px, py = rotate2d(2300, ly, yaw)
        cypress("Vegetation", "Cypress_A_%d" % i, o[0] + px, o[1] + py, 95)
    for i, ly in enumerate((-2000, 2000)):
        local_box(F, "Crate_%d" % i, o, yaw, 3300, ly, 10, (160, 160, 160), "Concrete")
        local_box(F, "CrateTop_%d" % i, o, yaw, 3300, ly + 60, 170, (120, 120, 120), "Concrete")
    # Warm interior glow through the glass
    for i, ly in enumerate((-2400, 0, 2400)):
        dx, dy = rotate2d(-1400, ly, yaw)
        l = spawn(unreal.RectLight, (o[0] + dx, o[1] + dy, 800), (0, yaw, 0), "OperaGlow_%d" % i, folder=F)
        c = l.get_component_by_class(unreal.RectLightComponent)
        c.set_editor_property("intensity", 40.0)
        c.set_editor_property("light_color", unreal.Color(255, 190, 130, 255))
        c.set_editor_property("source_width", 1200.0)
        c.set_editor_property("source_height", 800.0)
    dx, dy = rotate2d(2600, 0, yaw)
    return (o[0] + dx, o[1] + dy, 100), yaw


def build_tv_building():
    """Radio & Television building (مبنى الإذاعة والتلفزيون) - SITE B. North-east of the square."""
    F = "RadioTV"
    o = (15000.0, 5500.0)
    yaw = 200.0            # faces south-west, toward the monument
    local_box(F, "Podium", o, yaw, -2000, 0, 0, (4200, 6000, 900), "Concrete")
    local_box(F, "Tower", o, yaw, -2600, 1200, 900, (2200, 2600, 5200), "Limestone")
    # Window bands (some lit - it's a 24h broadcaster)
    for f in range(13):
        z = 1100 + f * 380
        lit = "WindowWarm" if (f * 7) % 3 == 0 else "Basalt"
        local_box(F, "Win_%02d" % f, o, yaw, -1480, 1200, z, (30, 2400, 180), lit, collide=False)
    local_box(F, "Crown", o, yaw, -2600, 1200, 6100, (2300, 2700, 200), "Basalt")
    # Broadcast mast with aviation lights
    ox, oy = o[0] + rotate2d(-2600, 1200, yaw)[0], o[1] + rotate2d(-2600, 1200, yaw)[1]
    cyl(F, "Mast", ox, oy, 6300, 80, 3200, "Steel")
    for k in range(4):
        l = spawn(unreal.PointLight, (ox, oy, 7000 + k * 800), label="AviationLight_%d" % k, folder=F)
        c = l.get_component_by_class(unreal.PointLightComponent)
        c.set_editor_property("intensity", 4000.0)
        c.set_editor_property("light_color", unreal.Color(255, 30, 20, 255))
        c.set_editor_property("attenuation_radius", 600.0)
    # Entrance canopy + plaza (Site B)
    local_box(F, "Canopy", o, yaw, 300, 0, 450, (1200, 3600, 60), "Steel")
    for i, ly in enumerate((-1600, 1600)):
        local_box(F, "CanopyPost_%d" % i, o, yaw, 700, ly, 0, (60, 60, 450), "Steel")
    local_box(F, "Plaza", o, yaw, 2200, 0, 0, (3200, 7000, 10), "Sidewalk")
    # Satellite truck, generator, jersey barriers
    vehicle("Props", "OB_Van", o[0] + rotate2d(2000, -2200, yaw)[0], o[1] + rotate2d(2000, -2200, yaw)[1], yaw + 90, "CarWhite", length=700, height=320)
    local_box(F, "Generator", o, yaw, 2600, 1800, 10, (300, 180, 180), "CarGrey")
    for i in range(3):
        jersey("Props", "Jersey_B_%d" % i, o[0] + rotate2d(3200, -600 + i * 450, yaw)[0], o[1] + rotate2d(3200, -600 + i * 450, yaw)[1], yaw + 90)
    # Satellite dishes on the podium roof
    for i in range(3):
        dx, dy = rotate2d(-1000, -2000 + i * 900, yaw)
        block(F, "Dish_%d" % i, SPHERE, (o[0] + dx, o[1] + dy, 1050), (300, 300, 80), "Steel", rot=(0, 0, 35))
    dx, dy = rotate2d(2200, 0, yaw)
    return (o[0] + dx, o[1] + dy, 100), yaw


def build_general_staff():
    """Long walled government compound on the south-east edge - forms the map's hard boundary."""
    F = "GeneralStaff"
    o = (-15500.0, 12500.0)
    yaw = 135.0
    local_box(F, "MainBlock", o, yaw, -2500, 0, 0, (3000, 14000, 2600), "Travertine")
    for f in range(6):
        local_box(F, "Windows_%d" % f, o, yaw, -980, 0, 350 + f * 380, (30, 13200, 170), "Basalt", collide=False)
    local_box(F, "PerimeterWall", o, yaw, 0, 0, 0, (80, 15000, 420), "Limestone")
    ablaq_band(F, "WallAblaq", o, yaw, 45, 0, 250, 15000, 20, rows=4, row_h=40)
    local_box(F, "Gate", o, yaw, 60, 0, 0, (120, 1000, 600), "Bronze")


def build_hotel():
    """Hotel block & gardens north-west (sniper lane overlooking Defender spawn)."""
    F = "Hotel"
    o = (7600.0, -16300.0)
    yaw = 115.0
    local_box(F, "Block", o, yaw, -2500, 0, 0, (3200, 9000, 3800), "Limestone")
    for f in range(9):
        local_box(F, "Balconies_%d" % f, o, yaw, -870, 0, 300 + f * 380, (120, 8600, 40), "Marble")
        local_box(F, "Glaze_%d" % f, o, yaw, -895, 0, 340 + f * 380, (20, 8600, 300),
                  "WindowWarm" if f % 3 == 1 else "Basalt", collide=False)
    local_box(F, "Garden", o, yaw, 600, 0, 0, (2000, 9000, 20), "Grass")
    for i in range(6):
        dx, dy = rotate2d(700, -3600 + i * 1440, yaw)
        palm("Vegetation", "HotelPalm_%d" % i, o[0] + dx, o[1] + dy, 20)


# --------------------------------------------------------------------------- props & vegetation
def palm(folder, label, x, y, z0, height=900):
    """Date palm: segmented slightly-leaning trunk + drooping frond cones."""
    lean = (h(label) % 7 - 3) * 1.5
    for s in range(6):
        cyl(folder, "%s_T%d" % (label, s), x + s * lean, y, z0 + s * height / 6.0, 55 - s * 3, height / 6.0 + 5, "PalmTrunk")
    top = (x + 6 * lean, y, z0 + height)
    for f in range(9):
        yaw = f * 40 + (h(label) % 40)
        dx, dy = rotate2d(160, 0, yaw)
        block(folder, "%s_F%d" % (label, f), CONE, (top[0] + dx, top[1] + dy, top[2] - 30),
              (80, 420, 90), "PalmLeaf", rot=(-35, yaw, 0), collide=False)


def cypress(folder, label, x, y, z0, height=700):
    cyl(folder, label + "_Trunk", x, y, z0, 30, 150, "PalmTrunk")
    block(folder, label + "_Crown", SPHERE, (x, y, z0 + 150 + height / 2.0), (200, 200, height), "Cypress")


def vehicle(folder, label, x, y, yaw, paint, length=450, height=150, width=180):
    """Car / van / bus blockout - wheels, body, cabin glass. Full-height cover."""
    o = (x, y)
    local_box(folder, label + "_Body", o, yaw, 0, 0, 35, (length, width, height * 0.55), paint)
    cab_len = length * (0.55 if length < 600 else 0.9)
    local_box(folder, label + "_Cabin", o, yaw, -length * 0.05, 0, 35 + height * 0.55, (cab_len, width - 20, height * 0.45), glass())
    for wx in (-length * 0.33, length * 0.33):
        for wy in (-width / 2.0, width / 2.0):
            dx, dy = rotate2d(wx, wy, yaw)
            block(folder, "%s_W_%d_%d" % (label, wx, wy), CYL, (x + dx, y + dy, 35), (70, 70, 25), "Tire", rot=(0, yaw, 90))


def bus(folder, label, x, y, yaw):
    vehicle(folder, label, x, y, yaw, "BusRed", length=1200, height=320, width=250)


def jersey(folder, label, x, y, yaw):
    box_on_ground(folder, label + "_Base", x, y, (60, 300, 40), "Concrete", yaw=yaw)
    box_on_ground(folder, label + "_Top", x, y, (30, 300, 70), "Concrete", yaw=yaw, z0=40)


def lamp(folder, label, x, y, yaw):
    """Twin-arm street lamp with a warm sodium-ish light (IES profile slot in the component)."""
    cyl(folder, label + "_Pole", x, y, 0, 25, 900, "Steel")
    for side in (-1, 1):
        dx, dy = rotate2d(0, side * 120, yaw)
        box(folder, "%s_Arm%d" % (label, side), (x + dx / 2, y + dy / 2, 880), (12, 240, 12), "Steel", yaw=yaw, collide=False)
        box(folder, "%s_Head%d" % (label, side), (x + dx, y + dy, 870), (50, 60, 18), "LampGlow", yaw=yaw, collide=False, shadow=False)
    l = spawn(unreal.PointLight, (x, y, 850), label=label + "_Light", folder="Lighting/Street")
    c = l.get_component_by_class(unreal.PointLightComponent)
    c.set_editor_property("intensity", 12000.0)
    c.set_editor_property("light_color", unreal.Color(255, 176, 110, 255))
    c.set_editor_property("attenuation_radius", 1800.0)
    c.set_editor_property("source_radius", 20.0)
    c.set_editor_property("cast_shadows", False)   # perf: dozens of these; the sun owns the shadows


def kiosk(folder, label, x, y, yaw):
    """Juice / ka'ak kiosk - classic Damascus street furniture. Half-height cover with a lit counter."""
    box_on_ground(folder, label + "_Body", x, y, (250, 320, 230), "Mosaic", yaw=yaw)
    box_on_ground(folder, label + "_Roof", x, y, (330, 400, 25), "Steel", yaw=yaw, z0=230)
    box_on_ground(folder, label + "_Counter", x, y, (20, 300, 20), "WindowWarm", yaw=yaw, z0=120, collide=False)


def bus_stop(folder, label, x, y, yaw):
    local = (x, y)
    local_box(folder, label + "_Roof", local, yaw, 0, 0, 260, (180, 500, 20), "Steel")
    local_box(folder, label + "_Back", local, yaw, -80, 0, 0, (10, 500, 250), glass())
    local_box(folder, label + "_Bench", local, yaw, -30, 0, 0, (60, 400, 50), "Steel")


def build_props():
    F = "Props"
    # Lamps around the roundabout outer kerb
    for i in range(24):
        a = i * 15 + 7.5
        x, y = rotate2d(ROAD_OUT_R + 250, 0, a)
        lamp(F, "Lamp_Ring_%02d" % i, x, y, a)
    # Lamps on the island promenade
    for i in range(8):
        a = i * 45 + 22.5
        x, y = rotate2d(3900, 0, a)
        lamp(F, "Lamp_Island_%d" % i, x, y, a)

    # Traffic frozen in the roundabout = the core cover language of the map
    traffic = [
        (5900, 18, "bus"), (5900, 62, "TaxiYellow"), (5000, 80, "CarWhite"), (6200, 118, "CarGrey"),
        (5200, 150, "bus"), (5800, 196, "TaxiYellow"), (5100, 222, "CarWhite"), (6300, 258, "CarGrey"),
        (5600, 290, "TaxiYellow"), (5000, 318, "bus"), (6200, 342, "CarWhite"),
    ]
    for i, (r, a, kind) in enumerate(traffic):
        x, y = rotate2d(r, 0, a)
        yaw = a + 90 + (h(kind + str(i)) % 21 - 10)   # tangent to the ring, a bit askew
        if kind == "bus":
            bus(F, "Bus_%02d" % i, x, y, yaw)
        else:
            vehicle(F, "Car_%02d" % i, x, y, yaw, kind)

    # Kiosks & bus stops on the plaza ring
    for i, a in enumerate((35, 125, 205, 300)):
        x, y = rotate2d(8700, 0, a)
        kiosk(F, "Kiosk_%d" % i, x, y, a)
    for i, a in enumerate((70, 160, 235, 345)):
        x, y = rotate2d(8900, 0, a)
        bus_stop(F, "BusStop_%d" % i, x, y, a + 180)

    # Checkpoint-style barrier chicanes at avenue mouths (choke points)
    for name, bearing, width in AVENUES:
        for k in range(3):
            d = 10200 + k * 700
            off = (k - 1) * width * 0.3
            dx, dy = rotate2d(d, off, bearing)
            jersey(F, "Chicane_%s_%d" % (name, k), dx, dy, bearing + 90)

    # Cypress & palm rings on the island lawn
    for i in range(20):
        a = i * 18
        x, y = rotate2d(3950, 0, a + 9)
        if i % 2:
            palm("Vegetation", "IslandPalm_%02d" % i, x, y, 36)
        else:
            cypress("Vegetation", "IslandCypress_%02d" % i, x, y, 36)


# --------------------------------------------------------------------------- skyline & bounds
def build_backdrop():
    """Mount Qasioun (جبل قاسيون) to the north with thousands of house lights, and a low skyline."""
    F = "Backdrop"
    for i in range(9):
        y = -60000 + i * 15000
        h = 30000 + (h("q%d" % i) % 9000)
        block(F, "Qasioun_%d" % i, SPHERE, (75000 + (i % 3) * 4000, y, -h * 0.2), (40000, 26000, h * 1.2), "Mountain", collide=False)
    # City lights clinging to the slopes (emissive specks, no real lights - cheap)
    for i in range(160):
        yy = -55000 + (h("l%d" % i) % 110000)
        zz = 1500 + (h("z%d" % i) % 9000)
        xx = 60000 + zz * 0.9
        box(F, "HouseLight_%03d" % i, (xx, yy, zz), (200, 300, 150), "CityLights", collide=False, shadow=False)
    # Mid-rise skyline ring
    for i in range(36):
        a = i * 10
        if any(abs(((a - b + 180) % 360) - 180) < 12 for _, b, _ in AVENUES):
            continue
        r = 26000 + (h("r%d" % i) % 6000)
        x, y = rotate2d(r, 0, a)
        h = 1500 + (h("h%d" % i) % 3500)
        box_on_ground(F, "Skyline_%02d" % i, x, y, (2400, 3000, h), "Limestone" if i % 3 else "Travertine", yaw=a, collide=False)
        box_on_ground(F, "SkylineWin_%02d" % i, x, y, (2420, 2800, 120), "WindowWarm", yaw=a, z0=h * 0.6, collide=False, shadow=False)


def build_bounds():
    """Invisible player-blocking cylinder of walls, ~21 m past the playable plazas."""
    F = "Bounds"
    for i in range(48):
        a = i * 7.5
        x, y = rotate2d(21000, 0, a)
        w = box(F, "Bound_%02d" % i, (x, y, 1500), (100, 2800, 3000), "Blocker", yaw=a)
        w.set_actor_hidden_in_game(True)
        w.static_mesh_component.set_collision_profile_name("InvisibleWall")
        w.static_mesh_component.set_editor_property("cast_shadow", False)
        w.static_mesh_component.set_visibility(False)


# --------------------------------------------------------------------------- gameplay actors
def game_class(name):
    cls = unreal.load_class(None, "/Script/UmayyadStrike." + name)
    if cls is None:
        unreal.log_error("C++ class %s not found - build the UmayyadStrike module first." % name)
    return cls


def build_gameplay(site_a, site_b):
    F = "Gameplay"
    # Spawns: Attackers (المهاجمون) come in from Shukri al-Quwatli (east),
    # Defenders (المدافعون) hold the west plaza between the two sites.
    t_center, ct_center = (-1500.0, 17000.0), (0.0, -12000.0)
    for team, (cx, cy), face in (("Attackers", t_center, 265.0), ("Defenders", ct_center, 90.0)):
        for i in range(5):
            dx, dy = rotate2d((i - 2) * 220, (i % 2) * 220, face + 90)
            ps = spawn(unreal.PlayerStart, (cx + dx, cy + dy, 120), (0, face, 0), "%s_Start_%d" % (team, i), folder=F)
            ps.set_editor_property("player_start_tag", team)
        buy_cls = game_class("USBuyZone")
        if buy_cls:
            bz = spawn(buy_cls, (cx, cy, 300), (0, face, 0), "BuyZone_" + team, folder=F)
            bz.set_editor_property("team", unreal.USTeam.ATTACKERS if team == "Attackers" else unreal.USTeam.DEFENDERS)

    site_cls = game_class("USBombSite")
    if site_cls:
        for name, (loc, yaw) in (("A", site_a), ("B", site_b)):
            s = spawn(site_cls, loc, (0, yaw, 0), "BombSite_" + name, folder=F)
            s.set_editor_property("site_name", name)

    # Spawn-side cover so spawns can't be sprayed from range
    for i in range(4):
        jersey("Props", "T_SpawnCover_%d" % i, t_center[0] + (i - 1.5) * 500, t_center[1] - 1300, 0)
        jersey("Props", "CT_SpawnCover_%d" % i, ct_center[0] + (i - 1.5) * 500, ct_center[1] + 1300, 0)


# --------------------------------------------------------------------------- main
def main():
    with unreal.ScopedSlowTask(9, "Building Umayyad Square...") as task:
        task.make_dialog(True)

        task.enter_progress_frame(1, "New level")
        if eal.does_asset_exist(MAP_PATH):
            level_sub.load_level(MAP_PATH)
            for a in actor_sub.get_all_level_actors():
                if str(a.get_folder_path()).startswith("Umayyad"):
                    actor_sub.destroy_actor(a)
        else:
            level_sub.new_level(MAP_PATH)

        task.enter_progress_frame(1, "Materials")
        ensure_master_material()
        for name in PALETTE:
            mat(name)

        task.enter_progress_frame(1, "Sky, sun, Lumen, fog")
        build_environment()
        task.enter_progress_frame(1, "Roads & plazas")
        build_ground()
        task.enter_progress_frame(1, "The Damascene Sword monument")
        build_island()
        task.enter_progress_frame(1, "Opera House, Radio & TV, General Staff, hotel")
        site_a = build_opera_house()
        site_b = build_tv_building()
        build_general_staff()
        build_hotel()
        task.enter_progress_frame(1, "Traffic, lamps, kiosks, vegetation")
        build_props()
        task.enter_progress_frame(1, "Qasioun & skyline")
        build_backdrop()
        build_bounds()
        task.enter_progress_frame(1, "Spawns, buy zones, bomb sites")
        build_gameplay(site_a, site_b)

    level_sub.save_current_level()
    unreal.log("Umayyad Square built: %d mesh actors. Press Play (Alt+P)." % _spawned)


main()
