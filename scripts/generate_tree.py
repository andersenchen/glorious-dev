#!/usr/bin/env python3
import os
from pathlib import Path
import sys
import mimetypes

try:
    import pathspec
except ImportError:
    print("The 'pathspec' library is required but not installed.")
    print("Please install it using 'poetry add pathspec' and try again.")
    sys.exit(1)

try:
    from pygments.lexers import guess_lexer_for_filename
    from pygments.util import ClassNotFound
except ImportError:
    print("The 'pygments' library is required but not installed.")
    print("Please install it using 'poetry add pygments' and try again.")
    sys.exit(1)


def load_gitignore(start_path):
    """
    Loads .gitignore patterns from the start_path directory and its parent directories.
    """
    gitignore_files = []
    current_path = Path(start_path).resolve()

    while True:
        gitignore = current_path / '.gitignore'
        if gitignore.is_file():
            gitignore_files.append(gitignore)
        if current_path.parent == current_path:
            break
        current_path = current_path.parent

    patterns = []
    for gitignore in reversed(gitignore_files):
        with open(gitignore, 'r') as f:
            patterns.extend(f.readlines())

    spec = pathspec.PathSpec.from_lines('gitwildmatch', patterns)
    return spec


def is_binary(file_path):
    """
    Determines if a file is binary by checking for null bytes and using mimetypes.
    """
    try:
        # Check for null bytes in the first 1024 bytes
        with open(file_path, 'rb') as f:
            chunk = f.read(1024)
            if b'\0' in chunk:
                return True

        # Use mimetypes as a fallback
        mime_type, _ = mimetypes.guess_type(file_path)
        if mime_type is None:
            # If mimetype cannot be guessed, assume binary
            return True
        return not mime_type.startswith('text')
    except Exception:
        # If any error occurs, assume binary to be safe
        return True


def is_code(file_path):
    """
    Determines if a text file is source code using pygments.
    """
    try:
        with open(file_path, 'rb') as f:
            content = f.read(2048)  # Read a larger sample for better detection
        decoded_content = content.decode('utf-8', errors='ignore')
        lexer = guess_lexer_for_filename(file_path, decoded_content)
        if lexer.name.lower() in ['text only', 'plain text']:
            return False
        return True
    except (ClassNotFound, UnicodeDecodeError):
        return False
    except Exception:
        return False


def get_file_size(file_path):
    """
    Returns the size of the file in bytes.
    """
    try:
        size_bytes = os.path.getsize(file_path)
        return size_bytes
    except Exception:
        return 0


def count_lines(file_path):
    """
    Counts the number of lines in a text file.
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return sum(1 for _ in f)
    except Exception:
        return 0


def human_readable_size(size, decimal_places=2):
    """
    Converts a byte size into a human-readable format.
    """
    for unit in ['B', 'KB', 'MB', 'GB', 'TB', 'PB']:
        if size < 1024:
            return f"{size:.{decimal_places}f} {unit}"
        size /= 1024
    return f"{size:.{decimal_places}f} PB"


def generate_tree_markdown(start_path, spec, debug=False):
    """
    Generates a markdown-formatted tree of the directory structure.
    """
    tree_lines = []
    start_path = Path(start_path).resolve()

    for root, dirs, files in os.walk(start_path):
        root_path = Path(root)

        # Skip ignored directories
        dirs[:] = [d for d in dirs if not spec.match_file(str(root_path / d))]

        relative_path = root_path.relative_to(start_path)
        if str(relative_path) == '.':
            tree_lines.append(f"{root_path.name}")
            level = 0
        else:
            level = len(relative_path.parts)
            indent = '│   ' * (level - 1)
            connector = '├── ' if dirs or files else '└── '
            dir_name = root_path.name
            tree_lines.append(f"{indent}{connector}{dir_name} (Directory)")

        # Sort directories and files for consistent output
        dirs.sort()
        files = [f for f in files if not spec.match_file(str(root_path / f))]
        files.sort()

        for idx, file in enumerate(files):
            file_path = root_path / file
            is_last_file = idx == len(files) - 1
            connector = '└── ' if is_last_file else '├── '

            if is_binary(file_path):
                size = human_readable_size(get_file_size(file_path))
                line = f"{'│   ' * level}{connector}{file} ({size})"
            elif is_code(file_path):
                lines = count_lines(file_path)
                size = get_file_size(file_path)
                readable_size = human_readable_size(size)
                line = f"{'│   ' * level}{connector}{file} ({lines} lines, {readable_size})"
            else:
                lines = count_lines(file_path)
                line = f"{'│   ' * level}{connector}{file} ({lines} lines)"

            tree_lines.append(line)

            if debug:
                print(f"Processed file: {file_path}")

    return "\n".join(tree_lines)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Generate Project Directory Structure in Markdown.")
    parser.add_argument(
        '-o', '--output',
        type=str,
        default='DIRECTORY_STRUCTURE.md',
        help='Output markdown file path.'
    )
    parser.add_argument(
        '-d', '--directory',
        type=str,
        default='.',
        help='Start directory for generating the tree.'
    )
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Enable debug mode to print processed files.'
    )
    args = parser.parse_args()

    start_dir = args.directory

    # Load .gitignore patterns
    spec = load_gitignore(start_dir)

    # Generate the tree structure
    markdown_tree = generate_tree_markdown(start_dir, spec, debug=args.debug)
    md_content = f"# Project Directory Structure\n\n```markdown\n{markdown_tree}\n```\n"

    # Metadata Overview
    md_content += """
## Metadata Overview

- **Directories**: Indicated by `(Directory)`
- **Binary Files**: Displayed with their size in a human-readable format, e.g., `(1.5 KB)`
- **Code Files**: Indicated by the number of lines and their size, e.g., `(150 lines, 2.3 KB)`
- **Other Text Files**: Indicated by the number of lines, e.g., `(40 lines)`

"""

    # Write to the specified output file
    output_path = Path(args.output)
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(md_content)
        print(f"Directory structure successfully written to {output_path}")
    except Exception as e:
        print(f"Failed to write to {output_path}: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
