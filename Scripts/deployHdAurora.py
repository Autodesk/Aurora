# Copyright 2022 Autodesk, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.'
import os
import shutil
import sys
import pathlib
import glob
import argparse
import pathlib
import subprocess

programDescription = """
Deploy HdAurora to the provided USD folder (will build a new USD install if build argument provided)
"""

parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=programDescription)

parser.add_argument("usd_root", type=str,
                    help="The USD root to deploy hdAurora into.")
parser.add_argument("-a", "--aurora_root", type=str, dest="aurora_root", action="store", default=".",
                    help="The root of the Aurora Git repo.")
parser.add_argument("-c", "--config",  type=str,dest="config", action="store", default="Release",
                    help="The Aurora configuration to deploy.")
parser.add_argument("-f", "--build_folder", dest="aurora_cmake_build_folder", action="store", default="Build",
                    help="The Aurora CMake build folder to deploy from.")
parser.add_argument("-e", "--externals_folder", dest="externals_folder", action="store", default="~/ADSK/AuroraExternals",
                    help="Externals folder used to build Aurora and USD (if --build specified).")
parser.add_argument("-b", "--build", dest="build", action="store_true", default=0,
                    help="Build Aurora and USD before deploying.")

args = parser.parse_args()

    
# Get the USD target folders.
usd_bin_folder = os.path.join(args.usd_root,"bin")
usd_lib_folder = os.path.join(args.usd_root,"lib")
usd_plugin_folder = os.path.join(args.usd_root,"plugin","usd")
usd_hdaurora_resource_folder = os.path.join(usd_plugin_folder,"hdAurora","resources")
usd_mtlx_folder = os.path.join(usd_lib_folder,"MaterialX")

# Get the Aurora source folders.
aurora_build_folder = os.path.join(args.aurora_root, args.aurora_cmake_build_folder)
aurora_build_bin_folder = os.path.join(aurora_build_folder ,"bin",args.config)
aurora_resource_folder = os.path.join(args.aurora_root, "Libraries", "hdAurora", "resources")
aurora_mtlx_folder = os.path.join(aurora_build_bin_folder, "MaterialX")

# If the build arg is set, then build Aurora and USD before deploying.
if(args.build):
    externals_path = pathlib.Path(args.externals_folder)
    externals_folder = str(externals_path.expanduser())
    # find usd src folder : e.g. USD-22.08-Aurora-v22.11
    usd_paths = [f.name for f in os.scandir(os.path.join(externals_folder,"src")) if f.is_dir() and f.name.startswith("USD-")]
    if (len(usd_paths)==0):
        raise Exception("[Error] Fail to find usd folder.")
    if (len(usd_paths)>1):
        print("[Warning] More than one usd folders, use {}.".format(usd_paths[0]))
    usd_repo_root = os.path.join(externals_folder,"src", usd_paths[0])
    print("Build flag is set, building Aurora in "+aurora_build_folder+" and building+installing USD from "+usd_repo_root+" into "+args.usd_root+" (With externals from "+externals_folder+")")
    print("- Installing externals.")
    if(subprocess.run(["python","Scripts/installExternals.py",externals_folder],cwd=args.aurora_root).returncode!=0):
        sys.exit("Failed to install externals")
    print("- Running Cmake.")
    if(subprocess.run(["cmake","-S",args.aurora_root,"-B",aurora_build_folder,"-D","EXTERNALS_DIR="+externals_folder,"-D","CMAKE_BUILD_TYPE="+args.config],cwd=args.aurora_root).returncode!=0):
        sys.exit("Failed to run Cmake")    
    print("- Building Aurora.")
    if(subprocess.run(["cmake","--build",aurora_build_folder,"--config",args.config],cwd=args.aurora_root).returncode!=0):
        sys.exit("Failed to build Aurora")        
    print("- Building USD.")
    if(subprocess.run(["python","build_scripts/build_usd.py","--python",args.usd_root],cwd=usd_repo_root).returncode!=0):
        sys.exit("Failed to build USD")

# Copy a file if it has changed.
def copy_if_changed(src, dst):
    if(not os.path.exists(src)):
        print("File not found "+src)
        return
    if (not os.path.exists(dst)) or (os.stat(src).st_mtime != os.stat(dst).st_mtime):
        print("Copying "+src+" to "+dst)
        shutil.copy2 (src, dst)
        
# Copy a file from src_folder to dst_folder if it has changed.
def copy_file(src_folder,dst_folder,name):
    copy_if_changed(os.path.join(src_folder,name), os.path.join(dst_folder,name))

# Create target folders if they don't exist.
os.makedirs(usd_bin_folder, exist_ok=True)
os.makedirs(usd_lib_folder, exist_ok=True)
os.makedirs(usd_plugin_folder, exist_ok=True)
os.makedirs(usd_hdaurora_resource_folder, exist_ok=True)

# Copy plugin JSON.
copy_file(aurora_resource_folder,usd_hdaurora_resource_folder,"plugInfo.json")

# Copy hdAurora and PDB (if it exists)
copy_file(aurora_build_bin_folder,usd_plugin_folder,"hdAurora.dll")
copy_file(aurora_build_bin_folder,usd_plugin_folder,"hdAurora.pdb")

# Copy aurora and PDB (if it exists)
copy_file(aurora_build_bin_folder,usd_lib_folder,"Aurora.dll")
copy_file(aurora_build_bin_folder,usd_lib_folder,"Aurora.pdb")

# Copy compiler DLLs.
copy_file(aurora_build_bin_folder,usd_lib_folder,"d3dcompiler_47.dll")
copy_file(aurora_build_bin_folder,usd_lib_folder,"dxcompiler.dll")
copy_file(aurora_build_bin_folder,usd_lib_folder,"dxil.dll")
copy_file(aurora_build_bin_folder,usd_lib_folder,"slang.dll")
copy_file(aurora_build_bin_folder,usd_lib_folder,"msvcp140.dll")
copy_file(aurora_build_bin_folder,usd_lib_folder,"glew32.dll")

# Copy MtlX library folder.
if(not os.path.exists(usd_mtlx_folder)):
    os.makedirs(aurora_mtlx_folder, exist_ok=True)
    mtlx_files = glob.glob(os.path.join(aurora_mtlx_folder, "**"), recursive=True)
    for mtlx_file in mtlx_files:
        usd_mtlx_file = mtlx_file.replace(aurora_mtlx_folder,usd_mtlx_folder)
        if(os.path.isfile(mtlx_file)):
            copy_if_changed(mtlx_file,usd_mtlx_file)
        else:
            os.makedirs(usd_mtlx_file, exist_ok=True)
