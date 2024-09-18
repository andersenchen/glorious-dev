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
"""

import os
import subprocess
import sys
import time
from typing import List

import openai

# Set your OpenAI API key from an environment variable
openai.api_key = os.getenv("OPENAI_API_KEY")

# Initialize OpenAI client
client = openai.OpenAI()

# Constant format string for the prompt
COMMIT_MESSAGE_PROMPT = """## GIT DIFF START
{diff}
## GIT DIFF END

A commit message starts with a brief imperative summary (max 50 chars), optionally followed by a body explaining why and how, with lines up to 72characters.

Write a GitHub commit message Jeff Dean would write if he saw this diff. Be realistic.

GitHub commit messages do not support bold text or Markdown formatting. Your first line will be rendered as the title of the commit in plaintext, and the rest of the lines will be the body.
"""


def get_git_diff_with_function_context(amend: bool = False) -> str:
  """Retrieves the git diff with function context.

  Args:
    amend: A boolean indicating whether to get the diff for amending a commit.

  Returns:
    A string containing the git diff output with function context.

  Raises:
    subprocess.CalledProcessError: If the git command fails.
  """
  try:
    if amend:
      parent_hash = subprocess.check_output(
          ["git", "rev-parse", "HEAD^"],
          stderr=subprocess.STDOUT
      ).decode("utf-8").strip()
      return subprocess.check_output(
          ["git", "diff", "--function-context", parent_hash],
          stderr=subprocess.STDOUT
      ).decode("utf-8")
    else:
      return subprocess.check_output(
          ["git", "diff", "--cached", "--function-context"],
          stderr=subprocess.STDOUT
      ).decode("utf-8")
  except subprocess.CalledProcessError as e:
    print(f"Error executing git command: {e.output.decode('utf-8')}")
    raise


def generate_commit_message(diff: str, amend: bool = False) -> str:
  """Generates a commit message using OpenAI's language model in Google style.

  Args:
    diff: A string containing the git diff to base the commit message on.
    amend: A boolean indicating whether the commit is amending a previous one.

  Returns:
    A string containing the generated commit message.

  Raises:
    openai.error.OpenAIError: If there's an error in the OpenAI API call.
  """
  try:
    print("Starting OpenAI API call...")
    start_time = time.time()

    response = client.chat.completions.create(
        model="o1-mini",
        messages=[
            {
              "role": "user",
              "content": COMMIT_MESSAGE_PROMPT.format(diff=diff),
            },
        ],
    )

    end_time = time.time()
    print(f"OpenAI API call completed in {end_time - start_time:.2f} seconds")

    # Get the raw content of the response
    message_content = response.choices[0].message.content.strip()

    # Remove the triple backticks and any surrounding whitespace
    if message_content.startswith("```") and message_content.endswith("```"):
      message_content = message_content[3:-3].strip()

    return message_content

  except openai.error.OpenAIError as e:
    print(f"Error generating commit message: {str(e)}")
    raise


def main() -> None:
  """Main function to run the AI commit message generator."""
  # Check for command line arguments
  amend = "--amend" in sys.argv
  review = "--review" in sys.argv

  try:
    diff = get_git_diff_with_function_context(amend)

    if not diff:
      print("No changes to commit.")
      return

    commit_message = generate_commit_message(diff, amend)

    # Print the generated commit message
    print("\nGenerated commit message:")
    print("---------------------------")
    print(commit_message)
    print("---------------------------\n")

    # Prepare the git command
    git_command: List[str] = ["git", "commit"]
    if amend:
      git_command.append("--amend")

    # Add -e flag if review is requested
    if review:
      git_command.append("-e")

    git_command.extend(["-m", commit_message])

    # Execute the git command
    subprocess.run(git_command, check=True)

    if amend:
      print(
          "Amended commit."
          + (
              " Please review and adjust the commit message if needed."
              if review
              else ""
          )
      )
    else:
      print(
          "Created new commit."
          + (
              " Please review and adjust the commit message if needed."
              if review
              else ""
          )
      )

  except subprocess.CalledProcessError as e:
    print(f"Error executing git command: {e}")
    sys.exit(1)
  except openai.error.OpenAIError as e:
    print(f"Error with OpenAI API: {e}")
    sys.exit(1)
  except Exception as e:
    print(f"An unexpected error occurred: {e}")
    sys.exit(1)


if __name__ == "__main__":
  main()
