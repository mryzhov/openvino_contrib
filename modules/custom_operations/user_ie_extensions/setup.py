from skbuild import setup
from skbuild import constants

setup(
    packages=["ov_tokenizer"],
    package_dir={"": "src/tokenizer/python"},
    cmake_install_dir="src/lib",
    cmake_args=['-DCUSTOM_OPERATIONS:STRING=tokenizer']
)

# When building extension modules `cmake_install_dir` should always be set to the
# location of the package you are building extension modules for.
# Specifying the installation directory in the CMakeLists subtley breaks the relative
# paths in the helloTargets.cmake file to all of the library components.