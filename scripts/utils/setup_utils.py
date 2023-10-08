import os
import subprocess
import logging

#################################### logging related #####################################
logging.basicConfig(
    format="%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s (%(name)s)]: \t%(message)s",
    level=logging.INFO,
    datefmt="%H:%M:%S",
)

logger = logging.getLogger("setup_utils")

BASEPATH = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../_build")
)
CACHESIM_PATH = os.path.join(BASEPATH, "bin/cachesim")


def install_dependency():
    p = subprocess.run(
        "bash {}/../scripts/install_dependency.sh".format(BASEPATH),
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )

    if p.stderr.decode() != "":
        print(p.stderr.decode())


def compile_cachesim():
    p = subprocess.run(
        "cd {}/../scripts/ && bash install_libcachesim.sh".format(BASEPATH),
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )

    if p.stderr.decode() != "":
        print(p.stderr.decode())


def setup():
    if os.path.exists(CACHESIM_PATH):
        return

    logger.info("cannot find {}, compiling...".format(CACHESIM_PATH))

    install_dependency()
    compile_cachesim()


if not os.path.exists(BASEPATH):
    os.makedirs(BASEPATH)

logger.info("BASEPATH: {}".format(BASEPATH))
logger.info("CACHESIM_PATH: {}".format(CACHESIM_PATH))

setup()
