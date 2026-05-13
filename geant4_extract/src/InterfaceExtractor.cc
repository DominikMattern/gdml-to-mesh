#include "InterfaceExtractor.hh"

#include <VolumeInstance.hh>
#include <OpticalInterface.hh>

#include <BRepAlgoAPI_Common.hxx>

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <TopoDS_Shape.hxx>

#include <iostream>
#include <string>

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