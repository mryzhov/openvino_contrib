from setuptools.command.build_py import build_py as _build_py
import os.path
import sys
import multiprocessing
from pathlib import Path

PYTHON_VERSION = f"python{sys.version_info.major}.{sys.version_info.minor}"

# The following variables can be defined in environment or .env file
SCRIPT_DIR = Path(__file__).resolve().parents[0]
WORKING_DIR = Path.cwd()
BUILD_BASE = f"{WORKING_DIR}/build_{PYTHON_VERSION}"
OPENVINO_BUILD_DIR = os.getenv("OPENVINO_BUILD_DIR")
CONFIG = os.getenv("BUILD_TYPE", "Release")

class build_py(_build_py):
    def run(self):
        self.cmake_build()
        # self.run_command("build_ext")
        return super().run()

    def initialize_options(self):
        """Set default values for all the options that this command supports."""
        super().initialize_options()
        self.build_base = BUILD_BASE
        self.jobs = None
        self.cmake_args = None

    def finalize_options(self):
        """Set final values for all the options that this command supports."""
        super().finalize_options()

        if self.jobs is None and os.getenv("MAX_JOBS") is not None:
            self.jobs = os.getenv("MAX_JOBS")
        self.jobs = multiprocessing.cpu_count() if self.jobs is None else int(self.jobs)

        if self.cmake_args is None:
            self.cmake_args = ""

    def cmake_build(self):
        """Runs cmake (configure, build) if artfiacts are not already built"""
        self.build_temp = os.path.join(self.build_base, "temp")
        self.announce(f"Create build directory: {self.build_temp}", level=3)

        source_dir = '/home/rmikhail/src/openvino_contrib/modules/custom_operations/user_ie_extensions'
        binary_dir = 'bin'

        # even perform a build in case of binary directory does not exist
        binary_dir = binary_dir if os.path.isabs(binary_dir) else os.path.join(self.build_temp, binary_dir)
        if not os.path.exists(binary_dir):
            binary_dir = os.path.join(self.build_temp, binary_dir)
            self.announce(f"Configuring cmake project", level=3)
            self.spawn(["cmake", f"-DOpenVINO_DIR={OPENVINO_BUILD_DIR}",
                                    f"-DCMAKE_BUILD_TYPE={CONFIG}",
                                    '-DCUSTOM_OPERATIONS=tokenizers',
                                    self.cmake_args,
                                    "-S", source_dir,
                                    "-B", binary_dir])

            self.announce(f"Building project", level=3)
            self.spawn(["cmake", "--build", binary_dir,
                                 "--config", CONFIG,
                                 "--parallel", str(self.jobs)])

    # def initialize_options(self):
    #     super().initialize_options()
    #     if self.distribution.ext_modules == None:
    #         self.distribution.ext_modules = []

    #     self.distribution.ext_modules.append(

    #         Extension(
    #             "bs",
    #             sources=["black_scholes/bs.c"],
    #             extra_compile_args=["-std=c17", "-lm", "-Wl", "-c", "-fPIC"],
    #         )
    #     )