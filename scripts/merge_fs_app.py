# scripts/auto_merge_after_build.py
import os, csv, time
from SCons.Script import Import

Import("env")  # PlatformIO provides 'env'

def _chip_arg():
    mcu = (env.BoardConfig().get("build.mcu") or "").lower()
    return mcu if mcu in ("esp32","esp32s2","esp32s3","esp32c3") else "esp32"

def _parse_partitions_offsets(csv_path):
    """Retourne les offsets app_off et fs_off (hex string) depuis partitions.csv."""
    app_candidates = []
    fs_off = None

    def to_hex(s):
        s = (s or "").strip().lower()
        if not s:
            return None
        try:
            return hex(int(s, 0))
        except Exception:
            if s.startswith("0x"): return s
            raise

    with open(csv_path, newline='') as f:
        rd = csv.reader(f)
        for row in rd:
            if not row or row[0].strip().startswith('#'):
                continue
            cols = [c.strip() for c in row] + [""]*6
            name, ptype, subtype, offset = cols[0], cols[1].lower(), cols[2].lower(), cols[3]
            if not offset:
                continue
            off = to_hex(offset)
            if ptype == "app" and subtype in ("factory","ota_0"):
                app_candidates.append((int(off,16), off))
            if ptype == "data" and subtype in ("spiffs","littlefs"):
                fs_off = off

    if not app_candidates:
        # fallback: première partition 'app' la plus basse
        with open(csv_path, newline='') as f:
            rd = csv.reader(f)
            for row in rd:
                if not row or row[0].strip().startswith('#'):
                    continue
                cols = [c.strip() for c in row] + [""]*6
                ptype, offset = cols[1].lower(), cols[3]
                if ptype == "app" and offset:
                    off = to_hex(offset)
                    app_candidates.append((int(off,16), off))

    if not app_candidates:
        raise RuntimeError("Offset APP introuvable dans partitions.csv")

    if not fs_off:
        raise RuntimeError("Partition FS (spiffs/littlefs) introuvable dans partitions.csv")

    app_off = min(app_candidates)[1]
    return app_off, fs_off

def _ensure_buildfs(build_dir):
    """Construit l'image FS si absente ou plus vieille que le contenu data/."""
    # PIO met littlefs.bin ou spiffs.bin dans BUILD_DIR
    fs_img_little = os.path.join(build_dir, "littlefs.bin")
    fs_img_spiffs = os.path.join(build_dir, "spiffs.bin")
    fs_img = fs_img_little if os.path.exists(fs_img_little) else fs_img_spiffs

    data_dir = os.path.join(env.subst("$PROJECT_DIR"), "data")
    need_buildfs = False

    if not os.path.isdir(data_dir):
        # pas de data/, rien à construire
        return fs_img if os.path.exists(fs_img) else None

    if not os.path.exists(fs_img):
        need_buildfs = True
    else:
        img_mtime = os.path.getmtime(fs_img)
        # si un fichier dans data/ est plus récent que l'image FS -> rebuild
        for root, _, files in os.walk(data_dir):
            for fn in files:
                if os.path.getmtime(os.path.join(root, fn)) > img_mtime:
                    need_buildfs = True
                    break

    if need_buildfs:
        print("==> Building FS image (buildfs)...")
        ret = env.Execute(f'"{env.subst("$PYTHONEXE")}" -m platformio run -t buildfs')
        if ret != 0:
            raise RuntimeError("Echec buildfs")
        # refresh path
        if os.path.exists(fs_img_little):
            fs_img = fs_img_little
        elif os.path.exists(fs_img_spiffs):
            fs_img = fs_img_spiffs
        else:
            raise RuntimeError("Image FS introuvable après buildfs")
    return fs_img

def _merge_bins(target, source, env):
    build_dir = env.subst("$BUILD_DIR")
    proj_dir  = env.subst("$PROJECT_DIR")
    chip      = _chip_arg()

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    app        = os.path.join(build_dir, env.subst("$PROGNAME") + ".bin")

    # image FS
    fsimg = _ensure_buildfs(build_dir)
    if not fsimg:
        print("==> Pas d'image FS à merger (pas de dossier data/).")
        return 0

    board = env.BoardConfig()
    part_csv_name = board.get("build.partitions", None)
    if not part_csv_name:
        part_csv_name = "partitions.csv"
    part_csv = os.path.join(proj_dir, part_csv_name)
    if not os.path.exists(part_csv):
        raise RuntimeError(f"Fichier partitions introuvable: {part_csv}")

    app_off, fs_off = _parse_partitions_offsets(part_csv)

    board = env.BoardConfig()
    mcu = (board.get("build.mcu") or "").lower()
    default_boot = "0x0" if mcu in ("esp32s2","esp32s3","esp32c3") else "0x1000"
    default_part = "0x8000"

    boot_off = board.get("build.bootloader_offset", default_boot)
    part_off = board.get("build.partitions_offset", default_part)

    out_path = os.path.join(build_dir, "firmware-combined.bin")

    esptool_path = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-esptoolpy", "esptool.py")

    cmd = (
        f'"{env.subst("$PYTHONEXE")}" "{esptool_path}" --chip {chip} merge_bin -o "{out_path}" '
        f'{boot_off} "{bootloader}" '
        f'{part_off} "{partitions}" '
        f'{app_off} "{app}" '
        f'{fs_off} "{fsimg}" '
    )
    print("==> Merging app + FS ->", out_path)
    print(cmd)
    ret = env.Execute(cmd)
    if ret == 0:
        print("==> OK:", out_path)
    return ret

# Brancher le merge en post-action du binaire appli
# Quand ${PROGNAME}.bin est produit, on lance le merge.
env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), _merge_bins)
