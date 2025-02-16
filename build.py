import os, sys, shutil
from typing import List

CC="gcc"
CFLAGS=" ".join([f"-I{i}" for i in [
]])

STATIC=[
    "assets/sprite_sheet.bmp",
    "assets/monomaniac.ttf"
]

INCLUDES=" ".join([f"-I{i}" for i in [
    "/usr/include/SDL2",
    "/usr/include/miniaudio",
    "lib/liteinput/src",
    "lib/tinyfiledialogs",
    "lib/microui/src",
    "static"
]])

LINKS=" ".join([
    "-lm","-levdev","-lxkbcommon","-lSDL2_ttf",
    "`sdl2-config --cflags --libs`"
])

SOURCE=[
    "lib/tinyfiledialogs/tinyfiledialogs.c",
    "lib/microui/src/microui.c",
    "src/gui.c", 
    "src/main.c"
]

OUTPUT="mollyplayback"
INSTALL_DIR="/usr/local/bin"

class COLORS:
    RED    = '\033[31m'
    GREEN  = '\033[32m'
    YELLOW = '\033[33m'
    BLUE   = '\033[34m'
    RESET  = '\033[0m'

def have_changed(in_files:List[str], out_file:str) -> bool:
    if(not os.path.exists(out_file)): return True
    out_path_date = os.path.getmtime(out_file)

    for f in in_files:
        if not os.path.exists(f): return True
        if os.path.getmtime(f) > out_path_date: return True
    return False

def create_dirs(routes:List[str]):
    for r in routes:
        if(not os.path.exists(r)):
            os.makedirs(r)

def rm_all(path:str):
    for f in os.listdir(path): os.remove(os.path.join(path,f))

def create_statics(static_dir:str,assets:List[str]):
    for a in assets:
        out = os.path.join(static_dir,a.split(os.sep)[-1].split(".")[0]+".h")
        if(not have_changed([a],out)):
            print(f"{out} {COLORS.BLUE}-> nothing to do!{COLORS.RESET}")
            continue

        og_var_name = a.replace(" ","_").replace(os.sep,"_").replace(".","_")
        new_var_name = a.split(os.sep)[-1].replace(" ","_").replace(".","_")

        command = f"xxd -i {a} | sed 's/{og_var_name}/{new_var_name}/g' > {out}"
        print(f"{COLORS.GREEN}COMMAND: {COLORS.RESET} {command}")
        if(os.system(command) != 0): sys.exit(1)
        
def compile(out_dir:str, sources:List[str]) -> List[str]:
    created_files = []
    for s in sources:
        out = os.path.join(out_dir,s.split(os.sep)[-1].replace(".c",".o"))
        if(not have_changed(s.split(" "),out)):
            print(f"{out} {COLORS.BLUE}-> nothing to do!{COLORS.RESET}")
            continue

        command = f"{CC} {CFLAGS} -c -o {out} {INCLUDES} {s}"
        print(f"{COLORS.GREEN}COMMAND: {COLORS.RESET} {command}")
        if(os.system(command) != 0): sys.exit(1)
        
        created_files.append(out)
    return created_files

def link(out_dir:str, out_name:str, sources:List[str]):
    out_name = os.path.join(out_dir,out_name)
    if(not have_changed(sources,out_name)):
        print(f"{out_name} {COLORS.BLUE}-> nothing to do!{COLORS.RESET}")
        return
    
    command = f"{CC} {CFLAGS} -o {out_name} {" ".join(sources)} {LINKS}"
    print(f"{COLORS.GREEN}COMMAND: {COLORS.RESET} {command}")
    if(os.system(command) != 0): sys.exit(1)


# ---------------------------------------- RULES ---------------------------------------- #

def make():
    create_dirs(["build","obj","static"])
    os.system("cd lib/liteinput && make")
    create_statics("static",STATIC)
    compile("obj",SOURCE)

    objs = [os.path.join("obj",d) for d in os.listdir("obj")]
    objs.append("lib/liteinput/obj/core.o")
    link("build",OUTPUT,objs)

def install():
    shutil.copy(f"build/{OUTPUT}",f"{INSTALL_DIR}/{OUTPUT}")

def uninstall():
    os.remove(f"{INSTALL_DIR}/{OUTPUT}")

def clean():
    rm_all("build")
    rm_all("obj")
    rm_all("static")
    os.system("cd lib/liteinput && make clear")

if __name__ == "__main__":
    if "install"     in sys.argv: make(); install()
    elif "uninstall" in sys.argv: uninstall()
    elif "clean"     in sys.argv: clean()
    elif "clear"     in sys.argv: clean()
    else: make()