import os
import sys
import sysconfig

import setuptools
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


# Function to extract library names from linker flags (for Unix-like systems)
def get_libraries():
    if sys.platform == "win32":
        return ["python" + sysconfig.get_config_var("py_version_nodot")]
    else:
        python_lib = sysconfig.get_config_var("LIBRARY")
        if python_lib:
            # Strip 'lib' prefix and extension
            if python_lib.startswith("lib"):
                python_lib = python_lib[3:]
            python_lib = os.path.splitext(python_lib)[0]
            return [python_lib]
        return []


# Define platform-specific macros and flags
extra_compile_args = []
extra_link_args = []
include_dirs = ["src/glorious/c/include", sysconfig.get_path("include")]
library_dirs = []

# Common compiler flags
common_compile_args = [
    "-fPIC",
    "-std=c11",
    "-O3",
    "-flto",
    "-fvisibility=hidden",
]
common_link_args = ["-flto"]

# Common warning flags for Unix-like systems
common_warning_flags = ["-Wall", "-Wextra"]

if sys.platform == "darwin":
    # macOS-specific settings
    python_lib = sysconfig.get_config_var("LIBDIR")
    if python_lib:
        extra_link_args += ["-L" + python_lib]
    extra_link_args += ["-undefined", "dynamic_lookup"]

    # Compiler flags for macOS (assuming GCC/Clang)
    extra_compile_args += common_compile_args + common_warning_flags
    extra_link_args += common_link_args

elif sys.platform == "win32":
    # Windows-specific settings
    python_lib = sysconfig.get_config_var("LIBDIR")
    if python_lib:
        library_dirs.append(python_lib)
        extra_link_args += [f"/LIBPATH:{python_lib}"]

    # Compiler flags for MSVC
    extra_compile_args += ["/O2", "/W3", "/EHsc", "/GL"]
    extra_link_args += ["/LTCG"]  # Link-time code generation

else:
    # Linux and other Unix-like systems
    extra_link_args += ["-shared"]

    # Compiler flags for GCC/Clang on Unix-like systems
    extra_compile_args += common_compile_args + common_warning_flags
    extra_link_args += common_link_args

    # Optionally, add position-independent code if not already included
    if "-fPIC" not in extra_compile_args:
        extra_compile_args.append("-fPIC")

    # Visibility settings to hide symbols by default
    extra_compile_args += ["-fvisibility=hidden"]

# Define the C extension module
module = Extension(
    "glorious",  # This is the desired import name
    sources=[
        "src/glorious/bindings/arithmetic_coding_bindings.c",
        "src/glorious/c/src/arithmetic_coding.c",
        "src/glorious/c/src/probability.c",
        "src/glorious/c/src/utilities.c",
    ],
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=get_libraries(),
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
    language="c",
)


# Custom build_ext command to set rpath on macOS and handle compiler-specific flags
class CustomBuildExt(build_ext):
    def build_extension(self, ext):
        if sys.platform == "darwin":
            python_lib = sysconfig.get_config_var("LIBDIR")
            if python_lib:
                ext.extra_link_args += ["-Wl,-rpath," + python_lib]

        # Handle compiler-specific flags if necessary
        compiler = self.compiler.compiler_type
        if compiler == "msvc":
            # MSVC-specific configurations can be added here if needed
            pass
        else:
            # For GCC/Clang, you might add more flags or checks
            pass

        super().build_extension(ext)


# Setup configuration
setup(
    name="glorious",
    version="1.0.0",
    description="Fast bitwise arithmetic coding in C with Python bindings.",
    ext_modules=[module],
    cmdclass={"build_ext": CustomBuildExt},
    python_requires=">=3.10",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: C",
        "Operating System :: OS Independent",
    ],
    zip_safe=False,
)
