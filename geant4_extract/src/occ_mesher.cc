#include "occ_mesher.hh"

#include "SolidConverter.hh"
#include "AssemblyBuilder.hh"
#include "MaterialExporter.hh"
#include "SurfaceExporter.hh"
#include "InterfaceExtractor.hh"
#include "SurfaceMesher.hh"

#include <G4GDMLParser.hh>
#include <G4LogicalVolume.hh>
#include <G4LogicalVolumeStore.hh>
#include <G4VSolid.hh>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Compound.hxx>

#include <BRepTools.hxx>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// ============================================================
// load GDML + convert solids
// ============================================================

void OCCMesher::run(
    const std::string& gdml_file
) {

    fs::path gdml_path =
        fs::absolute(gdml_file);

    if (!fs::exists(gdml_path)) {

        std::cerr
            << "GDML file not found:\n"
            << gdml_path
            << std::endl;

        return;
    }

    // ------------------------------------------------------------
    // IMPORTANT:
    // switch cwd so relative includes work
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

    // ------------------------------------------------------------
    // create output directories
    // ------------------------------------------------------------

    fs::create_directories("metadata");
    fs::create_directories("cad");

    // ------------------------------------------------------------
    // export materials
    // ------------------------------------------------------------

    MaterialExporter materialExporter;

    materialExporter.Export(
        "metadata/materials.json"
    );

    // ------------------------------------------------------------
    // export optical surfaces
    // (must happen after GDML parse, while G4 surface tables live)
    // ------------------------------------------------------------

    SurfaceExporter surfaceExporter;

    surfaceExporter.Export(".");

    // ------------------------------------------------------------
    // build full detector assembly
    // ------------------------------------------------------------

    auto* world =
        parser.GetWorldVolume();

    if (!world) {

        std::cerr
            << "No world volume"
            << std::endl;

        return;
    }

    AssemblyBuilder builder;

    DetectorAssembly detector =

        builder.Build(world);

    // ------------------------------------------------------------
    // extract touching interfaces
    // ------------------------------------------------------------

    InterfaceExtractor extractor;

    extractor.Extract(
        detector
    );

    // ------------------------------------------------------------
    // mesh optical interfaces
    // ------------------------------------------------------------

    SurfaceMesher mesher;

    mesher.MeshInterfaces(
        detector
    );

    // ------------------------------------------------------------
    // write OCC assembly
    // ------------------------------------------------------------

    BRepTools::Write(

        detector.assembly,

        "cad/detector.brep"
    );

    std::cout
        << "\nWrote detector assembly:\n"
        << "cad/detector.brep"
        << std::endl;

    // ------------------------------------------------------------
    // statistics
    // ------------------------------------------------------------

    std::cout
        << "\nDetector summary\n"
        << "-----------------------------\n"
        << "Volumes: "
        << detector.volumes.size()
        << "\nInterfaces: "
        << detector.interfaces.size()
        << std::endl;
}