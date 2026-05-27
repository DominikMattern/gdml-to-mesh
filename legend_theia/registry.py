"""
legend_theia/registry.py

Canonical material name registry.
Maps G4 material names → legend_theia canonical names.
Lives here (not in gdml-to-mesh) per base design Q2.
"""

from __future__ import annotations

CANONICAL_NAMES: dict[str, str] = {
    "liquid_argon":           "lar",
    "G4_lAr":                 "lar",
    "PEN":                    "pen",
    "EnrichedGermanium0.076": "germanium_enriched",
    "NaturalGermanium":       "germanium_natural",
    "tpb_on_fibers":          "tpb",
    "ps_fibers":              "bcf91a_core",
    "pmma":                   "bcf91a_cladding_1",
    "pmma_cl2":               "bcf91a_cladding_2",
    "metal_silicon":          "sipm_photocathode",
    "metal_copper":           "copper",
}


def canonical(g4_name: str) -> str:
    """
    Map a G4 material name to a canonical legend_theia name.

    Falls back to lowercase of the G4 name if not in registry.

    Parameters
    ----------
    g4_name
        Geant4 material name as it appears in materials.json or interfaces.json.

    Returns
    -------
    Canonical lowercase_underscore name.
    """
    return CANONICAL_NAMES.get(g4_name, g4_name.lower())
