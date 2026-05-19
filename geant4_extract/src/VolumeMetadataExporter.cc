#include "VolumeMetadataExporter.hh"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>

using json = nlohmann::json;

void VolumeMetadataExporter::ExportVolumes(const DetectorAssembly& assembly, const std::string& filename)
{
    json j;
    j["volumes"] = json::array();

    for (const auto& vol : assembly.volumes) {
        json volJson;
        volJson["id"] = vol.id;
        volJson["name"] = vol.name;
        volJson["material"] = vol.material;
        volJson["parent_id"] = vol.parent_id;

        // Assuming translation and rotation are std::vector<double>
        volJson["translation"] = vol.translation;
        volJson["rotation"] = vol.rotation;

        // Add mesh filename if you have it, e.g.
        // volJson["mesh_file"] = vol.mesh_file;

        j["volumes"].push_back(volJson);
    }

    std::ofstream out(filename);
    if (!out) {
        std::cerr << "ERROR: Could not open file for writing: " << filename << std::endl;
        return;
    }

    out << std::setw(2) << j << std::endl;

    std::cout << "Exported volume metadata to " << filename << std::endl;
}
