#include <G4GDMLParser.hh>

#include <G4LogicalBorderSurface.hh>
#include <G4LogicalSkinSurface.hh>
#include <G4LogicalVolumeStore.hh>

#include <G4OpticalSurface.hh>
#include <G4MaterialPropertiesTable.hh>

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------

std::string modelToString(G4OpticalSurfaceModel model) {

    switch (model) {

        case glisur:
            return "glisur";

        case unified:
            return "unified";

        case LUT:
            return "LUT";

        case DAVIS:
            return "DAVIS";

        default:
            return "unknown";
    }
}

std::string finishToString(G4OpticalSurfaceFinish finish) {

    switch (finish) {

        case polished:
            return "polished";

        case ground:
            return "ground";

        default:
            return "other";
    }
}

std::string typeToString(G4SurfaceType type) {

    switch (type) {

        case dielectric_metal:
            return "dielectric_metal";

        case dielectric_dielectric:
            return "dielectric_dielectric";

        case dielectric_LUT:
            return "dielectric_LUT";

        default:
            return "other";
    }
}

// ------------------------------------------------------------
// print optical material properties
// ------------------------------------------------------------

void printMaterialProperties(
    G4MaterialPropertiesTable* mpt
) {

    if (!mpt) {

        std::cout
            << "  No material properties table\n";

        return;
    }

    auto names =
        mpt->GetMaterialPropertyNames();

    if (names.empty()) {

        std::cout
            << "  No material properties\n";

        return;
    }

    std::cout
        << "  Material Properties:\n";

    for (const auto& n : names) {

        std::cout
            << "    - "
            << n
            << "\n";
    }
}

// ------------------------------------------------------------
// main
// ------------------------------------------------------------

int main(int argc, char** argv) {

    // ------------------------------------------------------------
    // arguments
    // ------------------------------------------------------------

    if (argc < 2) {

        std::cerr
            << "Usage:\n"
            << "  extract_surfaces <gdml_file>\n";

        return 1;
    }

    fs::path gdml_path =
        fs::absolute(argv[1]);

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

    fs::path old_cwd  =
        fs::current_path();

    fs::path gdml_dir =
        gdml_path.parent_path();

    fs::current_path(gdml_dir);

    G4GDMLParser parser;

    parser.Read(
        gdml_path.filename().string(),
        false
    );

    fs::current_path(old_cwd);

    // ============================================================
    // BORDER SURFACES
    // ============================================================

    std::cout
        << "\n========================================\n"
        << " BORDER SURFACES\n"
        << "========================================\n";

    auto borderTable =
        G4LogicalBorderSurface::GetSurfaceTable();

    if (!borderTable || borderTable->empty()) {

        std::cout
            << "No border surfaces found.\n";

    } else {

        for (const auto& entry : *borderTable) {

            auto* surf = entry.second;

            if (!surf)
                continue;

            std::cout
                << "\n----------------------------------------\n";

            std::cout
                << "Surface Name:\n"
                << "  "
                << surf->GetName()
                << "\n";

            auto* pv1 =
                surf->GetVolume1();

            auto* pv2 =
                surf->GetVolume2();

            if (pv1) {

                std::cout
                    << "Volume1:\n"
                    << "  "
                    << pv1->GetName()
                    << "\n";
            }

            if (pv2) {

                std::cout
                    << "Volume2:\n"
                    << "  "
                    << pv2->GetName()
                    << "\n";
            }

            auto* opt =
                dynamic_cast<G4OpticalSurface*>(
                    surf->GetSurfaceProperty()
                );

            if (!opt) {

                std::cout
                    << "No optical surface attached\n";

                continue;
            }

            std::cout
                << "Model:\n"
                << "  "
                << modelToString(opt->GetModel())
                << "\n";

            std::cout
                << "Finish:\n"
                << "  "
                << finishToString(opt->GetFinish())
                << "\n";

            std::cout
                << "Type:\n"
                << "  "
                << typeToString(opt->GetType())
                << "\n";

            auto* mpt =
                opt->GetMaterialPropertiesTable();

            printMaterialProperties(mpt);
        }
    }

    // ============================================================
    // SKIN SURFACES
    // ============================================================

    std::cout
        << "\n========================================\n"
        << " SKIN SURFACES\n"
        << "========================================\n";

    auto lvStore =
        G4LogicalVolumeStore::GetInstance();

    for (auto* lv : *lvStore) {

        if (!lv)
            continue;

        auto* skin =
            G4LogicalSkinSurface::GetSurface(lv);

        if (!skin)
            continue;

        std::cout
            << "\n----------------------------------------\n";

        std::cout
            << "Logical Volume:\n"
            << "  "
            << lv->GetName()
            << "\n";

        std::cout
            << "Skin Surface:\n"
            << "  "
            << skin->GetName()
            << "\n";

        auto* opt =
            dynamic_cast<G4OpticalSurface*>(
                skin->GetSurfaceProperty()
            );

        if (!opt)
            continue;

        std::cout
            << "Model:\n"
            << "  "
            << modelToString(opt->GetModel())
            << "\n";

        std::cout
            << "Finish:\n"
            << "  "
            << finishToString(opt->GetFinish())
            << "\n";

        std::cout
            << "Type:\n"
            << "  "
            << typeToString(opt->GetType())
            << "\n";

        auto* mpt =
            opt->GetMaterialPropertiesTable();

        printMaterialProperties(mpt);
    }

    std::cout
        << "\nDone.\n";

    return 0;
}

