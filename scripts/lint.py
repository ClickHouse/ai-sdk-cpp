#!/usr/bin/env python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#     "click>=8.1.0",
#     "rich>=13.0.0",
#     "asyncio",
# ]
# ///
"""Run clang-tidy on all C++ source files in parallel.

Usage: uv run scripts/lint.py [--fix] [--jobs N]
"""

import asyncio
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

import click
from rich.console import Console
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, MofNCompleteColumn

console = Console()


def find_compile_commands() -> Optional[Path]:
    """Find compile_commands.json in build directory or project root."""
    project_dir = Path(__file__).parent.parent
    build_dir = project_dir / "build"
    
    # Check build directory first
    build_commands = build_dir / "compile_commands.json"
    if build_commands.exists():
        return build_commands
    
    # Check project root
    root_commands = project_dir / "compile_commands.json"
    if root_commands.exists():
        return root_commands
    
    return None


def get_system_include_args() -> List[str]:
    """Get system include paths for macOS."""
    if sys.platform != "darwin":
        return []
    
    try:
        # Get Xcode path
        result = subprocess.run(["xcode-select", "-p"], capture_output=True, text=True)
        if result.returncode != 0:
            xcode_path = "/Applications/Xcode.app/Contents/Developer"
        else:
            xcode_path = result.stdout.strip()
        
        sdk_path = Path(xcode_path) / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
        
        if sdk_path.exists():
            return [
                f"--extra-arg=-isystem{sdk_path}/usr/include/c++/v1",
                f"--extra-arg=-isystem{sdk_path}/usr/include",
                f"--extra-arg=-isystem{xcode_path}/Toolchains/XcodeDefault.xctoolchain/usr/include"
            ]
    except Exception:
        pass
    
    return []


def find_cpp_files() -> List[Path]:
    """Find all C++ source files in the project."""
    project_dir = Path(__file__).parent.parent
    extensions = {".cc", ".cpp", ".cxx"}
    exclude_dirs = {"build", "vcpkg_installed", ".cache", ".git"}
    
    files = []
    for ext in extensions:
        for file in project_dir.rglob(f"*{ext}"):
            # Skip files in excluded directories
            if any(excluded in file.parts for excluded in exclude_dirs):
                continue
            files.append(file)
    
    return sorted(files)


async def lint_file(
    file: Path, 
    compile_commands: Path,
    fix: bool,
    extra_args: List[str],
    semaphore: asyncio.Semaphore
) -> Tuple[Path, bool, str]:
    """Run clang-tidy on a single file."""
    async with semaphore:
        cmd = [
            "clang-tidy",
            f"-p={compile_commands}",
            *extra_args,
        ]
        
        if fix:
            cmd.extend(["--fix", "--fix-errors"])
        
        cmd.append(str(file))
        
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        
        stdout, stderr = await proc.communicate()
        
        output = stdout.decode() + stderr.decode()
        success = proc.returncode == 0
        
        return file, success, output


async def lint_all_files(files: List[Path], compile_commands: Path, fix: bool, jobs: int):
    """Lint all files in parallel."""
    extra_args = get_system_include_args()
    semaphore = asyncio.Semaphore(jobs)
    
    # Create tasks for all files
    tasks = [
        lint_file(file, compile_commands, fix, extra_args, semaphore)
        for file in files
    ]
    
    # Run with progress bar
    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        MofNCompleteColumn(),
        console=console,
    ) as progress:
        task_id = progress.add_task(
            "[cyan]Linting files...", 
            total=len(files)
        )
        
        failed_files = []
        
        # Process results as they complete
        for coro in asyncio.as_completed(tasks):
            file, success, output = await coro
            progress.update(task_id, advance=1)
            
            if success:
                console.print(f"[green]✓[/green] {file.name}")
            else:
                console.print(f"[red]✗[/red] {file.name}")
                failed_files.append((file, output))
        
        # Print details for failed files
        if failed_files:
            console.print("\n[red]Issues found in the following files:[/red]")
            for file, output in failed_files:
                console.print(f"\n[yellow]{file}:[/yellow]")
                console.print(output)
        
        return len(failed_files) == 0


@click.command()
@click.option("--fix", is_flag=True, help="Apply fixes automatically")
@click.option("--jobs", "-j", type=int, default=os.cpu_count() or 4, help="Number of parallel jobs")
def main(fix: bool, jobs: int):
    """Run clang-tidy on all C++ source files in parallel."""
    # Check if clang-tidy is installed
    if not shutil.which("clang-tidy"):
        console.print("[red]Error: clang-tidy is not installed.[/red]")
        console.print("Install it with: brew install llvm (macOS) or apt-get install clang-tidy (Ubuntu)")
        sys.exit(1)
    
    # Find compile_commands.json
    compile_commands = find_compile_commands()
    if not compile_commands:
        console.print("[red]Error: compile_commands.json not found.[/red]")
        console.print("Please build the project first with:")
        console.print("  cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
        console.print("  cmake --build build")
        sys.exit(1)
    
    console.print(f"[blue]Using compile commands: {compile_commands}[/blue]")
    console.print(f"[blue]Running with {jobs} parallel jobs[/blue]")
    
    # Find all C++ files
    files = find_cpp_files()
    console.print(f"[blue]Found {len(files)} files to lint[/blue]\n")
    
    if not files:
        console.print("[yellow]No C++ files found to lint[/yellow]")
        return
    
    # Run linting
    success = asyncio.run(lint_all_files(files, compile_commands, fix, jobs))
    
    # Print summary
    console.print()
    if success:
        if fix:
            console.print("[green]✅ Linting completed with fixes applied[/green]")
        else:
            console.print("[green]✅ No linting issues found[/green]")
        sys.exit(0)
    else:
        if fix:
            console.print("[yellow]⚠️  Some files could not be automatically fixed[/yellow]")
        else:
            console.print("[red]❌ Linting issues found. Run 'uv run scripts/lint.py --fix' to auto-fix some issues.[/red]")
        sys.exit(1)


if __name__ == "__main__":
    main() 