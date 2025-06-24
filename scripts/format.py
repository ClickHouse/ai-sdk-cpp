#!/usr/bin/env python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#     "click>=8.1.0",
#     "rich>=13.0.0",
# ]
# ///
"""Format all C++ source files using clang-format.

Usage: uv run scripts/format.py [--check]
"""

import shutil
import subprocess
import sys
from pathlib import Path
from typing import List

import click
from rich.console import Console
from rich.progress import track

console = Console()


def find_cpp_files() -> List[Path]:
    """Find all C++ source and header files in the project."""
    project_dir = Path(__file__).parent.parent
    extensions = {".h", ".hpp", ".cc", ".cpp", ".cxx"}
    exclude_dirs = {"build", "vcpkg_installed", ".cache", ".git"}
    
    files = []
    for ext in extensions:
        for file in project_dir.rglob(f"*{ext}"):
            # Skip files in excluded directories
            if any(excluded in file.parts for excluded in exclude_dirs):
                continue
            files.append(file)
    
    return sorted(files)


def check_file_format(file: Path) -> bool:
    """Check if a file is properly formatted."""
    result = subprocess.run(
        ["clang-format", "--dry-run", "--Werror", str(file)],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    return result.returncode == 0


def format_file(file: Path) -> None:
    """Format a file in place."""
    subprocess.run(
        ["clang-format", "-i", str(file)],
        check=True
    )


@click.command()
@click.option("--check", is_flag=True, help="Check formatting without modifying files")
def main(check: bool):
    """Format all C++ source files using clang-format."""
    # Check if clang-format is installed
    if not shutil.which("clang-format"):
        console.print("[red]Error: clang-format is not installed.[/red]")
        console.print("Install it with: brew install clang-format (macOS) or apt-get install clang-format (Ubuntu)")
        sys.exit(1)
    
    # Find all C++ files
    files = find_cpp_files()
    
    if not files:
        console.print("[yellow]No C++ files found[/yellow]")
        return
    
    if check:
        console.print(f"[blue]Checking code formatting for {len(files)} files...[/blue]\n")
        
        needs_format = []
        for file in track(files, description="Checking files...", console=console):
            if not check_file_format(file):
                needs_format.append(file)
                console.print(f"[red]✗[/red] {file.relative_to(file.parent.parent)}")
        
        if not needs_format:
            console.print("\n[green]✅ All files are properly formatted[/green]")
            sys.exit(0)
        else:
            console.print(f"\n[red]❌ {len(needs_format)} files need formatting:[/red]")
            for file in needs_format:
                console.print(f"  • {file.relative_to(file.parent.parent)}")
            console.print("\n[yellow]Run 'uv run scripts/format.py' to fix them.[/yellow]")
            sys.exit(1)
    else:
        console.print(f"[blue]Formatting {len(files)} C++ files...[/blue]\n")
        
        for file in track(files, description="Formatting files...", console=console):
            format_file(file)
            console.print(f"[green]✓[/green] {file.name}")
        
        console.print("\n[green]✅ All files have been formatted[/green]")


if __name__ == "__main__":
    main() 