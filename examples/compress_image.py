#!/usr/bin/env python3

import argparse
import sys
import urllib.request

import glorious
from colorama import Fore, Style, init

# Initialize colorama
init(autoreset=True)

# Constants
BITS_PER_BYTE = 8
DEFAULT_IMAGE_URL = "https://freight.cargo.site/t/original/i/238bba1a42d1f45eabd8b67d36149419f9890f53bc59562f756b6c9a854dfb30/MS_Merkel_Angela_CloseUp.jpg"
DEFAULT_CONTEXT_LENGTH = 1000
MIN_CONTEXT_LENGTH = 1
MAX_CONTEXT_LENGTH = 10000


def validate_url(url):
    try:
        result = urllib.parse.urlparse(url)
        return all([result.scheme, result.netloc])
    except ValueError:
        return False


def validate_context_length(value):
    ivalue = int(value)
    if not MIN_CONTEXT_LENGTH <= ivalue <= MAX_CONTEXT_LENGTH:
        raise argparse.ArgumentTypeError(
            f"Context length must be between {MIN_CONTEXT_LENGTH} and {MAX_CONTEXT_LENGTH}"
        )
    return ivalue


def download_image_to_memory(url):
    try:
        with urllib.request.urlopen(url) as response:
            return response.read()
    except urllib.error.URLError as e:
        print(f"{Fore.RED}Error downloading image: {e}{Style.RESET_ALL}")
        sys.exit(1)


def main(url, context_length):
    print(f"Downloading image from: {Fore.CYAN}{url}{Style.RESET_ALL}")
    print(f"Using context length: {Fore.CYAN}{context_length}{Style.RESET_ALL}")

    image_data = download_image_to_memory(url)
    bit_length = len(image_data) * BITS_PER_BYTE

    compressed_data = glorious.encode(image_data, bit_length, context_length)
    decompressed_data = glorious.decode(compressed_data, bit_length, context_length)

    if decompressed_data == image_data:
        print(
            f"{Fore.GREEN}Success! Decompressed data matches the original.{Style.RESET_ALL}"
        )
    else:
        print(
            f"{Fore.RED}Error: Decompressed data does not match the original.{Style.RESET_ALL}"
        )

    compressed_bits_per_byte = len(compressed_data) * BITS_PER_BYTE / len(image_data)
    compression_ratio = BITS_PER_BYTE / compressed_bits_per_byte

    print(f"Original bits per byte: {Fore.YELLOW}{BITS_PER_BYTE}{Style.RESET_ALL}")
    print(
        f"Compressed bits per byte: {Fore.YELLOW}{compressed_bits_per_byte:.2f}{Style.RESET_ALL}"
    )
    print(
        f"Compression ratio: {Fore.GREEN if compression_ratio > 1 else Fore.RED}{compression_ratio:.2f}x{Style.RESET_ALL}"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compress and decompress an image using glorious."
    )
    parser.add_argument(
        "-u",
        "--url",
        default=DEFAULT_IMAGE_URL,
        type=str,
        help=f"URL of the image to compress (default: {DEFAULT_IMAGE_URL})",
    )
    parser.add_argument(
        "-c",
        "--context",
        type=validate_context_length,
        default=DEFAULT_CONTEXT_LENGTH,
        help=f"Context length for compression (default: {DEFAULT_CONTEXT_LENGTH}, min: {MIN_CONTEXT_LENGTH}, max: {MAX_CONTEXT_LENGTH})",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Increase output verbosity"
    )

    args = parser.parse_args()

    if not validate_url(args.url):
        parser.error(f"{Fore.RED}Invalid URL provided{Style.RESET_ALL}")

    if args.verbose:
        print(f"{Fore.BLUE}Verbose mode enabled{Style.RESET_ALL}")

    main(args.url, args.context)
