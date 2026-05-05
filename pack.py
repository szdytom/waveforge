#!/usr/bin/env python3
"""
Cross-platform packaging script for WaveForge
Copies executable and assets, then creates a zip archive
"""

import os
import sys
import shutil
import zipfile
import json
from pathlib import Path

def copy_assets(assets_dir: Path, output_assets_dir: Path):
    """
    Recursively copy required assets
    Minify all .json files
    """
    output_assets_dir.mkdir(parents=True, exist_ok=True)
    
    # Extensions to include
    included_extensions = {'.png', '.json', '.md', '.mp3', '.wav', '.ogg', '.js'}
    
    # Recursively find and copy files
    for file_path in assets_dir.rglob('*'):
        if file_path.is_file() and file_path.suffix in included_extensions:
            # Calculate relative path
            relative_path = file_path.relative_to(assets_dir)
            # Exclude all files in 'prototype' directories
            if 'prototype' in relative_path.parts:
                continue
            output_path = output_assets_dir / relative_path
            # Ensure output directory exists
            output_path.parent.mkdir(parents=True, exist_ok=True)
            if file_path.suffix == '.json':
                # Minify JSON
                try:
                    with file_path.open('r', encoding='utf-8') as rf:
                        data = json.load(rf)
                    minified = json.dumps(data, separators=(',', ':'), ensure_ascii=False)
                    output_path.write_text(minified, encoding='utf-8')
                    print(f"Added minified JSON: {relative_path}")
                except Exception as e:
                    # Fallback: copy original if JSON parsing fails
                    shutil.copy2(file_path, output_path)
                    print(f"Warning: failed to minify {relative_path} ({e}), copied original instead")
            else:
                # Copy other files directly
                shutil.copy2(file_path, output_path)
                print(f"Copied: {relative_path}")


def create_package(executable_path: Path, assets_dir: Path, output_zip: Path, platform_name: str):
    """
    Create a zip package containing the executable and assets
    """
    # Create temporary directory for packaging
    temp_dir = Path("temp_package")
    if temp_dir.exists():
        shutil.rmtree(temp_dir)
    temp_dir.mkdir()
    
    try:
        # Copy executable
        executable_name = executable_path.name
        shutil.copy2(executable_path, temp_dir / executable_name)
        print(f"Copied executable: {executable_name}")
        
        # Copy assets
        output_assets_dir = temp_dir / "assets"
        copy_assets(assets_dir, output_assets_dir)
        
        # Create zip file
        print(f"\nCreating {output_zip}...")
        with zipfile.ZipFile(output_zip, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # Add executable
            zipf.write(temp_dir / executable_name, executable_name)
            
            # Add assets
            for root, dirs, files in os.walk(output_assets_dir):
                for file in files:
                    file_path = Path(root) / file
                    arcname = file_path.relative_to(temp_dir)
                    zipf.write(file_path, arcname)
        
        print(f"Package created successfully: {output_zip}")
        print(f"Package size: {output_zip.stat().st_size / 1024 / 1024:.2f} MB")
        
    finally:
        # Clean up temporary directory
        if temp_dir.exists():
            shutil.rmtree(temp_dir)


def main():
    if len(sys.argv) != 5:
        print("Usage: pack.py <executable_path> <assets_dir> <output_zip> <platform_name>")
        print("Example: pack.py build/waveforge assets waveforge-linux-x64.zip linux")
        sys.exit(1)
    
    executable_path = Path(sys.argv[1])
    assets_dir = Path(sys.argv[2])
    output_zip = Path(sys.argv[3])
    platform_name = sys.argv[4]
    
    # Validate inputs
    if not executable_path.exists():
        print(f"Error: Executable not found: {executable_path}")
        sys.exit(1)
    
    if not assets_dir.exists():
        print(f"Error: Assets directory not found: {assets_dir}")
        sys.exit(1)
    
    # Create output directory if needed
    output_zip.parent.mkdir(parents=True, exist_ok=True)
    
    # Create package
    create_package(executable_path, assets_dir, output_zip, platform_name)


if __name__ == "__main__":
    main()
