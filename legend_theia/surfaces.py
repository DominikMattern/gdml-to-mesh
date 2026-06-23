"""
legend_theia/surfaces.py

Builds surface lookup from metadata/surfaces.json.
Joins surfaces to interfaces via lv_pair.
"""

from __future__ import annotations

import json
from pathlib import Path


def load_surfaces(surfaces_json: str | Path) -> dict[tuple[str, str], dict]:
    """
    Load surfaces.json and index by (lv_a, lv_b) pair for fast lookup.

    Parameters
    ----------
    surfaces_json
        Path to metadata/surfaces.json produced by gdml-to-mesh.

    Returns
    -------
    Dict mapping (lv_inside, lv_outside) → surface entry. For skin suurfaces,
    the other lv is stored as None.
    Note: both orderings are stored since border surfaces are directional.
    """
    with open(surfaces_json) as f:
        surfaces = json.load(f)

    index: dict[tuple[str, str], dict] = {}

    for surf in surfaces:
        if surf.get("type") == "border":
            pair = surf.get("lv_pair", [])
            if len(pair) == 2:
                key_ab = (pair[0], pair[1])
                key_ba = (pair[1], pair[0])
                index[key_ab] = surf
                index[key_ba] = surf

        elif surf.get("type") == "skin":
            lv = surf.get("lv_skin", [])
            if len(lv) == 1:
                key_ab = (lv, None)
                key_ba = (None, lv)
                index[key_ab] = surf
                index[key_ba] = surf

    return index


def get_surface_for_interface(
    interface: dict,
    surface_index: dict[tuple[str, str], dict],
) -> dict | None:
    """
    Look up the G4 surface definition for a given interface entry.

    Parameters
    ----------
    interface
        One entry from interfaces.json.
    surface_index
        Index returned by load_surfaces().

    Returns
    -------
    Surface dict or None if no surface found.
    """
    lv_in  = interface.get("lv_inside", "")
    lv_out = interface.get("lv_outside", "")
    return (surface_index.get((lv_in, lv_out)) or surface_index.get((lv_out, lv_in))
             or surface_index.get((lv_in, None)) or surface_index.get((None, lv_in))
             or surface_index.get((lv_out, None)) or surface_index.get((None, lv_out)))
