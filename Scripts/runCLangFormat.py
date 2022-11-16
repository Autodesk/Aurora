import os
import shutil
import subprocess
import pathlib
import glob

root_folder = os.getcwd()
source_files = glob.glob(os.path.join(root_folder, "**","*.cpp"), recursive=True) + glob.glob(os.path.join(root_folder, "**","*.h"), recursive=True)
print("Running clang format on "+str(len(source_files))+" files....")
for source_file in source_files:
    mtime_before = os.path.getmtime(source_file) 
    subprocess.run(["C:/Program Files/LLVM/bin/clang-format.exe", "-style=file","-i", source_file])
    mtime_after = os.path.getmtime(source_file) 
    if(mtime_before!=mtime_after):
        print("Clang-format modified:"+source_file)
