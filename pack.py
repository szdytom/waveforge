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
    included_extensions = {'.png', '.json', '.md', '.mp3', '.wav', '.ogg'}
    
    # Recursively find and copy files
    for file_path in assets_dir.rglob('*'):
        if file_path.is_file() and file_path.suffix in included_extensions:
            # Calculate relative path
            relative_path = file_path.relative_to(assets_dir)
            # Exclude all files in 'prototype' directories
            if 'prototype' in relative_path.parts:
                continue
            # Exclude bundled-js — handled separately by copy_bundled_js
            if relative_path.parts[0] == 'bundled-js':
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


def copy_bundled_js(assets_dir: Path, output_assets_dir: Path):
    """
    Copy bundled JS output (from esbuild) into the package
    Uses .metafile.json to determine which files belong to the build,
    ignoring stale chunks left over from previous builds.
    """
    bundled_dir = assets_dir / "bundled-js"
    metafile_path = bundled_dir / ".metafile.json"
    if not metafile_path.is_file():
        print("No .metafile.json found, skipping JS bundle")
        return

    with metafile_path.open('r', encoding='utf-8') as f:
        meta = json.load(f)

    out_bundled = output_assets_dir / "bundled-js"
    out_bundled.mkdir(parents=True, exist_ok=True)

    # Strip possible cwd prefix from esbuild output keys
    # Keys are relative to CWD, e.g. "assets/bundled-js/react_hello.js"
    base_key = str(Path("assets") / "bundled-js").replace("\\", "/")

    for output_key in meta.get("outputs", {}):
        key = output_key.replace("\\", "/")
        # Strip the base prefix to get the path within bundled-js
        if key.startswith(base_key + "/"):
            rel = key[len(base_key) + 1:]
        elif key.startswith("./" + base_key + "/"):
            rel = key[len("./" + base_key) + 1:]
        else:
            # Try interpreting key as an absolute / directly relative path
            try:
                rel = str(Path(key).relative_to(bundled_dir.resolve()))
            except ValueError:
                print(f"Warning: skipping unrecognized output key: {output_key}")
                continue

        src = bundled_dir / rel
        if not src.is_file():
            print(f"Warning: output listed in metafile but not found on disk: {rel}")
            continue

        dst = out_bundled / rel
        dst.parent.mkdir(parents=True, exist_ok=True)

        if src.suffix == '.json':
            # Minify JSON (e.g. .metafile.json)
            data = json.loads(src.read_text(encoding='utf-8'))
            dst.write_text(json.dumps(data, separators=(',', ':'), ensure_ascii=False), encoding='utf-8')
            print(f"Added minified bundled-js: {rel}")
        else:
            shutil.copy2(src, dst)
            print(f"Copied bundled-js: {rel}")


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
        copy_bundled_js(assets_dir, output_assets_dir)
        
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
