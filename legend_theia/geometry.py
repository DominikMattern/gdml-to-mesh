"""
legend_theia/geometry.py

Scene builder for LEGEND-Theia v0.
Reads the four artifacts from gdml-to-mesh and builds a Theia Scene.

Usage
-----
    from legend_theia.geometry import build_scene
    scene, detector_map = build_scene("path/to/gdml-to-mesh/output")
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from scipy.interpolate import CubicSpline

from theia.material import Material, MaterialFlags, MaterialStore
from theia.scene import MeshStore, RectBBox, Scene, Transform, loadMesh

from .materials import WAVELENGTH_MAX, WAVELENGTH_MIN, WAVELENGTH_N, build_media
from .registry import canonical
from .surfaces import get_surface_for_interface, load_surfaces


def _surface_flags(surface_type: str) -> tuple[MaterialFlags, MaterialFlags]:
    """
    Map interface surface classification to Theia MaterialFlags.

    Parameters
    ----------
    surface_type
        One of 'specular', 'blackbody', 'detector'.

    Returns
    -------
    Tuple of (inward_flags, outward_flags).
    """
    if surface_type == "blackbody":
        return MaterialFlags.BLACK_BODY, MaterialFlags.BLACK_BODY
    if surface_type == "detector":
        return MaterialFlags.DETECTOR, MaterialFlags.DETECTOR
    return MaterialFlags(0), MaterialFlags(0)


def _detector_efficiency(
    surface: dict | None,
    wavelength_min: float,
    wavelength_max: float,
    n_samples: int,
) -> np.ndarray | None:
    """
    Extract wavelength-dependent detector efficiency from a surface entry.

    Parameters
    ----------
    surface
        Surface dict from surfaces.json, or None.
    wavelength_min
        Minimum wavelength in nm.
    wavelength_max
        Maximum wavelength in nm.
    n_samples
        Number of samples on the wavelength grid.

    Returns
    -------
    Efficiency array on the global wavelength grid, or None if not available.
    """
    if surface is None:
        return None

    mpt = surface.get("MPT", {})
    if "EFFICIENCY" not in mpt:
        return None

    eff = mpt["EFFICIENCY"]
    wl  = eff["wavelength_nm"]
    v   = eff["values"]

    cs   = CubicSpline(wl[::-1], v[::-1], extrapolate=True)
    grid = np.linspace(wavelength_min, wavelength_max, n_samples)
    return np.clip(cs(grid), 0.0, 1.0)


def build_scene(
    output_dir: str | Path,
    *,
    wavelength_min: float = WAVELENGTH_MIN,
    wavelength_max: float = WAVELENGTH_MAX,
    n_samples: int = WAVELENGTH_N,
    scene_medium: str = "lar",
    verbose: bool = True,
) -> tuple[Scene, dict[int, int]]:
    """
    Build a Theia Scene from gdml-to-mesh output artifacts.

    Parameters
    ----------
    output_dir
        Root directory containing cad/ and metadata/ subdirectories.
    wavelength_min
        Minimum wavelength in nm.
    wavelength_max
        Maximum wavelength in nm.
    n_samples
        Number of uniform samples on the wavelength grid.
    scene_medium
        Canonical name of the bulk medium the scene is immersed in.
    verbose
        Print progress information.

    Returns
    -------
    scene
        The compiled Theia Scene ready for the propagator.
    detector_map
        Dict mapping interface id → detector_id for sensitive interfaces.
    """
    output_dir = Path(output_dir)

    interfaces_path = output_dir / "metadata" / "interfaces.json"
    materials_path  = output_dir / "metadata" / "materials.json"
    surfaces_path   = output_dir / "metadata" / "surfaces.json"

    # ── 1. build media ────────────────────────────────────────────────────────
    if verbose:
        print("\n[1/5] Building media from materials.json...")
    media = build_media(
        materials_path,
        wavelength_min=wavelength_min,
        wavelength_max=wavelength_max,
        n_samples=n_samples,
    )

    # ── 2. load interfaces and surfaces ───────────────────────────────────────
    if verbose:
        print("\n[2/5] Loading interfaces and surfaces...")
    with open(interfaces_path) as f:
        interfaces = json.load(f)

    surface_index = load_surfaces(surfaces_path)

    # ── 3. build Material objects ─────────────────────────────────────────────
    if verbose:
        print("\n[3/5] Building materials...")

    materials:        dict[str, Material] = {}
    detector_map:     dict[int, int]      = {}
    detector_counter: int                 = 1

    for iface in interfaces:
        mat_inside  = canonical(iface["material_inside"])
        mat_outside = canonical(iface["material_outside"])
        surface     = iface["surface"]
        mat_name    = f"{mat_inside}__{mat_outside}__{surface}"

        if mat_name not in materials:
            flags_in, flags_out = _surface_flags(surface)

            # for detector interfaces attach efficiency curve as property
            g4_surface = get_surface_for_interface(iface, surface_index)
            properties = {}

            if surface == "detector":
                eff = _detector_efficiency(
                    g4_surface, wavelength_min, wavelength_max, n_samples
                )
                if eff is not None:
                    from theia.lookup import Table
                    from theia.property import TableProperty
                    properties["efficiency"] = TableProperty(
                        Table(eff, (wavelength_min, wavelength_max))
                    )

            materials[mat_name] = Material(
                name       = mat_name,
                inside     = media.get(mat_inside),
                outside    = media.get(mat_outside),
                flags      = (flags_in, flags_out),
                properties = properties if properties else None,
            )

            if verbose:
                print(f"  material: {mat_name}  flags=({flags_in}, {flags_out})")

        # track detector interfaces
        if surface == "detector":
            det_id = iface.get("detector_id") or detector_counter
            detector_map[iface["id"]] = det_id
            detector_counter += 1

    # ── 4. load meshes into MeshStore ─────────────────────────────────────────
    if verbose:
        print(f"\n[4/5] Loading {len(interfaces)} meshes...")

    named_meshes = []
    valid_interfaces = []

    for iface in interfaces:
        stl_path = output_dir / iface["stl"]
        if not stl_path.exists():
            print(f"  WARNING: STL not found: {stl_path}")
            continue
        mesh = loadMesh(stl_path)
        named_meshes.append((f"interface_{iface['id']}", mesh))
        valid_interfaces.append(iface)

    mesh_store = MeshStore(dict(named_meshes))

    # ── 5. create MeshInstances and compile Scene ─────────────────────────────
    if verbose:
        print("\n[5/5] Compiling scene...")

    instances = []
    identity  = Transform()

    for iface in valid_interfaces:
        mat_inside  = canonical(iface["material_inside"])
        mat_outside = canonical(iface["material_outside"])
        surface     = iface["surface"]
        mat_name    = f"{mat_inside}__{mat_outside}__{surface}"
        mesh_name   = f"interface_{iface['id']}"

        det_id   = detector_map.get(iface["id"], 0)
        instance = mesh_store.createInstance(
            mesh_name,
            mat_name,
            identity,
            detectorId=det_id,
            scale=1.0,
        )
        instances.append(instance)

    mat_store = MaterialStore(
        list(materials.values()),
        media=list(media.values()),
    )

    scene = Scene(
        instances = instances,
        materials = mat_store,
        medium    = scene_medium,
    )

    if verbose:
        print(f"\nScene built successfully:")
        print(f"  interfaces : {len(instances)}")
        print(f"  materials  : {len(materials)}")
        print(f"  media      : {len(media)}")
        print(f"  detectors  : {len(detector_map)}")
        print(f"  scene_medium: {scene_medium}")

    return scene, detector_map
