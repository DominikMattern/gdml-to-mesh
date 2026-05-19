#pragma once

#include "DetectorAssembly.hh"
#include <string>

class VolumeMetadataExporter {
public:
    static void ExportVolumes(const DetectorAssembly& assembly, const std::string& filename);
};
