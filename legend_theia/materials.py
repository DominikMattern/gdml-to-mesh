"""
legend_theia/materials.py

Builds Theia Medium objects from metadata/materials.json.
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from scipy.interpolate import CubicSpline

from theia.material import Medium
from theia.lookup import Table
from theia.property import TableProperty

from .registry import canonical

# ─────────────────────────────────────────────────────────────────────────────
# Global wavelength grid — locked per base design §4
# ─────────────────────────────────────────────────────────────────────────────

WAVELENGTH_MIN: float = 110.0   # nm
WAVELENGTH_MAX: float = 700.0   # nm
WAVELENGTH_N:   int   = 512


def _grid() -> np.ndarray:
    return np.linspace(WAVELENGTH_MIN, WAVELENGTH_MAX, WAVELENGTH_N)


def _interp(wavelengths: list[float], values: list[float]) -> np.ndarray:
    """Interpolate onto the global wavelength grid using cubic spline."""
    cs = CubicSpline(wavelengths, values, extrapolate=True)
    return cs(_grid())


def _mfp_to_coef(mfp_mm: np.ndarray) -> np.ndarray:
    """Convert mean free path in mm to absorption/scattering coefficient in 1/m."""
    return 1000.0 / np.clip(mfp_mm, 1e-6, None)


def build_media(
    materials_json: str | Path,
    *,
    wavelength_min: float = WAVELENGTH_MIN,
    wavelength_max: float = WAVELENGTH_MAX,
    n_samples: int = WAVELENGTH_N,
) -> dict[str, Medium]:
    """
    Build a dict of canonical_name → Medium from materials.json.

    Parameters
    ----------
    materials_json
        Path to metadata/materials.json produced by gdml-to-mesh.
    wavelength_min
        Minimum wavelength in nm for the simulation grid.
    wavelength_max
        Maximum wavelength in nm for the simulation grid.
    n_samples
        Number of uniform samples on the wavelength grid.

    Returns
    -------
    Dict mapping canonical material name to a Theia Medium.
    """
    with open(materials_json) as f:
        raw = json.load(f)

    entries = raw.get("materials", raw) if isinstance(raw, dict) else raw

    media: dict[str, Medium] = {}

    for entry in entries:
        if not isinstance(entry, dict):
            continue

        canon = entry.get("canonical_name") or canonical(entry.get("name", ""))
        props = entry.get("properties", {})

        kwargs: dict = {}

        def _prop(arr):
            return TableProperty(Table(arr.astype("float32")))

        if "RINDEX" in props:
            p = props["RINDEX"]
            kwargs["refractive_index"] = _prop(_interp(p["wavelength_nm"], p["values"]))

        if "GROUPVEL" in props:
            p = props["GROUPVEL"]
            kwargs["group_velocity"] = _prop(_interp(p["wavelength_nm"], p["values"]) / 1000.0)

        if "ABSLENGTH" in props:
            p = props["ABSLENGTH"]
            kwargs["absorption_coef"] = _prop(_mfp_to_coef(_interp(p["wavelength_nm"], p["values"])))

        if "RAYLEIGH" in props:
            p = props["RAYLEIGH"]
            kwargs["scattering_coef"] = _prop(_mfp_to_coef(_interp(p["wavelength_nm"], p["values"])))

        medium = Medium(canon, (wavelength_min, wavelength_max), kwargs)
        media[canon] = medium
        print(f"  medium: {canon} ({list(kwargs.keys())})")

    return media
