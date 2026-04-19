Import("env")
import os, shutil

def copy_lv_conf(source, target, env):
    src = os.path.join(env["PROJECT_INCLUDE_DIR"], "lv_conf.h")
    dst = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "lvgl", "lv_conf.h")
    if os.path.exists(src) and not os.path.exists(dst):
        shutil.copy2(src, dst)
        print("[lv_conf] Copied lv_conf.h -> {}".format(dst))

env.AddPreAction("buildprog", copy_lv_conf)
