# Makefile for Glorious Project

# =============================================================================
#                           Configuration Variables
# =============================================================================

# Extract the project version dynamically from pyproject.toml
VERSION := $(shell grep '^version =' pyproject.toml | sed 's/version = "\(.*\)"/\1/')

# Detect Operating System and set OS-specific variables
ifeq ($(OS),Windows_NT)
  # Windows-specific configurations
  PYTHON := python
  MKDIR := mkdir
  RM_FILES := del /S /Q
  RM_DIR := rmdir /S /Q
  SEP := /

  # Installation commands
  POETRY_INSTALL := pip install poetry
  PYTHON_INSTALL := @echo "Please install Python manually on Windows."
else
  # Unix-like systems configurations
  PYTHON := python3
  MKDIR := mkdir -p
  RM := rm -rf
  SEP := /

  # Installation commands
  POETRY_INSTALL := pip3 install poetry
  UNAME_S := $(shell uname -s)

  # Conditional Python installation based on Unix variant
  ifeq ($(UNAME_S),Darwin)
    PYTHON_INSTALL := brew install python
  else
    PYTHON_INSTALL := sudo apt-get update && sudo apt-get install -y python3 python3-pip
  endif
endif

# Git command variable
GIT := git

# Directory paths
SRCDIR := src$(SEP)glorious$(SEP)c$(SEP)src
INCDIR := src$(SEP)glorious$(SEP)c$(SEP)include
BINDINGS_DIR := src$(SEP)glorious$(SEP)bindings
TESTDIR := tests
INTEGRATION_TESTDIR := integration_tests
DISTDIR := dist
EXAMPLES_DIR := examples
COMPRESS_IMAGE_SCRIPT := $(EXAMPLES_DIR)/compress_image.py

# Backup branch name for version control
BACKUP_BRANCH := backup-before-sync

# C Compiler and Flags
CC := gcc
CFLAGS := -I$(INCDIR) -Wall -Wextra -O2
CBINDIR := build/c
COUTPUT := $(CBINDIR)/main_c_program

# C Source Files
CSOURCES := \
	src/glorious/c/src/main.c \
	src/glorious/c/src/arithmetic_coding.c \
	src/glorious/c/src/probability.c

# Python Module Name
PYTHON_MODULE_NAME := glorious

# Rust-specific variables
CARGO := cargo
RUST_SRCDIR := rust_code
ARITHMETIC_CODING_DEMO_DIR := arithmetic_coding_demo

# =============================================================================
#                                 Targets
# =============================================================================

# Declare phony targets to avoid conflicts with files of the same name
.PHONY: all clean install wheel test unit_tests integration_tests deps \
        valgrind-python leaks-python examples run_c \
        check-poetry check-lock update-lock build_ext tree ship \
        rust-check rust-clippy rust-fmt rust-test rust-build rust-all

# Default target: Executes a sequence of essential build steps
all: check-python check-poetry check-lock build_ext install test rust-all

# -------------------------------------------------------------------------
# Setup and Installation
# -------------------------------------------------------------------------

# Ensure Python is installed, install if missing
check-python:
	@command -v $(PYTHON) >/dev/null 2>&1 || { \
		echo >&2 "Python is not installed. Installing Python..."; \
		$(PYTHON_INSTALL); \
	}

# Ensure Poetry is installed, install if missing
check-poetry: check-python
	@command -v poetry >/dev/null 2>&1 || { \
		echo >&2 "Poetry is not installed. Installing Poetry..."; \
		$(POETRY_INSTALL); \
	}

# Check poetry.lock consistency with pyproject.toml and regenerate if necessary
check-lock: check-poetry
	@echo "Checking if poetry.lock is consistent with pyproject.toml..."
	@poetry check > /dev/null 2>&1 || { \
		echo >&2 "poetry.lock is not consistent with pyproject.toml. Regenerating poetry.lock..."; \
		poetry lock --no-update || { echo >&2 "Failed to regenerate poetry.lock."; exit 1; } \
	}

# Build the C extension in-place
build_ext: check-poetry check-lock
	@echo "Installing setuptools..."
	@poetry run pip install setuptools
	@echo "Building C extension in-place..."
	@poetry run $(PYTHON) setup.py build_ext --inplace --verbose
	@echo "C extension built successfully."

# Install project dependencies using Poetry
install: build_ext
	@echo "Installing project dependencies with Poetry..."
	@poetry install --no-interaction --no-ansi || { \
		echo >&2 "Poetry install failed. Attempting to regenerate poetry.lock..."; \
		poetry lock --no-update && poetry install --no-interaction --no-ansi || { \
			echo >&2 "Poetry install failed even after regenerating poetry.lock."; \
			exit 1; \
		} \
	}
	@echo "Dependencies installed successfully."

# Build Python Wheel using Poetry
wheel: build_ext install
	@echo "Building Python wheel with Poetry..."
	@poetry build --format wheel
	@echo "Wheel built successfully in dist/."

# -------------------------------------------------------------------------
# Cleaning
# -------------------------------------------------------------------------

# Clean build artifacts and temporary files
clean:
	@echo "Removing dist, build, and virtual environment directories..."
ifeq ($(OS),Windows_NT)
	@$(RM_DIR) $(DISTDIR) 2>nul || echo "dist directory not found."
	@$(RM_DIR) *.egg-info 2>nul || echo "*.egg-info directory not found."
	@$(RM_DIR) build 2>nul || echo "build directory not found."
	@$(RM_DIR) .venv 2>nul || echo ".venv directory not found."
else
	@$(RM) -r $(DISTDIR) *.egg-info build .venv
endif

	@echo "Removing C extension artifacts..."
ifeq ($(OS),Windows_NT)
	# Manual removal required for Windows
	@echo "Manual removal of .so/.pyd/.dll files may be required on Windows."
else
	@$(RM) $(SRCDIR)/*.so $(SRCDIR)/*.o 2>/dev/null || echo "No C extension artifacts found."
endif

	@echo "Removing compiled Python files and __pycache__ directories..."
ifeq ($(OS),Windows_NT)
	@for /r . %%d in (__pycache__) do @if exist "%%d" $(RM_DIR) "%%d"
	@for /r . %%f in (*.pyc *.pyo *.pyd) do @if exist "%%f" $(RM_FILES) "%%f"
else
	@find . -type d -name "__pycache__" -exec $(RM) {} + 2>/dev/null
	@find . -type f -name "*.py[cod]" -exec $(RM) {} + 2>/dev/null
endif

	@echo "Cleaning up additional log files or temporary files..."
ifeq ($(OS),Windows_NT)
	@$(RM_FILES) *.coverage *.log 2>nul || echo "No coverage or log files to remove."
else
	@$(RM) *.coverage *.log 2>/dev/null || echo "No coverage or log files to remove."
endif

	@echo "Removing C program executable..."
	@$(RM) $(COUTPUT) 2>/dev/null || echo "C program executable not found."

	@echo "Cleaning Rust build artifacts..."
	@(cd $(RUST_SRCDIR) && $(CARGO) clean)
	@(cd $(ARITHMETIC_CODING_DEMO_DIR) && $(CARGO) clean)
	@$(CARGO) clean

	@echo "Clean complete."

# -------------------------------------------------------------------------
# Dependencies
# -------------------------------------------------------------------------

# Install system-level dependencies based on the operating system
deps:
ifeq ($(OS),Windows_NT)
	@echo "No system-level dependencies specified for Windows."
else ifeq ($(UNAME_S),Darwin)
	@echo "No system-level dependencies required for macOS."
else
	@echo "Installing system-level dependencies for Linux..."
	@sudo apt-get update
	@sudo apt-get install -y valgrind
	@echo "System dependencies installed."
endif

# -------------------------------------------------------------------------
# Testing and Analysis
# -------------------------------------------------------------------------

# Run Valgrind to check for memory leaks in the Python extension (Linux only)
valgrind-python: deps install
ifeq ($(OS),Windows_NT)
	@echo "Valgrind is not available on Windows."
else ifeq ($(UNAME_S),Darwin)
	@echo "Valgrind is not available on macOS. Skipping Valgrind checks."
else
	@echo "Running Python with Valgrind to check for memory leaks..."
	@valgrind --leak-check=full --track-origins=yes --show-reachable=yes --error-exitcode=1 $(PYTHON) -c "import $(PYTHON_MODULE_NAME)"
endif

# Run leaks to check for memory leaks in the Python extension (macOS only)
leaks-python: install
ifeq ($(UNAME_S),Darwin)
	@echo "Running Python with leaks to check for memory leaks..."
	@leaks --atExit -- $(PYTHON) -c "import $(PYTHON_MODULE_NAME)"
else
	@echo "The leaks command is only available on macOS. Skipping leaks checks."
endif

# Run all tests using Tox in parallel across all supported Python versions
test: clean install
	@echo "Running all tests using Tox in parallel..."
	@poetry run tox --parallel auto
	@echo "All tests passed successfully."

# Run Python tests specifically on Python 3.11 using Tox
py311_tests:
	@echo "Running unit tests using Tox..."
	@poetry run tox -e py311
	@echo "Unit tests passed successfully."

# -------------------------------------------------------------------------
# Examples and Utilities
# -------------------------------------------------------------------------

# Run all example scripts from the examples directory
examples: install
	@echo "Preparing to run all example scripts..."
	@echo "Installing necessary dependencies for examples..."
	@poetry install --with "examples"  # Updated line
	@echo "Dependencies installed."
	@for script in $(EXAMPLES_DIR)/*.py; do \
		if [ -f "$$script" ]; then \
			echo "Running $$(basename $$script)..."; \
			poetry run $(PYTHON) "$$script" || { \
				echo "Error: Script $$(basename $$script) failed."; \
				exit 1; \
			}; \
			echo "Finished running $$(basename $$script)."; \
			echo "------------------------"; \
		fi; \
	done
	@echo "All example scripts have been executed."

# Compile and run the C program
run_c: $(COUTPUT)
	@echo "Running C program..."
	@$(COUTPUT)

# Build the C program executable
$(COUTPUT): $(CSOURCES) $(INCDIR)/arithmetic_coding.h $(INCDIR)/probability.h
	@echo "Compiling C program..."
	@$(MKDIR) $(CBINDIR)
	@$(CC) $(CFLAGS) -o $(COUTPUT) $(CSOURCES)
	@echo "C program compiled successfully."

# Generate a tree structure of the project, excluding specified directories/files
tree:
	@echo "Generating tree structure..."
	@tree -a -I '.git|dist|*.egg-info|build|.venv|__pycache__|*.py[cod]|*.so|*.o|*.pyd|*.dll|target' --gitignore -f -s -h --si

# -------------------------------------------------------------------------
# Version Control and Deployment
# -------------------------------------------------------------------------

# Automatically commit all changes using an AI-generated commit message and push to private/main
ship: install
	@poetry run $(PYTHON) scripts/autocommit.py --push-to-private --force

# Update poetry.lock without updating dependencies
update-lock:
	@echo "Updating poetry.lock without updating dependencies..."
	@poetry lock --no-update
	@echo "poetry.lock updated."

# -------------------------------------------------------------------------
# Rust-specific targets
# -------------------------------------------------------------------------

# Run 'cargo check' on all Rust projects
rust-check:
	@echo "Running 'cargo check' on all Rust projects..."
	@(cd $(RUST_SRCDIR) && $(CARGO) check)
	@(cd $(ARITHMETIC_CODING_DEMO_DIR) && $(CARGO) check)
	@$(CARGO) check

# Run Clippy on all Rust projects
rust-clippy:
	@echo "Running Clippy on all Rust projects..."
	@$(CARGO) clippy --fix --allow-dirty

# Format Rust code
rust-fmt:
	@echo "Formatting Rust code..."
	@$(CARGO) fmt --all

# Run Rust tests
rust-test:
	@echo "Running Rust tests..."
	@(cd $(RUST_SRCDIR) && $(CARGO) test)
	@(cd $(ARITHMETIC_CODING_DEMO_DIR) && $(CARGO) test)
	@$(CARGO) test

# Build Rust projects
rust-build:
	@echo "Building Rust projects..."
	@(cd $(RUST_SRCDIR) && $(CARGO) build)
	@(cd $(ARITHMETIC_CODING_DEMO_DIR) && $(CARGO) build)
	@$(CARGO) build

# Run all Rust-related tasks
rust-all: rust-check rust-clippy rust-fmt rust-test rust-build

# =============================================================================
# End of Makefile
# =============================================================================
