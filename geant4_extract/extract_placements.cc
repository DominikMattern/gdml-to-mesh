#include <G4BooleanSolid.hh>
#include <G4GDMLParser.hh>
#include <G4LogicalVolume.hh>
#include <G4Material.hh>
#include <G4PhysicalVolumeStore.hh>
#include <G4Polyhedron.hh>
#include <G4RotationMatrix.hh>
#include <G4SystemOfUnits.hh>
#include <G4ThreeVector.hh>
#include <G4VPhysicalVolume.hh>
#include <G4VSolid.hh>

#include "cnpy/cnpy.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// material mapping
// ============================================================
int material_id(const std::string &mat) {
  if (mat.find("LAr") != std::string::npos)
    return 0;
  if (mat.find("Germanium") != std::string::npos)
    return 1;
  if (mat.find("ICPC") != std::string::npos)
    return 2;
  if (mat.find("PEN") != std::string::npos)
    return 3;
  return 99;
}

// ============================================================
// mesh mapping (simple solids only)
// ============================================================
int mesh_id(const std::string &solid) {
  if (solid == "lar_s")
    return 0;
  if (solid == "bege_lv")
    return 1;
  if (solid == "icpc_lv")
    return 2;
  if (solid == "pen_bege_pc_s")
    return 3;
  if (solid == "pen_icpc_pc_s")
    return 4;
  return -1;
}

// ============================================================
// main
// ============================================================
int main(int argc, char **argv) {

  if (argc < 2) {
    std::cerr << "Usage:\n"
              << "  extract_placements <gdml_file>\n";
    return 1;
  }

  fs::path gdml_path = fs::absolute(argv[1]);
  if (!fs::exists(gdml_path)) {
    std::cerr << "ERROR: GDML file not found: " << gdml_path << "\n";
    return 1;
  }

  // ============================================================
  // FORCE HIGH-RES TESSELLATION (CRITICAL TEST)
  // ============================================================
  // Default is ~24; try 72 first
  G4Polyhedron::SetNumberOfRotationSteps(72);

  // ------------------------------------------------------------
  // read GDML (directory-safe)
  // ------------------------------------------------------------
  fs::path old_cwd = fs::current_path();
  fs::path gdml_dir = gdml_path.parent_path();

  fs::current_path(gdml_dir);
  G4GDMLParser parser;
  parser.Read(gdml_path.filename().string(), false);
  fs::current_path(old_cwd);

  auto pvStore = G4PhysicalVolumeStore::GetInstance();
  if (!pvStore) {
    std::cerr << "ERROR: No physical volume store\n";
    return 1;
  }

  // ------------------------------------------------------------
  // output directories
  // ------------------------------------------------------------
  fs::path mesh_outdir = fs::current_path() / "meshes" / "boolean";
  fs::path place_out = fs::current_path() / "meshes";

  fs::create_directories(mesh_outdir);
  fs::create_directories(place_out);

  // ------------------------------------------------------------
  // placement arrays
  // ------------------------------------------------------------
  std::vector<float> translations;
  std::vector<float> rotations;
  std::vector<int> mesh_ids;
  std::vector<int> material_ids;

  std::unordered_set<const G4LogicalVolume *> extracted_boolean_lvs;
  size_t n_boolean_written = 0;

  // ------------------------------------------------------------
  // iterate physical volumes (OPTION A CORE)
  // ------------------------------------------------------------
  for (auto pv : *pvStore) {
    if (!pv)
      continue;

    auto lv = pv->GetLogicalVolume();
    if (!lv)
      continue;

    auto solid = lv->GetSolid();
    if (!solid)
      continue;

    const std::string &solid_name = solid->GetName();

    // ========================================================
    // 1) placement metadata
    // ========================================================
    int mid = mesh_id(solid_name);
    if (mid >= 0) {
      auto t = pv->GetTranslation();
      auto r = pv->GetRotation();

      translations.insert(
          translations.end(),
          {(float)(t.x() / mm), (float)(t.y() / mm), (float)(t.z() / mm)});

      if (r) {
        for (int i = 0; i < 3; ++i)
          for (int j = 0; j < 3; ++j)
            rotations.push_back((*r)(i, j));
      } else {
        rotations.insert(rotations.end(), {1, 0, 0, 0, 1, 0, 0, 0, 1});
      }

      mesh_ids.push_back(mid);

      auto mat = lv->GetMaterial();
      material_ids.push_back(mat ? material_id(mat->GetName()) : 99);
    }

    // ========================================================
    // 2) boolean / union geometry extraction (FORCED)
    // ========================================================
    if (!dynamic_cast<G4BooleanSolid *>(solid))
      continue;

    if (extracted_boolean_lvs.count(lv))
      continue;

    extracted_boolean_lvs.insert(lv);

    std::cout << "Extracting boolean solid via placement: " << solid_name
              << "\n";

    // ---- SAFE polyhedron creation ----
    solid->CreatePolyhedron();
    G4Polyhedron *poly = solid->GetPolyhedron();

    if (!poly) {
      std::cerr << "  -> no polyhedron even after forcing rotation steps\n";
      continue;
    }

    G4RotationMatrix rot;
    if (pv->GetRotation())
      rot = *(pv->GetRotation());

    G4ThreeVector trans = pv->GetTranslation();

    std::vector<float> vertices;
    std::vector<int> indices;

    for (int i = 1; i <= poly->GetNoVertices(); ++i) {
      G4ThreeVector v = poly->GetVertex(i);
      v = rot * v + trans;

      vertices.push_back(v.x() / mm);
      vertices.push_back(v.y() / mm);
      vertices.push_back(v.z() / mm);
    }

    for (int i = 1; i <= poly->GetNoFacets(); ++i) {
      G4int n;
      G4int idx[4];
      poly->GetFacet(i, n, idx);

      if (n == 3) {
        indices.insert(indices.end(), {idx[0] - 1, idx[1] - 1, idx[2] - 1});
      } else if (n == 4) {
        indices.insert(indices.end(), {idx[0] - 1, idx[1] - 1, idx[2] - 1,
                                       idx[0] - 1, idx[2] - 1, idx[3] - 1});
      }
    }

    fs::path filename = mesh_outdir / (solid_name + ".npz");

    cnpy::npz_save(filename.string(), "vertices", vertices.data(),
                   {vertices.size() / 3, 3}, "w");

    cnpy::npz_save(filename.string(), "indices", indices.data(),
                   {indices.size() / 3, 3}, "a");

    ++n_boolean_written;
  }

  // ------------------------------------------------------------
  // write placements.npz
  // ------------------------------------------------------------
  fs::path placements_file = place_out / "placements.npz";

  cnpy::npz_save(placements_file.string(), "translations", translations.data(),
                 {translations.size() / 3, 3}, "w");

  cnpy::npz_save(placements_file.string(), "rotations", rotations.data(),
                 {rotations.size() / 9, 9}, "a");

  cnpy::npz_save(placements_file.string(), "mesh_ids", mesh_ids.data(),
                 {mesh_ids.size()}, "a");

  cnpy::npz_save(placements_file.string(), "material_ids", material_ids.data(),
                 {material_ids.size()}, "a");

  // ------------------------------------------------------------
  // summary
  // ------------------------------------------------------------
  std::cout << "\nSummary (extract_placements)\n";
  std::cout << "  placement instances : " << mesh_ids.size() << "\n";
  std::cout << "  boolean meshes      : " << n_boolean_written << "\n";
  std::cout << "Done.\n";

  return 0;
}
