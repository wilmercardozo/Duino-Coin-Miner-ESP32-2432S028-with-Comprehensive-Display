Import("env")
import os, shutil, hashlib

def _file_hash(path):
    with open(path, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()

def copy_lv_conf(source, target, env):
    src = os.path.join(env["PROJECT_INCLUDE_DIR"], "lv_conf.h")
    dst = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "lvgl", "lv_conf.h")
    if not os.path.exists(src):
        return
    if not os.path.exists(dst) or _file_hash(src) != _file_hash(dst):
        shutil.copy2(src, dst)
        print("[lv_conf] Copied lv_conf.h -> {}".format(dst))
    else:
        print("[lv_conf] lv_conf.h up to date, skipping copy")

env.AddPreAction("buildprog", copy_lv_conf)

# ---------------------------------------------------------------------------
# Force LVGL sources into the build.
# PlatformIO 6.x sometimes skips compiling C-only libraries from lib_deps.
# We collect all .c files manually and add them with correct per-file
# include paths (LVGL uses relative includes from each file's own directory).
# ---------------------------------------------------------------------------
_LVGL_EXCL_DIRS = {
    "drivers", "bmp", "lodepng", "gif", "qrcode", "barcode",
    "rlottie", "thorvg", "ffmpeg", "freetype", "tiny_ttf",
    "fsdrv", "tjpgd", "dma2d", "nxp", "renesas", "vg_lite",
    "eve", "sdl", "helium", "arm2d", "neon", "espressif",
}

def _should_skip(path, lvgl_src):
    """Return True if this path is in an excluded directory."""
    rel = os.path.relpath(path, lvgl_src)
    parts = set(rel.replace("\\", "/").split("/"))
    return bool(parts & _LVGL_EXCL_DIRS)

def _add_lvgl_sources(env):
    lvgl_root = os.path.join(
        env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "lvgl"
    )
    lvgl_src = os.path.join(lvgl_root, "src")
    if not os.path.isdir(lvgl_src):
        print("[lvgl] WARNING: lvgl/src not found at {}".format(lvgl_src))
        return

    # ── Collect every subdirectory under lvgl/src as an include path ──────
    # LVGL uses relative headers; giving the compiler every dir allows all
    # relative includes to resolve regardless of compilation order.
    include_dirs = [lvgl_root, lvgl_src]
    for root, dirs, _ in os.walk(lvgl_src):
        # Prune excluded subtrees so we don't add them to includes either
        dirs[:] = [d for d in dirs if d not in _LVGL_EXCL_DIRS]
        include_dirs.append(root)

    env.Append(CPPPATH=include_dirs)

    # ── Collect .c source files ───────────────────────────────────────────
    c_files = []
    for root, dirs, files in os.walk(lvgl_src):
        dirs[:] = [d for d in dirs if d not in _LVGL_EXCL_DIRS]
        for fname in files:
            if fname.endswith(".c"):
                c_files.append(os.path.join(root, fname))

    if not c_files:
        print("[lvgl] WARNING: no .c files found")
        return

    print("[lvgl] Registering {} source files".format(len(c_files)))

    # Build each .c file into an object and add it to the firmware link.
    objs = []
    for c_file in c_files:
        rel = os.path.relpath(c_file, lvgl_root)
        obj_path = os.path.join("$BUILD_DIR", "lvgl_src",
                                rel.replace("\\", "/") + ".o")
        obj = env.Object(obj_path, c_file)
        objs.append(obj)

    env.Append(PIOBUILDFILES=objs)

_add_lvgl_sources(env)
