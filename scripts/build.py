#!/usr/bin/env python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#     "click>=8.1.0",
#     "rich>=13.0.0",
# ]
# ///
"""Build script for AI SDK C++

Usage: 
    uv run scripts/build.py [OPTIONS]

Examples:
    uv run scripts/build.py --mode debug
    uv run scripts/build.py --mode release --tests
    uv run scripts/build.py --mode debug --tests --clean --export-compile-commands

This script handles:
- CMake configuration with vcpkg toolchain
- Building in Debug or Release mode
- Optional test building
- Clean builds
- Cross-platform support
- Export compile commands for IDEs
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

import click
from rich.console import Console
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn
from rich.table import Table

console = Console()


def run_command(cmd: list[str], cwd: Optional[Path] = None, check: bool = True) -> subprocess.CompletedProcess:
    """Run a command and handle errors with rich output."""
    console.print(f"[dim]Running:[/dim] [cyan]{' '.join(cmd)}[/cyan]")
    
    try:
        result = subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True)
        if result.stdout.strip():
            console.print(f"[dim]{result.stdout.strip()}[/dim]")
        return result
    except subprocess.CalledProcessError as e:
        console.print(f"[red]Error running command:[/red] {e}")
        if e.stderr:
            console.print(f"[red]Error output:[/red] {e.stderr}")
        if e.stdout:
            console.print(f"[yellow]Output:[/yellow] {e.stdout}")
        sys.exit(1)


def find_vcpkg_toolchain() -> Optional[str]:
    """Find the vcpkg toolchain file."""
    possible_locations = [
        "vcpkg/scripts/buildsystems/vcpkg.cmake",  # Local vcpkg
        Path.home() / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake",  # User vcpkg
        "/usr/local/share/vcpkg/scripts/buildsystems/vcpkg.cmake",  # System vcpkg on Linux/macOS
        "C:/vcpkg/scripts/buildsystems/vcpkg.cmake",  # System vcpkg on Windows
    ]
    
    for location in possible_locations:
        if Path(location).exists():
            return str(Path(location).resolve())
    
    # Check environment variable
    vcpkg_root = os.environ.get("VCPKG_ROOT")
    if vcpkg_root:
        toolchain = Path(vcpkg_root) / "scripts" / "buildsystems" / "vcpkg.cmake"
        if toolchain.exists():
            return str(toolchain.resolve())
    
    return None


@click.command()
@click.option(
    "--mode", 
    type=click.Choice(["debug", "release"], case_sensitive=False),
    default="debug",
    help="Build configuration (debug or release)"
)
@click.option(
    "--tests", 
    is_flag=True,
    help="Enable building tests"
)
@click.option(
    "--clean", 
    is_flag=True,
    help="Clean build directory before building"
)
@click.option(
    "--verbose", 
    is_flag=True,
    help="Enable verbose build output"
)
@click.option(
    "--export-compile-commands",
    is_flag=True,
    help="Export compile commands for IDEs (compile_commands.json)"
)
@click.option(
    "--jobs",
    type=int,
    default=None,
    help="Number of parallel build jobs (default: CPU count)"
)
def main(mode: str, tests: bool, clean: bool, verbose: bool, export_compile_commands: bool, jobs: Optional[int]):
    """Build AI SDK C++ with modern tooling."""
    
    # Get project paths
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    build_dir = project_root / "build"
    
    # Display build configuration
    config_table = Table(title="Build Configuration", show_header=True, header_style="bold blue")
    config_table.add_column("Setting", style="cyan")
    config_table.add_column("Value", style="green")
    
    config_table.add_row("Project root", str(project_root))
    config_table.add_row("Build directory", str(build_dir))
    config_table.add_row("Build mode", mode.upper())
    config_table.add_row("With tests", "✓" if tests else "✗")
    config_table.add_row("Clean build", "✓" if clean else "✗")
    config_table.add_row("Export compile commands", "✓" if export_compile_commands else "✗")
    config_table.add_row("Parallel jobs", str(jobs or os.cpu_count() or 4))
    
    console.print(config_table)
    console.print()
    
    # Clean build directory if requested
    if clean and build_dir.exists():
        with Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            console=console,
        ) as progress:
            task = progress.add_task("Cleaning build directory...", total=None)
            shutil.rmtree(build_dir)
            progress.update(task, completed=True)
        console.print("[green]✓[/green] Build directory cleaned")
    
    # Create build directory
    build_dir.mkdir(exist_ok=True)
    
    # Find vcpkg toolchain
    toolchain_file = find_vcpkg_toolchain()
    if not toolchain_file:
        console.print("[yellow]⚠ Warning: vcpkg toolchain not found. Dependencies may not be available.[/yellow]")
        console.print("[dim]Please ensure vcpkg is installed and VCPKG_ROOT is set, or vcpkg is in a standard location.[/dim]")
    else:
        console.print(f"[green]✓[/green] Using vcpkg toolchain: [cyan]{toolchain_file}[/cyan]")
    
    console.print()
    
    # Configure CMake
    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,
    ) as progress:
        task = progress.add_task("Configuring with CMake...", total=None)
        
        cmake_args = [
            'cmake',
            str(project_root),
            f'-DCMAKE_BUILD_TYPE={mode.title()}',
        ]
        
        # Add vcpkg toolchain if found
        if toolchain_file:
            cmake_args.append(f'-DCMAKE_TOOLCHAIN_FILE={toolchain_file}')
        
        # Add export compile commands option
        if export_compile_commands:
            cmake_args.append('-DCMAKE_EXPORT_COMPILE_COMMANDS=ON')
        
        # Add test option
        cmake_args.append(f'-DBUILD_TESTS={"ON" if tests else "OFF"}')
        
        # Always build examples for now
        cmake_args.append('-DBUILD_EXAMPLES=ON')
        
        run_command(cmake_args, cwd=build_dir)
        progress.update(task, completed=True)
    
    console.print("[green]✓[/green] CMake configuration completed")
    
    # Build
    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,  
    ) as progress:
        task = progress.add_task("Building...", total=None)
        
        build_args = ['cmake', '--build', '.']
        
        if verbose:
            build_args.append('--verbose')
        
        # Add parallel build option
        parallel_jobs = jobs or os.cpu_count() or 4
        build_args.extend(['--parallel', str(parallel_jobs)])
        
        run_command(build_args, cwd=build_dir)
        progress.update(task, completed=True)
    
    console.print("[green]✓[/green] Build completed successfully!")
    
    # Copy compile commands if requested
    if export_compile_commands:
        compile_commands_src = build_dir / "compile_commands.json"
        compile_commands_dst = project_root / "compile_commands.json"
        
        if compile_commands_src.exists():
            shutil.copy2(compile_commands_src, compile_commands_dst)
            console.print(f"[green]✓[/green] Exported compile commands to [cyan]{compile_commands_dst}[/cyan]")
        else:
            console.print("[yellow]⚠ Warning: compile_commands.json not generated[/yellow]")
    
    console.print()
    
    # Display build results
    results_panel = Panel.fit(
        f"""[bold green]Build Results[/bold green]

[bold]Built targets:[/bold]
  📚 Library: {build_dir}/libai-sdk-cpp.a (or .lib on Windows)
  🎯 Examples: {build_dir}/examples/
{f"  🧪 Tests: {build_dir}/tests/" if tests else ""}

[bold]To run examples (after setting API keys):[/bold]
  [cyan]export OPENAI_API_KEY=your_openai_key[/cyan]
  [cyan]export ANTHROPIC_API_KEY=your_anthropic_key[/cyan]
  [cyan]{build_dir}/examples/basic_chat[/cyan]
  [cyan]{build_dir}/examples/streaming_chat[/cyan]

{"""[bold]To run tests:[/bold]
  [cyan]cd build && ctest[/cyan]
  [cyan]cd build && ctest --verbose[/cyan]
  [cyan]cd build && ctest -R "test_types"[/cyan] (run specific test)""" if tests else ""}""",
        title="🎉 Success",
        border_style="green"
    )
    
    console.print(results_panel)


if __name__ == '__main__':
    main()