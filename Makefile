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

# Git command variable
GIT := git

# Directories
SRCDIR := src$(SEP)compression$(SEP)c$(SEP)src
INCDIR := src$(SEP)compression$(SEP)c$(SEP)include
BINDINGS_DIR := src$(SEP)compression$(SEP)bindings
TESTDIR := tests
DISTDIR := dist

PYTHON_MODULE_NAME := glorious
EXAMPLES_DIR := examples
COMPRESS_IMAGE_SCRIPT := $(EXAMPLES_DIR)/compress_image.py

# Backup branch name
BACKUP_BRANCH := backup-before-sync

# Targets
.PHONY: all clean install wheel test deps valgrind-python leaks-python ac run_example ship sync-with-public

# Default target
all: install

# Create a virtual environment and install build requirements for the bindings
$(BINDINGS_VENV):
	@echo "Creating virtual environment in $(BINDINGS_VENV)..."
	@$(PYTHON) -m venv $(BINDINGS_VENV) > /dev/null 2>&1
	@echo "Upgrading pip, setuptools, and wheel in virtual environment..."
	@$(PYTHON_BINDINGS_VENV) -m pip install --upgrade pip setuptools wheel > /dev/null 2>&1
	@echo "Virtual environment setup complete."

# Install dependencies and build the extension within the bindings virtual environment
install: $(BINDINGS_VENV)
	@echo "Installing project and dependencies..."
	@$(PYTHON_BINDINGS_VENV) -m pip install . > /dev/null 2>&1
	@echo "Installation complete."

# Build Python Wheel using setuptools within the bindings virtual environment
wheel: install
	@echo "Building Python wheel..."
	@$(PYTHON_BINDINGS_VENV) setup.py bdist_wheel --dist-dir=$(DISTDIR) > /dev/null 2>&1
	@echo "Wheel built successfully in $(DISTDIR)."

# Clean Build Artifacts
clean:
	@echo "Removing dist, build, and virtual environment directories..."
	@$(RM) $(DISTDIR) $(BINDINGS_VENV) *.egg-info build

	@echo "Removing C extension artifacts..."
	@$(RM) $(SRCDIR)/*.so
	@$(RM) $(SRCDIR)/*.o

	@echo "Removing compiled Python files and __pycache__ directories..."
	@find . -type d -name "__pycache__" -exec $(RM) {} +
	@find . -type f -name "*.py[cod]" -exec $(RM) {} +

	@echo "Cleaning up additional log files or temporary files..."
	@$(RM) *.coverage
	@$(RM) *.log

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
	@sudo apt-get update > /dev/null 2>&1
	@sudo apt-get install -y valgrind > /dev/null 2>&1
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
	@valgrind --leak-check=full --track-origins=yes --show-reachable=yes --error-exitcode=1 $(PYTHON_BINDINGS_VENV) -c "import $(PYTHON_MODULE_NAME)"
endif
endif

# Run leaks on Python Extension (macOS only)
leaks-python: install
ifeq ($(UNAME_S),Darwin)
	@echo "Running Python with leaks to check for memory leaks..."
	@leaks --atExit -- $(PYTHON_BINDINGS_VENV) -c "import $(PYTHON_MODULE_NAME)"
else
	@echo "The leaks command is only available on macOS. Skipping leaks checks."
endif

# Test memory leaks and run unit tests
test: clean $(BINDINGS_VENV) deps install
ifeq ($(OS),Windows_NT)
	@echo "Memory checking tools like Valgrind are not available on Windows."
else
ifeq ($(UNAME_S),Darwin)
	@echo "Checking for memory leaks on macOS..."
	# Capture output in a temporary file and only show logs if there is a failure
	@$(MAKE) leaks-python > leaks.log 2>&1 || (cat leaks.log && echo "Memory leaks detected!" && exit 1)
	@rm -f leaks.log
else
	@echo "Checking for memory leaks with Valgrind..."
	# Capture Valgrind output in a temporary file and only show logs if there is a failure
	@$(MAKE) valgrind-python > valgrind.log 2>&1 || (cat valgrind.log && echo "Valgrind detected issues!" && exit 1)
	@rm -f valgrind.log
endif
endif
	@echo "Running unit tests..."
	# Activate venv, run tests, and deactivate venv
	@$(PYTHON_BINDINGS_VENV) -m unittest discover $(TESTDIR) || (exit 1)
	@echo "All tests passed successfully."


# New target to run compress_image.py script
run_example: install
	@echo "Installing necessary dependencies for compress_image.py..."
	@$(PYTHON_BINDINGS_VENV) -m pip install requests Pillow > /dev/null 2>&1
	@echo "Dependencies installed."

	@echo "Running compress_image.py..."
	@$(PYTHON_BINDINGS_VENV) $(COMPRESS_IMAGE_SCRIPT)

# New target to sync private and public repositories, generate AI commit message, and push to private
sync-with-public: install test
	@echo "Syncing private repo with public repo..."

	# Ensure the working directory is clean
	@echo "Checking for uncommitted changes..."
	@if [ -n "$$(git status --porcelain)" ]; then \
		echo "You have uncommitted changes. Please commit or stash them before syncing."; \
		exit 1; \
	fi

	# Backup current state to a temporary branch
	@echo "Creating backup branch '$(BACKUP_BRANCH)'..."
	@$(GIT) branch $(BACKUP_BRANCH) > /dev/null 2>&1 || echo "Backup branch '$(BACKUP_BRANCH)' already exists."

	# Fetch the latest changes from both private and public remotes
	@echo "Fetching latest changes from 'private' and 'origin' remotes..."
	@$(GIT) fetch private
	@$(GIT) fetch origin

	# Find the last common commit between the public and private branches
	@echo "Determining the last common commit between 'origin/main' and 'private/main'..."
	@LAST_COMMON_COMMIT=$$(git merge-base origin/main private/main) && \
	echo "Last common commit is $$LAST_COMMON_COMMIT" && \

	# Soft reset to the last common commit so changes are staged but history remains intact
	@echo "Performing soft reset to the last common commit..."
	@$(GIT) reset --soft $$LAST_COMMON_COMMIT

	# Stage all changes for commit
	@echo "Staging all changes for commit..."
	@$(GIT) add -A

	# Generate AI-powered commit message using your AI commit script (assuming it's already set up in poetry)
	@echo "Generating AI-powered commit message..."
	@PYTHONPATH=$(shell pwd) poetry run ai_commit

	# Detect current branch and push if on 'main'
	@echo "Detecting current branch to determine push behavior..."
	@CURRENT_BRANCH=$$(git rev-parse --abbrev-ref HEAD); \
	if [ "$$CURRENT_BRANCH" = "main" ]; then \
		echo "On 'main' branch. Pushing cleaned-up history to private repository..."; \
		$(GIT) push private main; \
		\
		# Delete the backup branch after successful push
		echo "Deleting backup branch '$(BACKUP_BRANCH)'..."; \
		$(GIT) branch -d $(BACKUP_BRANCH) > /dev/null 2>&1 || echo "Backup branch '$(BACKUP_BRANCH)' could not be deleted. It might not exist or has unmerged changes."; \
	else \
		echo "Not on 'main' branch. Skipping push."; \
	fi

	@echo "Sync with public repository completed successfully."
