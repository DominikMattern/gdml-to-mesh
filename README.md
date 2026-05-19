# gdml-to-mesh

Converts GDML detector geometry files into triangulated mesh outputs for optical simulation and visualization. Built for the [LEGEND-Theia](https://legend-exp.org) optical photon transport pipeline.

---

## Installation

### Option 1 — conda (recommended)

This installs everything including Geant4 and OpenCASCADE from conda-forge:

```bash
git clone https://github.com/maninder1apr/gdml-to-mesh.git
cd gdml-to-mesh

conda env create -f environment.yml
conda activate gdml-to-mesh
```

That's it. The `environment.yml` installs all C++ and Python dependencies and builds the package.

### Option 2 — manual (macOS with Homebrew)

Install C++ dependencies:

```bash
brew install cmake opencascade qt@5 hdf5
```

Install Geant4 manually (requires CMake build):

```bash
git clone https://github.com/Geant4/geant4.git
cd geant4 && mkdir build && cd build
cmake .. -DGEANT4_USE_GDML=ON -DCMAKE_INSTALL_PREFIX=~/geant4-install
make -j8 && make install
export GEANT4_INSTALL=~/geant4-install
```

Then install the Python package:

```bash
git clone https://github.com/maninder1apr/gdml-to-mesh.git
cd gdml-to-mesh
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
```

### Custom dependency paths

If your dependencies are in non-standard locations, set these environment variables before running `pip install -e .`:

```bash
export GEANT4_INSTALL=/path/to/geant4-install
export OCC_INSTALL=/path/to/opencascade
export HDF5_INSTALL=/path/to/hdf5-install
export Qt5_DIR=/path/to/qt5
```

---

## System requirements

| Dependency | Version | Install |
|-----------|---------|---------|
| Python | ≥ 3.9 | `conda install python=3.11` |
| Geant4 | ≥ 11 (with GDML) | `conda install -c conda-forge geant4` |
| OpenCASCADE | ≥ 7.6 | `conda install -c conda-forge opencascade` |
| HDF5 | any | `conda install -c conda-forge hdf5` |
| Qt5 | 5.x | `conda install -c conda-forge qt` |
| CMake | ≥ 3.16 | `conda install -c conda-forge cmake` |
| trimesh | any | installed automatically |
| matplotlib | any | installed automatically |
| numpy | any | installed automatically |
| scipy | any | installed automatically |

---

## Verify installation

```bash
gdml-to-mesh check
```

Expected output:

```
gdml-to-mesh dependency detection:
  conda env : /path/to/conda/envs/gdml-to-mesh
  Geant4   : /path/to/geant4
  OCC      : /path/to/opencascade
  HDF5     : /path/to/hdf5
  Qt5      : /path/to/qt5

  binary built : True
  binary path  : .../geant4_extract/build/occ_mesher
```

---

## Usage

### Command line

```bash
# run the geometry engine on a GDML file
gdml-to-mesh run gdml/scarf_pen.gdml

# with verbose output
gdml-to-mesh run gdml/scarf_pen.gdml --verbose

# write outputs to a specific directory
gdml-to-mesh run gdml/scarf_pen.gdml --output-dir ./output

# rebuild the C++ binary
gdml-to-mesh build

# launch the visualizer
gdml-to-mesh visualize
gdml-to-mesh visualize --surface detector
gdml-to-mesh visualize --surface blackbody
gdml-to-mesh visualize --surface specular
```

### Python API

```python
from gdml_to_mesh import run, load_interfaces, load_materials

# run the geometry engine
result = run("gdml/scarf_pen.gdml")

print(result.summary())
# GeometryResult
#   interfaces : 298
#     specular    : 272
#     blackbody   : 14
#     detector    : 12
#   materials  : 9
#   surfaces   : 554

# access the data
for iface in result.interfaces:
    print(iface["lv_inside"], iface["surface"], iface["area_mm2"])

# load previously generated outputs
interfaces = load_interfaces("output/metadata/interfaces.json")
materials  = load_materials("output/metadata/materials.json")
```

---

## Output structure

```
cad/
  interfaces/
    interface_0.stl       # LAr ↔ BeGe boundary (blackbody)
    interface_1.stl       # LAr ↔ PEN boundary (specular)
    ...                   # 298 total for scarf_pen.gdml
  volumes/
    lar_pv.stl
    bege_pv.stl
    pen_bege_pv.stl
    icpc_pv.stl
    sipm_top_0.stl
    ...
  detector.brep

metadata/
  interfaces.json         # one entry per STL
  materials.json          # optical MPT tables with canonical LEGEND names
  surfaces.json           # G4LogicalBorderSurface entries with MPT
```

### Interface surface types

| Type | Volumes | Count |
|------|---------|-------|
| `blackbody` | HPGe (BeGe, ICPC) + SiPM Cu wraps | 14 |
| `specular` | PEN encapsulations + TPB fiber coatings | 272 |
| `detector` | SiPM silicon photocathodes | 12 |

---

## Visualizer

```bash
# all interfaces + detector volumes
python3 geant4_extract/visualize.py

# filter by surface type
python3 geant4_extract/visualize.py --surface detector
python3 geant4_extract/visualize.py --surface blackbody
python3 geant4_extract/visualize.py --surface specular

# inspect a single interface with boundary validation
python3 geant4_extract/visualize_single.py --id 0
python3 geant4_extract/visualize_single.py --id 1
```

Colors: 🔴 blackbody · 🔵 specular · 🟢 detector

---

## Metadata format

### `interfaces.json`

```json
[
  {
    "id": 0,
    "stl": "cad/interfaces/interface_0.stl",
    "lv_inside": "bege_pv",
    "lv_outside": "lar_pv",
    "material_inside": "EnrichedGermanium0.076",
    "material_outside": "liquid_argon",
    "surface": "blackbody",
    "detector_id": null,
    "n_triangles": 2688,
    "area_mm2": 15216.9
  }
]
```

### `materials.json`

Exports all Geant4 optical material property tables (RINDEX, RAYLEIGH, ABSLENGTH, GROUPVEL, scintillation components) with a `canonical_name` field mapping G4 material names to LEGEND registry keys.

### `surfaces.json`

Exports all `G4LogicalBorderSurface` and `G4LogicalSkinSurface` entries with model, finish, type, and full wavelength-indexed MPT tables. 554 surfaces for `scarf_pen.gdml`.

---

## Surface classification

| Material | Surface type | Notes |
|----------|-------------|-------|
| `EnrichedGermanium0.076`, `NaturalGermanium` | `blackbody` | HPGe approximation |
| `PEN` | `specular` | WLS not yet implemented |
| `tpb_on_fibers` | `specular` | Diffuse/WLS not yet implemented |
| `metal_silicon` | `detector` | SiPM photocathode |
| `metal_copper` | `blackbody` | SiPM copper wrap |

---

## License

See [LICENSE](LICENSE).
