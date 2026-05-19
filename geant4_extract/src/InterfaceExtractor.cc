#include "InterfaceExtractor.hh"

#include <VolumeInstance.hh>
#include <OpticalInterface.hh>

#include <BRepAlgoAPI_Common.hxx>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <TopoDS_Shape.hxx>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>

using json = nlohmann::json;

// ============================================================
// bbox overlap helper
// ============================================================

static bool BoxesOverlap(

    const TopoDS_Shape& a,
    const TopoDS_Shape& b
) {

    Bnd_Box boxA;
    Bnd_Box boxB;

    BRepBndLib::Add(a, boxA);
    BRepBndLib::Add(b, boxB);

    return !boxA.IsOut(boxB);
}

// ============================================================
// extract interfaces
// ============================================================

void InterfaceExtractor::Extract(

    DetectorAssembly& assembly
) {

    auto& volumes =
        assembly.volumes;

    std::cout
        << "\nChecking interfaces...\n"
        << std::endl;

    for (size_t i = 0;
         i < volumes.size();
         ++i)
    {

        for (size_t j = i + 1;
             j < volumes.size();
             ++j)
        {

            const auto& A =
                volumes[i];

            const auto& B =
                volumes[j];

            // ------------------------------------------------
            // skip same materials
            // ------------------------------------------------

            if (A.material == B.material)
                continue;

            // ------------------------------------------------
            // ONLY keep BeGE ↔ liquid argon
            // ------------------------------------------------

            bool A_is_lar =

                (A.material == "liquid_argon");

            bool B_is_lar =

                (B.material == "liquid_argon");

            bool A_is_bege =

                (A.name.find("bege") != std::string::npos);

            bool B_is_bege =

                (B.name.find("bege") != std::string::npos);

            bool valid_interface =

                (A_is_bege && B_is_lar) ||
                (B_is_bege && A_is_lar);

            if (!valid_interface)
                continue;

            // ------------------------------------------------
            // bbox reject
            // ------------------------------------------------

            if (!BoxesOverlap(
                    A.shape,
                    B.shape))
            {
                continue;
            }

            // ------------------------------------------------
            // OCC intersection
            // ------------------------------------------------

            BRepAlgoAPI_Common common(

                A.shape,
                B.shape
            );

            common.Build();

            if (!common.IsDone())
                continue;

            TopoDS_Shape result =
                common.Shape();

            if (result.IsNull())
                continue;

            // ------------------------------------------------
            // construct interface object
            // ------------------------------------------------

            OpticalInterface iface;

            iface.id =
                assembly.interfaces.size();

            iface.volumeA =
                A.id;

            iface.volumeB =
                B.id;

            iface.nameA =
                A.name;

            iface.nameB =
                B.name;

            iface.materialA =
                A.material;

            iface.materialB =
                B.material;

            iface.boundary =
                result;

            assembly.interfaces.push_back(
                iface
            );

            // ------------------------------------------------
            // debug output
            // ------------------------------------------------

            std::cout
                << "INTERFACE:\n  "
                << A.name
                << " ("
                << A.material
                << ")\n  ↔ "
                << B.name
                << " ("
                << B.material
                << ")\n"
                << std::endl;
        }
    }

    std::cout
        << "\nTotal interfaces found: "
        << assembly.interfaces.size()
        << std::endl;
}

// ============================================================
// write metadata/interfaces.json
// called after SurfaceMesher has populated n_triangles/area_mm2
// ============================================================

void InterfaceExtractor::WriteInterfacesJSON(
    const DetectorAssembly& assembly,
    const std::string& outDir)
{
    json arr = json::array();

    for (const auto& iface : assembly.interfaces) {

        // ----------------------------------------------------
        // orient sides: lv_inside = non-LAr, lv_outside = LAr
        // convention matches design doc §3.1
        // ----------------------------------------------------

        bool A_is_lar = (iface.materialA == "liquid_argon");

        std::string lv_inside    = A_is_lar ? iface.nameB     : iface.nameA;
        std::string lv_outside   = A_is_lar ? iface.nameA     : iface.nameB;
        std::string mat_inside   = A_is_lar ? iface.materialB : iface.materialA;
        std::string mat_outside  = A_is_lar ? iface.materialA : iface.materialB;

        // ----------------------------------------------------
        // surface type classification (v0 rules, design doc §1)
        //
        // Classify by MATERIAL not by LV name to avoid
        // substring collisions (e.g. "pen_bege_pv" contains
        // "bege" but is PEN, not germanium).
        //
        //   germanium → blackbody  (HPGe, v0 approximation)
        //   sipm name → detector   (sensitive, gets detector_id)
        //   everything else → specular
        //     covers PEN, TPB, fibers, copper, tetratex, etc.
        //     WLS and diffuse surfaces are deferred to v1
        // ----------------------------------------------------

        std::string surface = "specular";
        json        det_id  = nullptr;

        if (mat_inside == "EnrichedGermanium0.076" ||
            mat_inside == "NaturalGermanium")
        {
            surface = "blackbody";
        }
        else if (lv_inside.find("sipm") != std::string::npos)
        {
            surface = "detector";
            det_id  = iface.id;
        }
        // else: specular — PEN, fibers, copper, world boundary, etc.

        // ----------------------------------------------------
        // build JSON entry
        // ----------------------------------------------------

        json entry;
        entry["id"]               = iface.id;
        entry["stl"]              = "cad/interfaces/interface_"
                                     + std::to_string(iface.id) + ".stl";
        entry["lv_inside"]        = lv_inside;
        entry["lv_outside"]       = lv_outside;
        entry["material_inside"]  = mat_inside;
        entry["material_outside"] = mat_outside;
        entry["surface"]          = surface;
        entry["detector_id"]      = det_id;
        entry["n_triangles"]      = iface.n_triangles;
        entry["area_mm2"]         = iface.area_mm2;

        arr.push_back(entry);
    }

    // --------------------------------------------------------
    // write file
    // --------------------------------------------------------

    std::string path = outDir + "/metadata/interfaces.json";
    std::ofstream f(path);

    if (!f) {
        std::cerr << "ERROR: could not write " << path << std::endl;
        return;
    }

    f << std::setw(2) << arr << std::endl;

    std::cout << "Wrote " << path << std::endl;
}