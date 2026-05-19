"""
_config.py — auto-detect paths for Geant4, OCC, HDF5, Qt5.

Priority order for each dependency:
  1. Environment variable (e.g. GEANT4_INSTALL)
  2. Active conda environment
  3. Known manual install locations
  4. Homebrew (macOS)
  5. Common system paths
"""

import os
import shutil
import sys
from pathlib import Path


# ============================================================
# conda environment prefix
# ============================================================

def _conda_prefix() -> Path | None:
    """Return the active conda environment prefix if any."""
    prefix = os.environ.get("CONDA_PREFIX") or os.environ.get("CONDA_DEFAULT_ENV")
    if prefix and Path(prefix).exists():
        return Path(prefix)
    # infer from sys.prefix if it looks like a conda env
    p = Path(sys.prefix)
    if (p / "conda-meta").exists():
        return p
    return None


# ============================================================
# candidate paths for each dependency
# ============================================================

def _candidates(env_var: str, conda_subpath: str, extras: list[str]) -> list[Path]:
    """Build ordered list of candidate paths."""
    candidates = []

    # 1. environment variable
    v = os.environ.get(env_var, "")
    if v:
        candidates.append(Path(v))

    # 2. conda environment
    conda = _conda_prefix()
    if conda:
        candidates.append(conda / conda_subpath)
        candidates.append(conda)

    # 3. extras (manual installs, homebrew, system)
    for e in extras:
        if e:
            candidates.append(Path(e))

    return candidates


def _find(name: str, marker: str, candidates: list[Path]) -> Path | None:
    for p in candidates:
        if p and (p / marker).exists():
            return p
    return None


# ============================================================
# public finders
# ============================================================

def geant4_dir() -> Path:
    candidates = _candidates(
        env_var="GEANT4_INSTALL",
        conda_subpath=".",          # conda installs geant4 into env root
        extras=[
            "/Users/maninder/Desktop/Programs/geant4-install",
            "/usr/local/geant4",
            "/opt/geant4",
        ]
    )
    p = _find("geant4", "bin/geant4-config", candidates)
    if p is None:
        raise RuntimeError(
            "Geant4 not found.\n"
            "Install via conda:  conda install -c conda-forge geant4\n"
            "Or set:             export GEANT4_INSTALL=/path/to/geant4-install"
        )
    return p


def occ_dir() -> Path:
    candidates = _candidates(
        env_var="OCC_INSTALL",
        conda_subpath=".",
        extras=[
            "/opt/homebrew/Cellar/opencascade/7.9.3",
            "/opt/homebrew/opt/opencascade",
            "/usr/local/opencascade",
        ]
    )
    p = _find("occ", "include/opencascade/TopoDS_Shape.hxx", candidates)
    if p is None:
        raise RuntimeError(
            "OpenCASCADE not found.\n"
            "Install via conda:  conda install -c conda-forge opencascade\n"
            "Install via brew:   brew install opencascade\n"
            "Or set:             export OCC_INSTALL=/path/to/opencascade"
        )
    return p


def hdf5_dir() -> Path:
    candidates = _candidates(
        env_var="HDF5_INSTALL",
        conda_subpath=".",
        extras=[
            "/Users/maninder/Desktop/Programs/hdf5-install",
            "/opt/homebrew/opt/hdf5",
            "/usr/local",
        ]
    )
    # try cmake subdir markers
    for marker in ["cmake/hdf5-config.cmake", "lib/cmake/hdf5/hdf5-config.cmake"]:
        p = _find("hdf5", marker, candidates)
        if p:
            return p
    raise RuntimeError(
        "HDF5 not found.\n"
        "Install via conda:  conda install -c conda-forge hdf5\n"
        "Install via brew:   brew install hdf5\n"
        "Or set:             export HDF5_INSTALL=/path/to/hdf5-install"
    )


def qt5_dir() -> Path:
    candidates = _candidates(
        env_var="Qt5_DIR",
        conda_subpath=".",
        extras=[
            "/opt/homebrew/opt/qt@5",
            "/opt/homebrew/opt/qt5",
            "/usr/local/opt/qt@5",
        ]
    )
    p = _find("qt5", "lib/cmake/Qt5Core", candidates)
    if p is None:
        raise RuntimeError(
            "Qt5 not found.\n"
            "Install via conda:  conda install -c conda-forge qt\n"
            "Install via brew:   brew install qt@5\n"
            "Or set:             export Qt5_DIR=/path/to/qt5"
        )
    return p


def cmake_bin() -> str:
    c = shutil.which("cmake")
    if c is None:
        raise RuntimeError(
            "cmake not found.\n"
            "Install via conda:  conda install -c conda-forge cmake\n"
            "Install via brew:   brew install cmake"
        )
    return c


def print_config():
    """Print detected dependency paths."""
    conda = _conda_prefix()
    print("gdml-to-mesh dependency detection:")
    if conda:
        print(f"  conda env : {conda}")
    else:
        print("  conda env : not active")
    print()
    for name, finder in [
        ("Geant4", geant4_dir),
        ("OCC",    occ_dir),
        ("HDF5",   hdf5_dir),
        ("Qt5",    qt5_dir),
    ]:
        try:
            p = finder()
            print(f"  {name:8s}: {p}")
        except RuntimeError as e:
            print(f"  {name:8s}: NOT FOUND")
            for line in str(e).split("\n")[1:]:
                print(f"            {line}")
