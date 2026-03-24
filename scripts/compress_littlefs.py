Import("env")

print(">>> compress script loaded")

def before_buildfs(source, target, env):
    print(">>> running before_buildfs")

# Hook all possible filesystem build targets
env.AddPreAction("buildfs", before_buildfs)
env.AddPreAction("buildfs_littlefs", before_buildfs)
env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs)

# Import("env")

# print(">>> compress script loaded")
# import os
# import gzip
# import shutil

# DATA_DIR = "data"   # your LittleFS folder

# COMPRESS_EXTENSIONS = (".html", ".css", ".js")

# def compress_file(filepath):
#     gz_path = filepath + ".gz"

#     with open(filepath, 'rb') as f_in:
#         with gzip.open(gz_path, 'wb', compresslevel=9) as f_out:
#             shutil.copyfileobj(f_in, f_out)

#     print(f"Compressed: {filepath} -> {gz_path}")


# def clean_old_gz():
#     for root, _, files in os.walk(DATA_DIR):
#         for file in files:
#             if file.endswith(".gz"):
#                 full_path = os.path.join(root, file)
#                 os.remove(full_path)
#                 print(f"Removed old: {full_path}")


# def compress_all():
#     for root, _, files in os.walk(DATA_DIR):
#         for file in files:
#             if file.endswith(COMPRESS_EXTENSIONS):
#                 full_path = os.path.join(root, file)
#                 compress_file(full_path)


# def before_buildfs(source, target, env):
#     print("\n=== Compressing LittleFS files ===")
#     clean_old_gz()
#     compress_all()
#     print("=== Compression complete ===\n")

# env.AddPreAction("buildfs", before_buildfs)