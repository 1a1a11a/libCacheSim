
import os
import subprocess


BASEPATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../_build")
CACHESIM_PATH = os.path.join(BASEPATH, "/bin/cachesim")

if not os.path.exists(BASEPATH):
    os.makedirs(BASEPATH)

def install_dependency():
    subprocess.run("bash {}/../scripts/install_dependency.sh".format(BASEPATH), shell=True)
    
def compile_cachesim():
    subprocess.run("cd {}/../scripts/ && bash install_libcachesim.sh".format(BASEPATH), shell=True)

def setup():
    if os.path.exists(CACHESIM_PATH):
        return
    
    install_dependency()
    compile_cachesim()

