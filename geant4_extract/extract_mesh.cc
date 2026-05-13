#include <G4GDMLParser.hh>

#include <G4LogicalVolumeStore.hh>
#include <G4LogicalSkinSurface.hh>

#include <G4Material.hh>

#include <G4Polyhedron.hh>
#include <G4SystemOfUnits.hh>
#include <G4VSolid.hh>

#include "cnpy/cnpy.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ------------------------------------------------------------
// main
// ------------------------------------------------------------

int main(int argc, char **argv) {

    // finer curved tessellation
    G4Polyhedron::SetNumberOfRotationSteps(128);

    // ------------------------------------------------------------
    // argument parsing
    // ------------------------------------------------------------

    if (argc < 3) {

        std::cerr
            << "Usage:\n"
            << "  extract_mesh <gdml_file> <list|fiber|pen|lar|bege|icpc>\n";

        return 1;
    }

    fs::path gdml_path = fs::absolute(argv[1]);

    std::string mode = argv[2];

    if (mode != "list" &&
        mode != "fiber" &&
        mode != "pen" &&
        mode != "lar" &&
        mode != "bege" &&
        mode != "icpc") {

        std::cerr
            << "Invalid mode: "
            << mode
            << "\n";

        return 1;
    }

    if (!fs::exists(gdml_path)) {

        std::cerr
            << "ERROR: GDML file not found:\n"
            << "  "
            << gdml_path
            << "\n";

        return 1;
    }

    // ------------------------------------------------------------
    // safe GDML loading
    // ------------------------------------------------------------

    fs::path old_cwd  = fs::current_path();
    fs::path gdml_dir = gdml_path.parent_path();

    fs::current_path(gdml_dir);

    G4GDMLParser parser;

    parser.Read(
        gdml_path.filename().string(),
        false
    );

    fs::current_path(old_cwd);

    auto lvStore =
        G4LogicalVolumeStore::GetInstance();

    if (!lvStore) {

        std::cerr
            << "ERROR: No logical volume store found\n";

        return 1;
    }

    // ------------------------------------------------------------
    // LIST MODE
    // ------------------------------------------------------------

    if (mode == "list") {

        std::unordered_set<std::string> seen;

        std::cout
            << "\n=== Solid names in GDML ===\n";

        for (auto lv : *lvStore) {

            if (!lv || !lv->GetSolid())
                continue;

            const std::string &name =
                lv->GetSolid()->GetName();

            if (seen.insert(name).second)
                std::cout << name << "\n";
        }

        std::cout
            << "==========================\n";

        return 0;
    }

    // ------------------------------------------------------------
    // SOLID SELECTION SETS
    // ------------------------------------------------------------

    const std::unordered_set<std::string>
    fiber_solids = {

        "fiber_core_l200_bNone",
        "fiber_cl1_l200_bNone",
        "fiber_cl2_l200_bNone",
        "fiber_coating_l200_tpb150"
    };

    const std::unordered_set<std::string>
    pen_solids = {

        "pen_bege_pc_s",
        "pen_icpc_pc_s"
    };

    // ------------------------------------------------------------
    // output directory
    // ------------------------------------------------------------

    fs::path outdir =
        fs::current_path() / "meshes" / mode;

    fs::create_directories(outdir);

    std::unordered_set<std::string>
    written_solids;

    size_t n_written = 0;
    size_t n_skipped_poly = 0;
    size_t n_skipped_filter = 0;

    // ------------------------------------------------------------
    // iterate logical volumes
    // ------------------------------------------------------------

    for (auto lv : *lvStore) {

        if (!lv)
            continue;

        G4VSolid *solid =
            lv->GetSolid();

        if (!solid)
            continue;

        const std::string &name =
            solid->GetName();

        bool accept = false;

        if (mode == "fiber") {

            accept =
                (fiber_solids.count(name) > 0);

        } else if (mode == "pen") {

            accept =
                (pen_solids.count(name) > 0);

        } else if (mode == "lar") {

            accept =
                (name == "lar_s");

        } else if (mode == "bege") {

            accept =
                (name == "bege_lv");

        } else if (mode == "icpc") {

            accept =
                (name == "icpc_lv");
        }

        if (!accept) {

            ++n_skipped_filter;
            continue;
        }

        if (written_solids.count(name))
            continue;

        written_solids.insert(name);

        std::cout
            << "\nExtracting ["
            << mode
            << "] solid:\n  "
            << name
            << "\n";

        // ------------------------------------------------------------
        // semantic metadata
        // ------------------------------------------------------------

        std::cout
            << "  Logical Volume:\n    "
            << lv->GetName()
            << "\n";

        auto* material =
            lv->GetMaterial();

        if (material) {

            std::cout
                << "  Material:\n    "
                << material->GetName()
                << "\n";
        }

        auto* skin =
            G4LogicalSkinSurface::GetSurface(lv);

        if (skin) {

            std::cout
                << "  Skin Surface:\n    "
                << skin->GetName()
                << "\n";
        }

        // ------------------------------------------------------------
        // polyhedron extraction
        // ------------------------------------------------------------

        solid->CreatePolyhedron();

        G4Polyhedron *poly =
            solid->GetPolyhedron();

        if (!poly) {

            std::cerr
                << "  -> no polyhedron, skipping\n";

            ++n_skipped_poly;

            continue;
        }

        std::vector<float> vertices;
        std::vector<int> indices;

        int nVert =
            poly->GetNoVertices();

        vertices.reserve(nVert * 3);

        for (int i = 1; i <= nVert; ++i) {

            auto v =
                poly->GetVertex(i);

            vertices.push_back(v.x() / mm);
            vertices.push_back(v.y() / mm);
            vertices.push_back(v.z() / mm);
        }

        int nFace =
            poly->GetNoFacets();

        indices.reserve(nFace * 3);

        std::cout
            << "  Mesh Stats:\n"
            << "    vertices : "
            << nVert
            << "\n"
            << "    faces    : "
            << nFace
            << "\n";

        for (int i = 1; i <= nFace; ++i) {

            G4int n;
            G4int idx[4];

            poly->GetFacet(i, n, idx);

            if (n == 3) {

                indices.insert(
                    indices.end(),
                    {
                        idx[0]-1,
                        idx[1]-1,
                        idx[2]-1
                    }
                );

            } else if (n == 4) {

                indices.insert(
                    indices.end(),
                    {
                        idx[0]-1,
                        idx[1]-1,
                        idx[2]-1,

                        idx[0]-1,
                        idx[2]-1,
                        idx[3]-1
                    }
                );
            }
        }

        if (vertices.empty() ||
            indices.empty()) {

            std::cerr
                << "  -> empty mesh, skipping\n";

            continue;
        }

        // ------------------------------------------------------------
        // save mesh
        // ------------------------------------------------------------

        fs::path filename =
            outdir / (name + ".npz");

        cnpy::npz_save(
            filename.string(),
            "vertices",
            vertices.data(),
            {vertices.size() / 3, 3},
            "w"
        );

        cnpy::npz_save(
            filename.string(),
            "indices",
            indices.data(),
            {indices.size() / 3, 3},
            "a"
        );

        // ------------------------------------------------------------
        // save semantic metadata
        // ------------------------------------------------------------

        fs::path metafile =
            outdir / (name + ".txt");

        std::ofstream meta(metafile);

        meta << "solid: "
             << name
             << "\n";

        meta << "logical_volume: "
             << lv->GetName()
             << "\n";

        if (material) {

            meta << "material: "
                 << material->GetName()
                 << "\n";
        }

        if (skin) {

            meta << "skin_surface: "
                 << skin->GetName()
                 << "\n";
        }

        meta.close();

        std::cout
            << "  -> wrote:\n"
            << "     "
            << filename
            << "\n";

        ++n_written;
    }

    // ------------------------------------------------------------
    // summary
    // ------------------------------------------------------------

    std::cout
        << "\n========================================\n"
        << "Summary for mode = "
        << mode
        << "\n"
        << "========================================\n";

    std::cout
        << "written meshes     : "
        << n_written
        << "\n";

    std::cout
        << "skipped (filter)   : "
        << n_skipped_filter
        << "\n";

    std::cout
        << "skipped (no poly)  : "
        << n_skipped_poly
        << "\n";

    std::cout
        << "Done.\n";

    return 0;
}
