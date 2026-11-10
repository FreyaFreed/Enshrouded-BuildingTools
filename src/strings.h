// Every text the player sees, with its key in the translation files and the English
// text. {0}, {1}, ... are filled in by the mod (numbers, key names). The translation
// files (lang/<code>.txt, installed as BuildingTools-Languages\<code>.txt) use the same
// keys; lang/en.txt must match this list (the unit tests check it).
//
// X(id, key, English)
#define BT_STRINGS(X) \
    /* numbers */ \
    X(S_DECIMAL, "num.decimal", ".") \
    /* messages */ \
    X(S_READY, "msg.ready", "Building Tools {0} ready") \
    X(S_READY_CREATIVE, "msg.ready_creative", "Building Tools {0} ready (creative building)") \
    X(S_UNSUPPORTED, "msg.unsupported", "Building Tools: game version not supported") \
    X(S_MOD_OFF, "msg.mod_off", "Building Tools OFF (vanilla game)  |  {0} turns it on") \
    X(S_MOD_ON, "msg.mod_on", "Building Tools ON  |  {0} turns it off") \
    X(S_MODE_AREA, "msg.mode_area", "Area mode") \
    X(S_MODE_PREFAB, "msg.mode_prefab", "Prefab mode (vanilla)") \
    X(S_REPLACE_ON, "msg.replace_on", "Replace ON") \
    X(S_REPLACE_OFF, "msg.replace_off", "Replace OFF") \
    X(S_STYLE_FULL, "msg.style_full", "Replace style: full shape") \
    X(S_STYLE_OVERLAP, "msg.style_overlap", "Replace style: overlap only") \
    X(S_STYLE_CONNECTED, "msg.style_connected", "Replace style: connected (fill bucket, Prefab mode; right click deletes)") \
    X(S_FILL_SOLID, "msg.fill_solid", "Area fill: solid") \
    X(S_FILL_HOLLOW, "msg.fill_hollow", "Area fill: hollow room (walls, floor, ceiling)") \
    X(S_FILL_WALLS, "msg.fill_walls", "Area fill: walls only") \
    X(S_FILL_ROOF, "msg.fill_roof", "Area fill: roof ({0})") \
    X(S_ROOF_SHAPE, "msg.roof_shape", "Roof shape: {0}") \
    X(S_ROOF_TURNED, "msg.roof_turned", "Roof turned ({0} of {1})") \
    X(S_THICK_SET, "msg.thick", "Thickness: {0}") \
    X(S_OVERHANG_SET, "msg.overhang", "Roof overhang: {0}") \
    X(S_FILL_ROUND, "msg.fill_round", "Area fill: round ({0}, {1})") \
    X(S_ROUND_SHAPE, "msg.round_shape", "Round shape: {0}") \
    X(S_ROUND_TURNED, "msg.round_turned", "Round shape turned ({0} of {1})") \
    X(S_ROUND_SOLID, "msg.round_solid", "Round fill: solid") \
    X(S_ROUND_HOLLOW, "msg.round_hollow", "Round fill: hollow (a round wall)") \
    X(S_ROUND_STANDING, "msg.round_standing", "Round fill: standing") \
    X(S_ROUND_LYING, "msg.round_lying", "Round fill: lying (a tube along the area)") \
    X(S_FILL_DIAG, "msg.fill_diag", "Area fill: diagonal ({0})") \
    X(S_DIAG_TURNED, "msg.diag_turned", "Diagonal turned ({0} of {1})") \
    X(S_DIAG_STANDING, "msg.diag_standing", "Diagonal fill: standing (a wall from corner to corner)") \
    X(S_EXACT_ON, "msg.exact_on", "Exact mode ON: click once, then enter the sizes") \
    X(S_EXACT_CANCELLED, "msg.exact_cancelled", "Exact area cancelled") \
    X(S_EXACT_CLICK, "msg.exact_click", "Click to build the exact area") \
    X(S_EXACT_TOO_BIG, "msg.exact_too_big", "Too big: more than {0} shapes") \
    X(S_DIAG_LYING, "msg.diag_lying", "Diagonal fill: lying (a ramp)") \
    X(S_FREE_ON, "msg.free_on", "Free placement ON (no surface snapping)") \
    X(S_FREE_OFF, "msg.free_off", "Free placement OFF") \
    X(S_FREE_DISTANCE, "msg.free_distance", "Free placement distance {0} m") \
    X(S_CREATIVE_ON, "msg.creative_on", "Creative building ON (building costs nothing)") \
    X(S_CREATIVE_OFF, "msg.creative_off", "Creative building OFF") \
    X(S_CREATIVE_SERVER, "msg.creative_server", "Creative building is set on the server (its BuildingTools.ini)") \
    X(S_CORNERS_CLEARED, "msg.corners_cleared", "Corners cleared") \
    X(S_BUILD_CORNER, "msg.build_corner", "Build corner {0} of {1} set") \
    X(S_REPLACE_CORNER, "msg.replace_corner", "Replace corner {0} of {1} set") \
    X(S_REMOVE_CORNER, "msg.remove_corner", "Remove corner {0} of {1} set") \
    X(S_REMOVE_PIECE, "msg.remove_piece", "Building pieces are removed one at a time (Area remove works on blocks)") \
    X(S_TOO_BIG, "msg.too_big", "Too big: {0} shapes (max {1})") \
    X(S_PLACED, "msg.placed", "Placed {0} shapes") \
    X(S_PLACED_PARTIAL, "msg.placed_partial", "Placed {0} of {1} shapes (not enough material)") \
    X(S_REMOVED, "msg.removed", "Removed {0} shapes") \
    X(S_AREA_SENT, "msg.area_sent", "Area sent to the server: {0} shapes") \
    X(S_UNDO_COVERS, "msg.undo_covers", "undo covers {0}") \
    X(S_NOT_ENOUGH, "msg.not_enough", "Not enough material for {0} shapes (needs {1} items)") \
    X(S_UNDO_NEEDS, "msg.undo_needs", "Undo needs the replaced material back in your inventory") \
    X(S_NOTHING_TO_REPLACE, "msg.nothing_to_replace", "Nothing to replace there") \
    X(S_REPLACE_READ_FAIL, "msg.replace_read_fail", "Replace: could not read the aimed block, try again") \
    X(S_CREPLACE_OWN, "msg.creplace_own", "Connected replace: that is already your material") \
    X(S_CREPLACE_NOTHING, "msg.creplace_nothing", "Connected replace: nothing to replace there") \
    X(S_CREPLACE_CAPPED, "msg.creplace_capped", "Connected replace: only the first {0} shapes (FillLimit)") \
    X(S_CDELETE_UNAVAILABLE, "msg.cdelete_unavailable", "Connected delete: not available in this game version") \
    X(S_CDELETE_NOTHING, "msg.cdelete_nothing", "Connected delete: nothing to remove there") \
    X(S_CDELETE_CAPPED, "msg.cdelete_capped", "Connected delete: only the first {0} shapes (FillLimit)") \
    X(S_LANGUAGE_SET, "msg.language_set", "Building Tools language: English") \
    /* info bar (bottom centre) */ \
    X(S_BAR_AREA_BUILD, "bar.area_build", "Area build {0}/{1}") \
    X(S_BAR_AREA_REPLACE, "bar.area_replace", "Area replace {0}/{1}") \
    X(S_BAR_AREA_REMOVE, "bar.area_remove", "Area remove {0}/{1}") \
    /* rows in the game's building box (bottom right; the box is narrow) */ \
    X(S_HUD_AREA, "hud.area", "Area mode") \
    X(S_HUD_AREA_ON, "hud.area_on", "Area mode: on") \
    X(S_HUD_AREA_OFF, "hud.area_off", "Area mode: off") \
    X(S_HUD_FILL_SOLID, "hud.fill_solid", "Fill: solid") \
    X(S_HUD_FILL_HOLLOW, "hud.fill_hollow", "Fill: hollow") \
    X(S_HUD_FILL_WALLS, "hud.fill_walls", "Fill: walls") \
    X(S_HUD_FILL_ROOF, "hud.fill_roof", "Fill: roof") \
    X(S_HUD_ROOF_SHAPE, "hud.roof_shape", "Roof: {0}") \
    X(S_HUD_ROOF_TURN, "hud.roof_turn", "Turn roof") \
    X(S_HUD_FILL_ROUND, "hud.fill_round", "Fill: round") \
    X(S_HUD_ROUND_SHAPE, "hud.round_shape", "Round: {0}") \
    X(S_HUD_ROUND_TURN, "hud.round_turn", "Turn round shape") \
    X(S_HUD_ROUND_SOLID, "hud.round_solid", "Round: solid") \
    X(S_HUD_ROUND_HOLLOW, "hud.round_hollow", "Round: hollow") \
    X(S_HUD_ROUND_STANDING, "hud.round_standing", "Round: standing") \
    X(S_HUD_ROUND_LYING, "hud.round_lying", "Round: lying") \
    X(S_HUD_FILL_DIAG, "hud.fill_diag", "Fill: diagonal") \
    X(S_HUD_DIAG_TURN, "hud.diag_turn", "Turn diagonal") \
    X(S_HUD_DIAG_STANDING, "hud.diag_standing", "Diagonal: standing") \
    X(S_HUD_DIAG_LYING, "hud.diag_lying", "Diagonal: lying") \
    X(S_HUD_THICK, "hud.thick", "Thickness") \
    X(S_HUD_CLEAR, "hud.clear", "Clear corners") \
    X(S_HUD_REPLACE, "hud.replace", "Replace") \
    X(S_HUD_REPLACE_ON, "hud.replace_on", "Replace: on") \
    X(S_HUD_REPLACE_OFF, "hud.replace_off", "Replace: off") \
    X(S_HUD_STYLE_FULL, "hud.style_full", "Style: full") \
    X(S_HUD_STYLE_OVERLAP, "hud.style_overlap", "Style: overlap") \
    X(S_HUD_STYLE_CONNECTED, "hud.style_connected", "Style: connected") \
    X(S_HUD_FREE, "hud.free", "Free placement") \
    X(S_HUD_FREE_ON, "hud.free_on", "Free placement: on") \
    X(S_HUD_FREE_OFF, "hud.free_off", "Free placement: off") \
    X(S_HUD_WHEEL, "hud.wheel", "+ wheel: {0} m") \
    X(S_HUD_MENU, "hud.menu", "Tools menu") \
    X(S_HUD_RADIAL, "hud.radial", "Tools wheel") \
    /* floating text under the crosshair */ \
    X(S_FLOAT_TOO_BIG, "float.too_big", "too big") \
    X(S_FLOAT_SIZE, "float.size", "{0} x {1}, h {2} = {3}") \
    /* key names */ \
    X(S_KEY_CTRL, "key.ctrl", "Ctrl") \
    X(S_KEY_SHIFT, "key.shift", "Shift") \
    X(S_KEY_ALT, "key.alt", "Alt") \
    X(S_KEY_NUMBER, "key.number", "key {0}") \
    X(S_KEY_MOUSE_MIDDLE, "key.mouse_middle", "Middle mouse") \
    X(S_KEY_MOUSE4, "key.mouse4", "Mouse 4") \
    X(S_KEY_MOUSE5, "key.mouse5", "Mouse 5") \
    /* menu */ \
    X(S_MENU_TITLE, "menu.title", "Building Tools") \
    X(S_MENU_KEYS, "menu.keys", "Keys") \
    X(S_MENU_OPTIONS, "menu.options", "Options") \
    X(S_MENU_QUICK_KEYS, "menu.quick_keys", "Quick keys") \
    X(S_MENU_FOOTER, "menu.footer", "Version {0}. Esc or the menu key closes the menu.") \
    X(S_MENU_CLOSE, "menu.close", "Close") \
    X(S_MENU_CANCEL, "menu.cancel", "Cancel") \
    X(S_MENU_PRESS_KEY, "menu.press_key", "Press a key...") \
    X(S_MENU_NO_KEY, "menu.no_key", "None") \
    X(S_MENU_KEYS_HELP, "menu.keys_help", "Click a key to change it. Saved to BuildingTools.ini.") \
    X(S_MENU_KEYS_WAITING, "menu.keys_waiting", "Press the new key, also with Shift, Ctrl or Alt. Esc cancels, Delete clears it.") \
    X(S_MENU_TRANSLATION_NOTE, "menu.translation_note", "") \
    X(S_ACT_MENU, "menu.key_menu", "Building Tools menu") \
    X(S_ACT_RADIAL, "menu.key_radial", "Tools wheel (hold)") \
    X(S_ACT_AREA, "menu.key_area", "Area mode on / off") \
    X(S_ACT_REPLACE, "menu.key_replace", "Replace on / off") \
    X(S_ACT_STYLE, "menu.key_style", "Replace style") \
    X(S_ACT_FILL, "menu.key_fill", "Area fill") \
    X(S_ACT_ROOF_SHAPE, "menu.key_roof_shape", "Next roof / round shape (Area mode, roof or round fill)") \
    X(S_ACT_ROOF_TURN, "menu.key_roof_turn", "Turn roof / round / diagonal (Area mode, those fills)") \
    X(S_ACT_THICK, "menu.key_thick", "Thickness 1 / 2 / 3 (rooms, walls, roofs)") \
    X(S_ACT_OVERHANG, "menu.key_overhang", "Roof overhang none / 1 / 2 / 3") \
    X(S_ACT_ROUND_HOLLOW, "menu.key_round_hollow", "Round fill: solid / hollow") \
    X(S_ACT_ROUND_LYING, "menu.key_round_lying", "Round and diagonal fills: standing / lying") \
    X(S_ACT_CLEAR, "menu.key_clear", "Clear area corners") \
    X(S_ACT_EXACT, "menu.key_exact", "Exact mode on / off (one click, enter the sizes)") \
    X(S_ACT_FREE, "menu.key_free", "Free placement on / off") \
    X(S_ACT_FARTHER, "menu.key_farther", "Free placement farther") \
    X(S_ACT_CLOSER, "menu.key_closer", "Free placement closer") \
    X(S_ACT_WHEEL, "menu.key_wheel", "Distance (hold + mouse wheel)") \
    X(S_ACT_OFF, "menu.key_off", "Whole mod off / on") \
    X(S_ACT_STYLE_FULL, "menu.key_style_full", "Replace style: full shape") \
    X(S_ACT_STYLE_OVERLAP, "menu.key_style_overlap", "Replace style: overlap only") \
    X(S_ACT_STYLE_CONNECTED, "menu.key_style_connected", "Replace style: connected") \
    X(S_ACT_FILL_SOLID, "menu.key_fill_solid", "Area fill: solid") \
    X(S_ACT_FILL_HOLLOW, "menu.key_fill_hollow", "Area fill: hollow room") \
    X(S_ACT_FILL_WALLS, "menu.key_fill_walls", "Area fill: walls only") \
    X(S_ACT_FILL_ROOF, "menu.key_fill_roof", "Area fill: roof") \
    X(S_ACT_FILL_ROUND, "menu.key_fill_round", "Area fill: round") \
    X(S_ACT_FILL_DIAG, "menu.key_fill_diag", "Area fill: diagonal") \
    X(S_ACT_CREATIVE, "menu.key_creative", "Creative building on / off") \
    X(S_ACT_REPLACE_HOLD, "menu.key_replace_hold", "Replace while held") \
    X(S_ACT_PREFAB, "menu.key_prefab", "Prefab mode (vanilla)") \
    X(S_ACT_AREA_ON, "menu.key_area_on", "Area mode") \
    X(S_ACT_REPLACE_ON, "menu.key_replace_on", "Replace on") \
    X(S_ACT_REPLACE_OFF, "menu.key_replace_off", "Replace off") \
    X(S_MENU_QUICK_HELP, "menu.quick_help", "Keys that set an option directly. Click a key to set it.") \
    X(S_OPT_OFF, "option.off", "Off") \
    X(S_OPT_ON, "option.on", "On") \
    X(S_OPT_MODE, "option.mode", "Mode") \
    X(S_OPT_MODE_PREFAB, "option.mode_prefab", "Prefab (vanilla)") \
    X(S_OPT_MODE_AREA, "option.mode_area", "Area") \
    X(S_OPT_REPLACE, "option.replace", "Replace") \
    X(S_OPT_STYLE, "option.style", "Replace style") \
    X(S_OPT_STYLE_FULL, "option.style_full", "Full shape") \
    X(S_OPT_STYLE_OVERLAP, "option.style_overlap", "Overlap only") \
    X(S_OPT_STYLE_CONNECTED, "option.style_connected", "Connected (fill bucket)") \
    X(S_OPT_CORNERS, "option.corners", "Area corners") \
    X(S_OPT_CORNERS_2, "option.corners_2", "Two corners") \
    X(S_OPT_CORNERS_3, "option.corners_3", "Base corners + height") \
    X(S_OPT_CORNERS_EXACT, "option.corners_exact", "Exact (enter the sizes)") \
    X(S_OPT_FILL, "option.fill", "Area fill") \
    X(S_OPT_FILL_SOLID, "option.fill_solid", "Solid") \
    X(S_OPT_FILL_HOLLOW, "option.fill_hollow", "Hollow room") \
    X(S_OPT_FILL_WALLS, "option.fill_walls", "Walls only") \
    X(S_OPT_FILL_ROOF, "option.fill_roof", "Roof") \
    X(S_OPT_FILL_ROUND, "option.fill_round", "Round") \
    X(S_OPT_FILL_DIAG, "option.fill_diag", "Diagonal") \
    X(S_OPT_ROOF_SHAPE, "option.roof_shape", "Roof shape") \
    X(S_OPT_ROOF_GABLE, "option.roof_gable", "Gable (2 slopes)") \
    X(S_OPT_ROOF_HIP, "option.roof_hip", "Hip (4 slopes)") \
    X(S_OPT_ROOF_GAMBREL, "option.roof_gambrel", "Gambrel (barn)") \
    X(S_OPT_ROOF_MANSARD, "option.roof_mansard", "Mansard") \
    X(S_OPT_ROOF_SHED, "option.roof_shed", "Shed (1 slope)") \
    X(S_OPT_ROOF_CONE, "option.roof_cone", "Conical (round)") \
    X(S_OPT_ROOF_DOME, "option.roof_dome", "Dome (round)") \
    X(S_OPT_ROOF_TURN, "option.roof_turn", "Roof turn") \
    X(S_OPT_OVERHANG, "option.overhang", "Roof overhang") \
    X(S_OPT_ROUND_SHAPE, "option.round_shape", "Round shape") \
    X(S_OPT_ROUND_FULL, "option.round_full", "Full") \
    X(S_OPT_ROUND_HALF, "option.round_half", "Half") \
    X(S_OPT_ROUND_QUARTER, "option.round_quarter", "Quarter") \
    X(S_OPT_ROUND_TURN, "option.round_turn", "Round turn") \
    X(S_OPT_ROUND_FILL, "option.round_fill", "Round inside") \
    X(S_OPT_ROUND_HOLLOW, "option.round_hollow", "Hollow (wall)") \
    X(S_OPT_ROUND_DIR, "option.round_dir", "Round direction") \
    X(S_OPT_ROUND_STANDING, "option.round_standing", "Standing") \
    X(S_OPT_ROUND_LYING, "option.round_lying", "Lying (tube)") \
    X(S_OPT_DIAG_DIR, "option.diag_dir", "Diagonal direction") \
    X(S_OPT_DIAG_LYING, "option.diag_lying", "Lying (ramp)") \
    X(S_OPT_DIAG_TURN, "option.diag_turn", "Diagonal turn") \
    X(S_OPT_THICK, "option.thick", "Thickness (rooms, walls, roofs)") \
    X(S_OPT_NONE, "option.none", "None") \
    X(S_OPT_FREE, "option.free", "Free placement") \
    X(S_OPT_CREATIVE, "option.creative", "Creative building") \
    X(S_OPT_TOOLS_HELP, "option.tools_help", "Tools in use. A tool that is off leaves its keys to the game and leaves the hints and the wheel.") \
    X(S_OPT_MOD, "option.mod", "Building Tools (whole mod)") \
    X(S_OPT_TOOL_AREA, "option.tool_area", "Area mode") \
    X(S_OPT_TOOL_SHAPES, "option.tool_shapes", "Area shapes (roofs, round, diagonal)") \
    X(S_OPT_GLOBAL_CHESTS, "option.global_chests", "Magic chests of all bases") \
    X(S_OPT_LANGUAGE, "option.language", "Language") \
    X(S_OPT_LANGUAGE_GAME, "option.language_game", "Same as the game") \
    /* exact mode window */ \
    X(S_EXACT_TITLE, "exact.title", "Exact size") \
    X(S_EXACT_HELP, "exact.help", "Shapes past the clicked one: 0 = only it, negative = the other way.") \
    X(S_EXACT_KEY_LOOK, "exact.key_look", "Hold: hide the window, look around, walk") \
    X(S_EXACT_MATERIAL, "exact.material", "Material") \
    X(S_EXACT_REF, "exact.reference", "Clicked block") \
    X(S_EXACT_REF_CENTRE, "exact.ref_centre", "Middle (bottom)") \
    X(S_EXACT_REF_FL, "exact.ref_front_left", "Front left corner") \
    X(S_EXACT_REF_FR, "exact.ref_front_right", "Front right corner") \
    X(S_EXACT_REF_BL, "exact.ref_back_left", "Back left corner") \
    X(S_EXACT_REF_BR, "exact.ref_back_right", "Back right corner") \
    X(S_EXACT_REF_FC, "exact.ref_front_centre", "Front middle (bottom)") \
    X(S_EXACT_REF_BC, "exact.ref_back_centre", "Back middle (bottom)") \
    X(S_EXACT_WIDTH, "exact.width", "Width") \
    X(S_EXACT_DEPTH, "exact.depth", "Depth") \
    X(S_EXACT_HEIGHT, "exact.height", "Height") \
    X(S_EXACT_SIZE, "exact.size", "{0} x {1}, h {2} = {3} shapes") \
    X(S_EXACT_BUILD, "exact.build", "Build") \
    X(S_EXACT_REMOVE, "exact.remove", "Remove") \
    X(S_EXACT_REPLACE, "exact.replace", "Replace") \
    X(S_EXACT_CANCEL, "exact.cancel", "Cancel") \
    /* object placement */ \
    X(S_OBJ_SINGLE, "obj.single", "Single") \
    X(S_OBJ_LINE, "obj.line", "Line") \
    X(S_OBJ_CURVE, "obj.curve", "Curve") \
    X(S_OBJ_CIRCLE, "obj.circle", "Circle") \
    X(S_OBJ_ARCH, "obj.arch", "Arch") \
    X(S_OBJ_VCIRCLE, "obj.vcircle", "Standing circle") \
    X(S_OBJ_MODE, "obj.mode", "Objects: {0}") \
    X(S_OBJ_POINT, "obj.point", "{0}: point {1} of {2} set") \
    X(S_OBJ_PLACED, "obj.placed", "Objects placed: {0}") \
    X(S_OBJ_NOT_ENOUGH, "obj.not_enough", "Not enough for {0} objects") \
    X(S_OBJ_CLEARED, "obj.cleared", "Object line points cleared") \
    X(S_OBJ_REMOTE, "obj.remote", "Object lines work only in a world you host (not yet when you join a server)") \
    X(S_FLOAT_OBJ, "obj.float", "{0}: click {1} of {2}, {3} objects") \
    X(S_OBJ_TITLE, "obj.title", "Object placement") \
    X(S_OBJ_HELP, "obj.help", "The clicks before the last only mark points.") \
    X(S_OBJ_HOW_SINGLE, "obj.how_single", "Single: each click places one object, as in the game.") \
    X(S_OBJ_HOW_LINE, "obj.how_line", "Line: click the start, then the end.") \
    X(S_OBJ_HOW_CURVE, "obj.how_curve", "Curve: start, end, then a point it bends through.") \
    X(S_OBJ_HOW_CIRCLE, "obj.how_circle", "Circle: the centre, then a point on the circle.") \
    X(S_OBJ_HOW_ARCH, "obj.how_arch", "Arch: its two ends (a half circle standing up).") \
    X(S_OBJ_HOW_VCIRCLE, "obj.how_vcircle", "Standing circle: the centre, then a point on it.") \
    X(S_OBJ_MODE_LABEL, "obj.mode_label", "Mode") \
    X(S_OBJ_COUNT, "obj.count", "Number of objects") \
    X(S_OBJ_SPACING, "obj.spacing", "Random spacing (%)") \
    X(S_OBJ_ROT, "obj.rotation", "Random rotation (degrees)") \
    X(S_OBJ_ELEV, "obj.elevation", "Random elevation (cm)") \
    X(S_OBJ_REF, "obj.reference", "Reference plane") \
    X(S_OBJ_REF_GROUND, "obj.ref_ground", "Ground under each object (objects on walls: as Slope)") \
    X(S_OBJ_REF_LEVEL, "obj.ref_level", "Level (first click height)") \
    X(S_OBJ_REF_SLOPE, "obj.ref_slope", "Slope (straight from click to click)") \
    X(S_OBJ_TURN, "obj.turn", "Turn with the line (each object faces along it)") \
    X(S_OBJ_REROLL, "obj.reroll", "New random values") \
    X(S_OBJ_CLOSE, "obj.close", "Close") \
    X(S_WHEEL_OBJ_MODE, "obj.wheel_mode", "Objects") \
    X(S_WHEEL_OBJ_MENU, "obj.wheel_menu", "Object settings") \
    X(S_WHEEL_OBJ_CLEAR, "obj.wheel_clear", "Clear points") \
    X(S_ACT_OBJ_MODE, "obj.key_mode", "Object placement: next mode (single, line, curve, circle, arch, standing circle)") \
    X(S_ACT_OBJ_MENU, "obj.key_menu", "Object placement: settings window") \
    X(S_ACT_OBJ_UNDO, "obj.key_undo", "Object placement: undo (object in hand)") \
    X(S_OPT_TOOL_OBJECTS, "obj.tool", "Object lines") \
    /* blueprints */ \
    X(S_OPT_TOOL_BP, "bp.tool", "Blueprints") \
    X(S_ACT_BP, "bp.key", "Blueprint mode on / off") \
    X(S_ACT_BP_SUB, "bp.key_sub", "Blueprint mode: save <-> place") \
    X(S_OPT_MODE_BP, "bp.mode", "Blueprint") \
    X(S_BP_SUB_NAME, "bp.sub_name", "Blueprint") \
    X(S_BP_SUB_SAVE, "bp.sub_save", "Save") \
    X(S_BP_SUB_PLACE, "bp.sub_place", "Place") \
    X(S_MODE_BP, "bp.mode_on", "Blueprint mode ({0}): click a block") \
    X(S_HUD_BP, "bp.hud", "Blueprint mode") \
    X(S_HUD_BP_SAVE, "bp.hud_save", "Click: area to save") \
    X(S_HUD_BP_PLACE, "bp.hud_place", "Click: pick a blueprint") \
    X(S_BP_SAVED, "bp.saved", "Blueprint saved: {0} ({1} blocks, {2} materials)") \
    X(S_BP_EMPTY, "bp.empty", "Nothing to save there: only building blocks are saved") \
    X(S_BP_WRITE_FAIL, "bp.write_fail", "Could not write the blueprint file") \
    X(S_BP_REMOTE, "bp.remote", "Blueprints work only in a world you host (not yet on a dedicated server)") \
    X(S_BP_CANCELLED, "bp.cancelled", "Blueprint cancelled") \
    X(S_BP_SHAPE, "bp.shape", "Hold a block of the blueprint's size ({0} x {1} x {2} m) to build it") \
    X(S_BP_NOT_ENOUGH, "bp.not_enough", "Not enough materials for the whole blueprint (Allow partial build is off)") \
    X(S_BP_NO_MATERIAL, "bp.no_material", "Materials of this blueprint the game does not know: {0} (their blocks are left out)") \
    X(S_BP_NOTHING, "bp.nothing", "Nothing to build: no material of this blueprint could be used") \
    X(S_BP_SAVE_TITLE, "bp.save_title", "Save blueprint") \
    X(S_BP_SAVE_HELP, "bp.save_help", "The clicked block is the reference. Blocks in the drawn box are saved (only building blocks).") \
    X(S_BP_NAME, "bp.name", "Name: {0}") \
    X(S_BP_NAME_HINT, "bp.name_hint", "Click the name to type it; Enter or another click ends typing (empty = date and time).") \
    X(S_BP_CLEAR_NAME, "bp.clear_name", "Clear name") \
    X(S_BP_SAVE_BTN, "bp.save_button", "Save") \
    X(S_BP_CELLS, "bp.cells", "Cells of {0} m (the shape in your hand): {1} x {2} x {3}") \
    X(S_BP_LIST_TITLE, "bp.list_title", "Blueprints") \
    X(S_BP_NONE, "bp.none", "No blueprints yet. Switch Blueprint to Save on the tools wheel, click a block and save one.") \
    X(S_BP_BAD_FILE, "bp.bad_file", "This file is not a blueprint this version can read") \
    X(S_BP_SIZE, "bp.size", "Size {0} x {1} x {2} cells, {3} blocks") \
    X(S_BP_ROTATION, "bp.rotation", "Rotation") \
    X(S_BP_PARTIAL, "bp.partial", "Allow partial build") \
    X(S_BP_REF, "bp.reference", "Point you look at") \
    X(S_BP_FOLLOWING, "bp.following", "The ghost follows where you look: hold the right mouse button and aim, then Place here.") \
    X(S_BP_PLACED, "bp.placed", "Placed: move it with the rows below, then Build.") \
    X(S_BP_PLACE_BTN, "bp.place_here", "Place here") \
    X(S_BP_FOLLOW_BTN, "bp.follow", "Follow the aim") \
    X(S_BP_MOVE, "bp.move", "Move the ghost (blocks; negative = left, closer, down):") \
    X(S_BP_RIGHT, "bp.right", "Right") \
    X(S_BP_AWAY, "bp.away", "Away") \
    X(S_BP_UP, "bp.up", "Up") \
    X(S_BP_BACK, "bp.back", "Back to the list") \
    X(S_BP_MOVE_POINT, "bp.move_point", "Move the clicked point (blocks; negative = left, closer, down):") \
    X(S_BP_SIZE_HDR, "bp.size_header", "Size (blocks past the reference point):") \
    X(S_BP_NOT_ENOUGH_ANY, "bp.not_enough_any", "Not enough materials for any block of this blueprint") \
    X(S_BP_SAVED_OBJ, "bp.saved_obj", "Blueprint saved: {0} ({1} blocks, {2} materials, {3} objects)") \
    X(S_BP_SAVING, "bp.saving", "Saving the blueprint: looking for objects...") \
    X(S_BP_OBJ_NONE_HERE, "bp.obj_none_here", "Objects not saved: save inside a base (the game finds them through its comfort check)") \
    X(S_BP_OBJ_COUNT, "bp.obj_count", "Objects ({0})") \
    X(S_BP_OBJ_BUILD, "bp.obj_build", "Build") \
    X(S_BP_OBJ_LEAVE, "bp.obj_leave", "Leave out") \
    X(S_BP_OBJ_ROW, "bp.object_row", "Object {0}: {1} needed, you have {2}") \
    X(S_BP_OBJ_MISSING, "bp.obj_missing", "Objects the game does not know: {0} (left out)") \
    X(S_BP_OBJ_LOGIC, "bp.obj_logic", "Logic objects (wirable: switches, lamps...) are left out for now: {0}") \
    X(S_BP_MIRROR, "bp.mirror", "Mirror") \
    X(S_BP_MIRROR_OBJS, "bp.mirror_objs", "Mirrored objects: turn around any kind that comes out facing the wrong way.") \
    X(S_BP_MIRROR_ASIS, "bp.mirror_asis", "As is") \
    X(S_BP_MIRROR_TURN, "bp.mirror_turn", "Turned around") \
    X(S_BP_COSTS, "bp.costs", "Materials needed:") \
    X(S_BP_MAT_ROW, "bp.material_row", "Material {0}: {1} needed, you have {2}") \
    X(S_BP_MAT_UNKNOWN, "bp.have_unknown", "?") \
    X(S_BP_STOCK_HINT, "bp.stock_hint", "? = not known yet: open the material picker once") \
    X(S_BP_SHORT, "bp.short", "Not enough of {0} materials: only part can be built") \
    X(S_BP_OBJECTS_LATER, "bp.objects_later", "Objects are saved too: what you can use (doors, chests, beds, stations), what gives comfort (furniture, lights) and ladders.") \
    /* exact rotation and nudge of the object in hand (props.h) */ \
    X(S_PROP_TITLE, "obj.xf_title", "Exact rotation and nudge (also when you join a server)") \
    X(S_PROP_HELP, "obj.xf_help", "On top of the game's rotation. Keys on the Keys page; hold Ctrl for steps of 1.") \
    X(S_PROP_TURN, "obj.xf_turn", "Turn about its up axis (degrees)") \
    X(S_PROP_TILT, "obj.xf_tilt", "Tilt about its X axis (degrees)") \
    X(S_PROP_ROLL, "obj.xf_roll", "Roll about its Z axis (degrees)") \
    X(S_PROP_NUDGE_X, "obj.xf_nudge_x", "Nudge X (cm)") \
    X(S_PROP_NUDGE_Y, "obj.xf_nudge_y", "Nudge Y, up (cm)") \
    X(S_PROP_NUDGE_Z, "obj.xf_nudge_z", "Nudge Z (cm)") \
    X(S_PROP_AXES, "obj.xf_axes", "Nudge along") \
    X(S_PROP_AXES_OBJECT, "obj.xf_axes_object", "The object's axes (turned with it)") \
    X(S_PROP_AXES_WORLD, "obj.xf_axes_world", "The world's axes") \
    X(S_PROP_TURN_STEP, "obj.xf_turn_step", "One key press turns (degrees)") \
    X(S_PROP_NUDGE_STEP, "obj.xf_nudge_step", "One key press nudges (cm)") \
    X(S_PROP_RESET, "obj.xf_reset", "Reset rotation and nudge") \
    X(S_PROP_RESET_MSG, "obj.xf_reset_msg", "Object rotation and nudge reset") \
    X(S_PROP_FLOAT_ROT, "obj.xf_float_rot", "Turn {0}, tilt {1}, roll {2} degrees") \
    X(S_PROP_FLOAT_NUDGE, "obj.xf_float_nudge", "nudge {0} / {1} / {2} cm") \
    X(S_ACT_PROP_TURN_P, "obj.key_turn_plus", "Object in hand: turn +") \
    X(S_ACT_PROP_TURN_M, "obj.key_turn_minus", "Object in hand: turn -") \
    X(S_ACT_PROP_TILT_P, "obj.key_tilt_plus", "Object in hand: tilt +") \
    X(S_ACT_PROP_TILT_M, "obj.key_tilt_minus", "Object in hand: tilt -") \
    X(S_ACT_PROP_ROLL_P, "obj.key_roll_plus", "Object in hand: roll +") \
    X(S_ACT_PROP_ROLL_M, "obj.key_roll_minus", "Object in hand: roll -") \
    X(S_ACT_PROP_X_P, "obj.key_nudge_x_plus", "Object in hand: nudge right") \
    X(S_ACT_PROP_X_M, "obj.key_nudge_x_minus", "Object in hand: nudge left") \
    X(S_ACT_PROP_Y_P, "obj.key_nudge_up", "Object in hand: nudge up") \
    X(S_ACT_PROP_Y_M, "obj.key_nudge_down", "Object in hand: nudge down") \
    X(S_ACT_PROP_Z_P, "obj.key_nudge_z_plus", "Object in hand: nudge away") \
    X(S_ACT_PROP_Z_M, "obj.key_nudge_z_minus", "Object in hand: nudge closer") \
    X(S_ACT_PROP_RESET, "obj.key_xf_reset", "Object in hand: reset rotation and nudge")

enum Str {
#define BT_STR_ID(id, key, en) id,
    BT_STRINGS(BT_STR_ID)
#undef BT_STR_ID
    S_COUNT
};

// The languages of the game's language menu: file code (BuildingTools-Languages\<code>.txt),
// name in the language itself, the game's LanguageId names (user_settings.json) and the
// Steam language names that select it.
struct LangInfo { const char* code; const char* name; const char* game[3]; const char* steam[3]; };
static const LangInfo kLanguages[] = {
    { "en",    "English",            { "En_Us", "En_Gb" },           { "english" } },
    { "de",    "Deutsch",            { "De_De" },                    { "german" } },
    { "fr",    "Français",           { "Fr_Fr", "Fr_Ca" },           { "french" } },
    { "it",    "Italiano",           { "It_It" },                    { "italian" } },
    { "ja",    "日本語 (ja)",        { "Ja_Jp" },                    { "japanese" } },
    { "ko",    "한국어 (ko)",        { "Ko_Kr" },                    { "koreana" } },
    { "pl",    "Polski",             { "Pl_Pl" },                    { "polish" } },
    { "pt-BR", "Português (Brasil)", { "Pt_Br", "Pt_Pt" },           { "brazilian", "portuguese" } },
    { "ru",    "Русский",            { "Ru_Ru" },                    { "russian" } },
    { "es",    "Español",            { "Es_Es", "Es_Mx", "Es_419" }, { "spanish", "latam" } },
    { "th",    "ไทย (th)",           { "Th_Th" },                    { "thai" } },
    { "tr",    "Türkçe",             { "Tr_Tr" },                    { "turkish" } },
    { "uk",    "Українська",         { "Uk_Ua" },                    { "ukrainian" } },
    { "zh-CN", "简体中文 (zh-CN)",   { "Zh_Cn" },                    { "schinese" } },
    { "zh-TW", "繁體中文 (zh-TW)",   { "Zh_Tw" },                    { "tchinese" } },
};
static const int kLanguageCount = sizeof kLanguages / sizeof kLanguages[0];
