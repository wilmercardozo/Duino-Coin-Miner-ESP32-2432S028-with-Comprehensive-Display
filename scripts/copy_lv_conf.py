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
