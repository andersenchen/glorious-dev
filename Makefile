# Makefile for Glorious Project

# Extract version dynamically from pyproject.toml
VERSION := $(shell grep '^version =' pyproject.toml | sed 's/version = "\(.*\)"/\1/')

# Detect OS and set OS-specific variables
ifeq ($(OS),Windows_NT)
  PYTHON := python
  MKDIR := mkdir
  # Use appropriate commands for Windows
  # 'del /S /Q' deletes files, 'rmdir /S /Q' deletes directories
  RM_FILES := del /S /Q
  RM_DIR := rmdir /S /Q
  SEP := /
  POETRY_INSTALL := pip install poetry
  PYTHON_INSTALL := @echo "Please install Python manually on Windows."
else
  PYTHON := python3
  MKDIR := mkdir -p
  RM := rm -rf
  SEP := /
  POETRY_INSTALL := pip3 install poetry
  PYTHON_INSTALL := sudo apt-get update && sudo apt-get install -y python3 python3-pip

  # Detect Unix-like OS
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    PYTHON_INSTALL := brew install python
  endif
endif

# Git command variable
GIT := git

# Directories
SRCDIR := src$(SEP)glorious$(SEP)c$(SEP)src
INCDIR := src$(SEP)glorious$(SEP)c$(SEP)include
BINDINGS_DIR := src$(SEP)glorious$(SEP)bindings
TESTDIR := tests
INTEGRATION_TESTDIR := integration_tests
DISTDIR := dist

PYTHON_MODULE_NAME := glorious
EXAMPLES_DIR := examples
COMPRESS_IMAGE_SCRIPT := $(EXAMPLES_DIR)/compress_image.py

# Backup branch name
BACKUP_BRANCH := backup-before-sync

# Targets
.PHONY: all clean install wheel test unit_tests integration_tests deps \
        valgrind-python leaks-python run_example \
        check-poetry check-lock update-lock build_ext tree ship

# Default target
all: check-python check-poetry check-lock build_ext install test

# Ensure Python is installed
check-python:
	@command -v $(PYTHON) >/dev/null 2>&1 || { \
		echo >&2 "Python is not installed. Installing Python..."; \
		$(PYTHON_INSTALL); \
	}

# Ensure Poetry is installed
check-poetry: check-python
	@command -v poetry >/dev/null 2>&1 || { \
		echo >&2 "Poetry is not installed. Installing Poetry..."; \
		$(POETRY_INSTALL); \
	}

# Check if poetry.lock is consistent with pyproject.toml
check-lock: check-poetry
	@echo "Checking if poetry.lock is consistent with pyproject.toml..."
	@poetry check > /dev/null 2>&1 || { \
		echo >&2 "poetry.lock is not consistent with pyproject.toml. Regenerating poetry.lock..."; \
		poetry lock --no-update || { echo >&2 "Failed to regenerate poetry.lock."; exit 1; } \
	}

# Build C extension
build_ext: check-poetry check-lock
	@echo "Installing setuptools..."
	@poetry run pip install setuptools
	@echo "Building C extension in-place..."
	@poetry run $(PYTHON) setup.py build_ext --inplace --verbose
	@echo "C extension built successfully."

# Install dependencies using Poetry
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

# Clean Build Artifacts
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
	# Windows doesn't support wildcard deletion in the same way; might need additional tools or scripts
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

	@echo "Clean complete."

# Install system dependencies (conditional based on OS)
deps:
ifeq ($(OS),Windows_NT)
	@echo "No system-level dependencies specified for Windows."
else
ifeq ($(UNAME_S),Darwin)
	@echo "No system-level dependencies required for macOS."
else
	@echo "Installing system-level dependencies for Linux..."
	@sudo apt-get update
	@sudo apt-get install -y valgrind
	@echo "System dependencies installed."
endif
endif

# Run Valgrind on Python Extension (Linux only)
valgrind-python: deps install
ifeq ($(OS),Windows_NT)
	@echo "Valgrind is not available on Windows."
else
ifeq ($(UNAME_S),Darwin)
	@echo "Valgrind is not available on macOS. Skipping Valgrind checks."
else
	@echo "Running Python with Valgrind to check for memory leaks..."
	@valgrind --leak-check=full --track-origins=yes --show-reachable=yes --error-exitcode=1 $(PYTHON) -c "import $(PYTHON_MODULE_NAME)"
endif
endif

# Run leaks on Python Extension (macOS only)
leaks-python: install
ifeq ($(UNAME_S),Darwin)
	@echo "Running Python with leaks to check for memory leaks..."
	@leaks --atExit -- $(PYTHON) -c "import $(PYTHON_MODULE_NAME)"
else
	@echo "The leaks command is only available on macOS. Skipping leaks checks."
endif

# Autocommit changes using AI-generated commit message
ship: check-poetry
	@echo "Running autocommit..."
	@git add -A
	@poetry run ai_commit

# Run tests using Tox in parallel
test: clean	install
	@echo "Running all tests using Tox in parallel..."
	@poetry run tox --parallel auto
	@echo "All tests passed successfully."

# Run unit tests using Tox
unit_tests:
	@echo "Running unit tests using Tox..."
	@poetry run tox -e py311
	@echo "Unit tests passed successfully."

# Run integration tests using Tox
integration_tests:
	@echo "Running integration tests using Tox..."
	@poetry run tox -e integration
	@echo "Integration tests passed successfully."

# Run compress_image.py script
run_example: install
	@echo "Installing necessary dependencies for compress_image.py..."
	@poetry install --extras "examples" --no-interaction --no-ansi
	@echo "Dependencies installed."
	@echo "Running compress_image.py..."
	@poetry run $(PYTHON) $(COMPRESS_IMAGE_SCRIPT)

# New target for tree command
tree:
	@echo "Generating tree structure..."
	@tree -a -I '.git|dist|*.egg-info|build|.venv|__pycache__|*.py[cod]|*.so|*.o|*.pyd|*.dll' --gitignore -f -s -h --si

# Update poetry.lock without updating dependencies
update-lock:
	@echo "Updating poetry.lock without updating dependencies..."
	@poetry lock --no-update
	@echo "poetry.lock updated."
