# Detect OS and set OS-specific variables
ifeq ($(OS),Windows_NT)
  PYTHON := python
  BINDINGS_VENV := bindings_venv
  ACTIVATE_BINDINGS := $(BINDINGS_VENV)/Scripts/activate.bat
  MKDIR := mkdir
  RM := rm -rf  # Use rm -rf for Git Bash compatibility on Windows
  PIP := $(BINDINGS_VENV)/Scripts/pip.exe
  PYTHON_BINDINGS_VENV := $(BINDINGS_VENV)/Scripts/python.exe
  SEP := /
else
  PYTHON := python3
  BINDINGS_VENV := bindings_venv
  ACTIVATE_BINDINGS := source $(BINDINGS_VENV)/bin/activate
  MKDIR := mkdir -p
  RM := rm -rf
  PIP := $(BINDINGS_VENV)/bin/pip
  PYTHON_BINDINGS_VENV := $(BINDINGS_VENV)/bin/python
  SEP := /

  # Detect Unix-like OS
  UNAME_S := $(shell uname -s)
endif

# Directories
SRCDIR := src$(SEP)compression$(SEP)c$(SEP)src
INCDIR := src$(SEP)compression$(SEP)c$(SEP)include
BINDINGS_DIR := src$(SEP)compression$(SEP)bindings
TESTDIR := tests
DISTDIR := dist

PYTHON_MODULE_NAME := glorious
EXAMPLES_DIR := examples
COMPRESS_IMAGE_SCRIPT := $(EXAMPLES_DIR)/compress_image.py

# Targets
.PHONY: all clean install wheel test deps valgrind-python leaks-python ac run_example

# Create a virtual environment and install build requirements for the bindings
$(BINDINGS_VENV):
	@echo "Creating virtual environment in $(BINDINGS_VENV)..."
	$(PYTHON) -m venv $(BINDINGS_VENV)
	@echo "Upgrading pip, setuptools, and wheel in virtual environment..."
	$(PYTHON_BINDINGS_VENV) -m pip install --upgrade pip setuptools wheel

# Install dependencies and build the extension within the bindings virtual environment
install: $(BINDINGS_VENV)
	@echo "Python executable for bindings: $(PYTHON_BINDINGS_VENV)"
	$(PYTHON_BINDINGS_VENV) -m pip install .

# Build Python Wheel using setuptools within the bindings virtual environment
wheel: install
	$(PYTHON_BINDINGS_VENV) setup.py bdist_wheel --dist-dir=$(DISTDIR)

# Clean Build Artifacts
clean:
	@echo "Removing dist, build, and virtual environment directories..."
	$(RM) $(DISTDIR) $(BINDINGS_VENV) *.egg-info build

	@echo "Removing C extension artifacts..."
	$(RM) $(SRCDIR)/*.so
	$(RM) $(SRCDIR)/*.o

	@echo "Removing compiled Python files and __pycache__ directories..."
	@find . -type d -name "__pycache__" -exec $(RM) {} +
	@find . -type f -name "*.py[cod]" -exec $(RM) {} +

	@echo "Cleaning up additional log files or temporary files..."
	$(RM) *.coverage
	$(RM) *.log

# Install system dependencies (conditional based on OS)
deps:
ifeq ($(OS),Windows_NT)
	@echo "No system-level dependencies specified for Windows."
else
ifeq ($(UNAME_S),Darwin)
	@echo "No system-level dependencies required for macOS."
else
	@echo "Installing system-level dependencies for Linux..."
	sudo apt-get update
	sudo apt-get install -y valgrind
endif
endif

# Run Valgrind on Python Extension (Linux only)
valgrind-python: deps install
ifneq ($(OS),Windows_NT)
ifneq ($(UNAME_S),Darwin)
	@echo "Running Python with Valgrind to check for memory leaks..."
	valgrind --leak-check=full --track-origins=yes --show-reachable=yes --error-exitcode=1 $(PYTHON_BINDINGS_VENV) -c "import $(PYTHON_MODULE_NAME)"
endif
endif

# Run leaks on Python Extension (macOS only)
leaks-python: install
ifeq ($(UNAME_S),Darwin)
	@echo "Running Python with leaks to check for memory leaks..."
	leaks --atExit -- $(PYTHON_BINDINGS_VENV) -c "import $(PYTHON_MODULE_NAME)"
else
	@echo "The leaks command is only available on macOS."
endif

# Test memory leaks and run unit tests
test: clean $(BINDINGS_VENV) deps install
ifeq ($(OS),Windows_NT)
	@echo "Memory checking tools like Valgrind are not available on Windows."
else
ifeq ($(UNAME_S),Darwin)
	$(MAKE) leaks-python
else
	$(MAKE) valgrind-python
endif
endif
	$(PYTHON_BINDINGS_VENV) -m unittest discover $(TESTDIR) && \
	echo "All tests passed successfully."

# New target for AI-powered commit, add everything and push
ac:
	git add -A
	@PYTHONPATH=$(shell pwd) poetry run ai_commit 2>/dev/null | tail -n 5
	git push --quiet

# New target to run compress_image.py script
run_example: install
	@echo "Installing necessary dependencies for compress_image.py..."
	$(PYTHON_BINDINGS_VENV) -m pip install requests Pillow

	@echo "Running compress_image.py..."
	$(PYTHON_BINDINGS_VENV) $(COMPRESS_IMAGE_SCRIPT)
