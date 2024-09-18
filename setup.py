"""Setup script for the glorious package."""

import os
import sys
import sysconfig
from typing import Tuple

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext


def get_libraries() -> list[str]:
  """Extract library names from linker flags.

  Returns:
    A list of library names.
  """
  if sys.platform == "win32":
    return ["python" + sysconfig.get_config_var("py_version_nodot")]
  return []


def get_platform_specific_args() -> Tuple[list[str], list[str], list[str], list[str]]:
  """Define platform-specific macros and flags.

  Returns:
    A tuple containing extra_compile_args, extra_link_args, include_dirs,
    and library_dirs.
  """
  extra_compile_args: list[str] = []
  extra_link_args: list[str] = []
  include_dirs = [
      os.path.join("src", "glorious", "c", "include"),
      os.path.join("src", "glorious", "bindings"),
      sysconfig.get_path("include"),
  ]
  library_dirs: list[str] = []

  common_compile_args = ["-fPIC", "-std=c11",
                         "-O3", "-flto", "-fvisibility=hidden"]
  common_link_args = ["-flto"]
  common_warning_flags = ["-Wall", "-Wextra"]

  if sys.platform == "darwin":
    python_lib = sysconfig.get_config_var("LIBDIR")
    if python_lib:
      extra_link_args.append("-L" + python_lib)
    extra_link_args.extend(["-undefined", "dynamic_lookup"])
    extra_compile_args.extend(common_compile_args + common_warning_flags)
    extra_link_args.extend(common_link_args)
  elif sys.platform == "win32":
    python_lib = sysconfig.get_config_var("LIBDIR")
    if python_lib:
      library_dirs.append(python_lib)
      extra_link_args.append(f"/LIBPATH:{python_lib}")
    extra_compile_args.extend(["/O2", "/W3", "/EHsc", "/GL"])
    extra_link_args.append("/LTCG")
  else:
    extra_link_args.append("-shared")
    extra_compile_args.extend(common_compile_args + common_warning_flags)
    extra_link_args.extend(common_link_args)
    if "-fPIC" not in extra_compile_args:
      extra_compile_args.append("-fPIC")
    extra_compile_args.append("-fvisibility=hidden")

  return extra_compile_args, extra_link_args, include_dirs, library_dirs


def get_extension_module() -> Extension:
  """Define the C extension module.

  Returns:
    The Extension object for the C module.
  """
  extra_compile_args, extra_link_args, include_dirs, library_dirs = (
      get_platform_specific_args()
  )

  return Extension(
      "glorious._glorious",
      sources=[
          os.path.join("src", "glorious", "bindings",
                       "arithmetic_coding_bindings.c"),
          os.path.join("src", "glorious", "c", "src", "arithmetic_coding.c"),
          os.path.join("src", "glorious", "c", "src", "probability.c"),
          os.path.join("src", "glorious", "c", "src", "utilities.c"),
      ],
      include_dirs=include_dirs,
      library_dirs=library_dirs,
      libraries=get_libraries(),
      extra_compile_args=extra_compile_args,
      extra_link_args=extra_link_args,
      language="c",
  )


class CustomBuildExt(build_ext):
  """Custom build_ext command for macOS rpath and compiler-specific flags."""

  def build_extension(self, ext: Extension) -> None:
    """Build the extension with custom flags.

    Args:
      ext: The extension being built.
    """
    if sys.platform == "darwin":
      python_lib = sysconfig.get_config_var("LIBDIR")
      if python_lib:
        ext.extra_link_args.append("-Wl,-rpath," + python_lib)

    if self.compiler.compiler_type == "msvc":
      # MSVC-specific configurations can be added here if needed
      pass
    else:
      # For GCC/Clang, you might add more flags or checks
      pass

    super().build_extension(ext)


def read_readme() -> str:
  """Read the README file.

  Returns:
    The content of the README file.
  """
  here = os.path.abspath(os.path.dirname(__file__))
  with open(os.path.join(here, "README.md"), encoding="utf-8") as f:
    return f.read()


if __name__ == "__main__":
  setup(
      name="glorious",
      version="1.0.0",
      description="Python bindings for Arithmetic Coding with Context",
      long_description=read_readme(),
      long_description_content_type="text/markdown",
      author="Andy Chen",
      author_email="your.email@example.com",
      url="https://github.com/mister-brain/glorious",
      project_urls={
          "Documentation": "https://github.com/mister-brain/glorious/docs",
          "Repository": "https://github.com/mister-brain/glorious",
      },
      classifiers=[
          "Programming Language :: Python :: 3",
          "Programming Language :: C",
          "Operating System :: OS Independent",
          "License :: OSI Approved :: MIT License",
          "Topic :: Software Development :: Libraries",
      ],
      packages=find_packages(where="src"),
      package_dir={"": "src"},
      ext_modules=[get_extension_module()],
      cmdclass={"build_ext": CustomBuildExt},
      include_package_data=True,
      python_requires=">=3.8",
      install_requires=[
          "openai>=1.45.0",
          "pathspec>=0.12.1",
          "pygments>=2.17.0",
      ],
      extras_require={
          "dev": [
              "pytest>=8.3.3",
              "pre-commit>=2.20.0",
              "tox>=4.19.0",
          ],
          "examples": [
              "requests",
              "Pillow",
          ],
      },
      entry_points={
          "console_scripts": [
              "generate-tree=scripts.generate_tree:main",
              "ai_commit=scripts.autocommit:main",
          ],
      },
      zip_safe=False,
  )
