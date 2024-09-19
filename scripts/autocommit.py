#!/usr/bin/env python3
"""AI-powered Git commit message generator using OpenAI's language models.

This script generates Git commit messages in Google style based on the diff of
staged changes or when amending a commit. It uses OpenAI's language models to
analyze the diff and create an appropriate commit message.

Typical usage example:

  python autocommit.py
  python autocommit.py --amend
  python autocommit.py --review
  python autocommit.py --amend --review
  python autocommit.py --push-to-private
  python autocommit.py --push-to-private --force
"""

import argparse
import os
import subprocess
import sys
import time
from typing import List

import openai
from colorama import Fore, init

# Initialize colorama
init(autoreset=True)

# Set your OpenAI API key from an environment variable
openai.api_key = os.getenv("OPENAI_API_KEY")

# Initialize OpenAI client
client = openai.OpenAI()

# Constant format string for the prompt
COMMIT_MESSAGE_PROMPT = """\
## GIT DIFF START
{diff}
## GIT DIFF END


You are a helpful intelligent programming assistant with the intelligence and writing style of Jeff Dean.

Recall that GitHub commit messages do not support bold text or Markdown formatting.

Your first line will be rendered as the title of the commit in plaintext, and the rest of the lines will be the body.

# Your instructions:
- A commit message starts with a brief imperative summary (a strict maximum of 50 characters), optionally followed by a body explaining why and how, with lines up to 72 characters.
- In the message body, use lists with dashes if appropriate.
"""


def print_step(message: str) -> None:
    """Print a step message in cyan color."""
    print(f"{Fore.CYAN}{message}")


def print_success(message: str) -> None:
    """Print a success message in green color."""
    print(f"{Fore.GREEN}{message}")


def print_warning(message: str) -> None:
    """Print a warning message in yellow color."""
    print(f"{Fore.YELLOW}{message}")


def print_error(message: str) -> None:
    """Print an error message in red color."""
    print(f"{Fore.RED}{message}")


def get_git_diff_with_function_context(amend: bool = False) -> str:
    """Retrieves the git diff with function context."""
    try:
        if amend:
            parent_hash = (
                subprocess.check_output(
                    ["git", "rev-parse", "HEAD^"], stderr=subprocess.STDOUT
                )
                .decode("utf-8")
                .strip()
            )
            return subprocess.check_output(
                ["git", "diff", "--function-context", parent_hash],
                stderr=subprocess.STDOUT,
            ).decode("utf-8")
        else:
            return subprocess.check_output(
                ["git", "diff", "--cached", "--function-context"],
                stderr=subprocess.STDOUT,
            ).decode("utf-8")
    except subprocess.CalledProcessError as e:
        print_error(f"Error executing git command: {e.output.decode('utf-8')}")
        raise


def get_previous_commits(num_commits: int = 5) -> str:
    """Retrieves the previous commit messages."""
    try:
        return subprocess.check_output(
            ["git", "log", f"-{num_commits}", "--pretty=format:%s%n%b%n"],
            stderr=subprocess.STDOUT,
        ).decode("utf-8")
    except subprocess.CalledProcessError as e:
        print_error(f"Error executing git command: {e.output.decode('utf-8')}")
        raise


def generate_commit_message(
    diff: str, previous_commits: str, amend: bool = False
) -> str:
    """Generates a commit message using OpenAI's language model in Google style."""
    try:
        print_step("Starting OpenAI API call...")
        start_time = time.time()

        response = client.chat.completions.create(
            model="o1-mini",
            messages=[
                {
                    "role": "user",
                    "content": COMMIT_MESSAGE_PROMPT.format(
                        previous_commits=previous_commits, diff=diff
                    ),
                },
            ],
        )

        end_time = time.time()
        print_success(
            f"OpenAI API call completed in {end_time - start_time:.2f} seconds"
        )

        # Get the raw content of the response
        message_content = response.choices[0].message.content.strip()

        # Remove the triple backticks and any surrounding whitespace
        if message_content.startswith("```") and message_content.endswith("```"):
            message_content = message_content[3:-3].strip()

        return message_content

    except Exception as e:
        print_error(f"Error generating commit message: {str(e)}")
        raise


def main() -> None:
    """Main function to run the AI commit message generator."""
    parser = argparse.ArgumentParser(
        description="AI-powered Git commit message generator"
    )
    parser.add_argument(
        "--amend", action="store_true", help="Amend the previous commit"
    )
    parser.add_argument(
        "--review", action="store_true", help="Open editor for review before committing"
    )
    parser.add_argument(
        "--push-to-private",
        action="store_true",
        help="Push to private repository after committing",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force push to private repository (use with caution)",
    )
    args = parser.parse_args()

    try:
        print_step("Adding all changes to git...")
        subprocess.run(["git", "add", "-A"], check=True)
        print_success("All changes added to git.")

        diff = get_git_diff_with_function_context(args.amend)

        if not diff:
            print_warning("No changes to commit.")
            return

        previous_commits = get_previous_commits()
        commit_message = generate_commit_message(diff, previous_commits, args.amend)

        # Print the generated commit message
        print_step("\nGenerated commit message:")
        print_step("---------------------------")
        print(f"{Fore.WHITE}{commit_message}")
        print_step("---------------------------\n")

        # Prepare the git command
        git_command: List[str] = ["git", "commit"]
        if args.amend:
            git_command.append("--amend")

        # Add -e flag if review is requested
        if args.review:
            git_command.append("-e")

        git_command.extend(["-m", commit_message])

        # Execute the git command
        print_step("Committing changes...")
        subprocess.run(git_command, check=True)

        if args.amend:
            print_success(
                "Amended commit."
                + (
                    " Please review and adjust the commit message if needed."
                    if args.review
                    else ""
                )
            )
        else:
            print_success(
                "Created new commit."
                + (
                    " Please review and adjust the commit message if needed."
                    if args.review
                    else ""
                )
            )

        # Push to private repository if --push-to-private option is used
        if args.push_to_private:
            push_command = ["git", "push", "private", "main"]
            if args.force:
                push_command.append("--force")

            print_step(
                f"{'Force pushing' if args.force else 'Pushing'} to private repository..."
            )
            subprocess.run(push_command, check=True)
            print_success(
                f"{'Force pushed' if args.force else 'Pushed'} to private repository."
            )

    except subprocess.CalledProcessError as e:
        print_error(f"Error executing git command: {e}")
        sys.exit(1)
    except openai.error.OpenAIError as e:
        print_error(f"Error with OpenAI API: {e}")
        sys.exit(1)
    except Exception as e:
        print_error(f"An unexpected error occurred: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
