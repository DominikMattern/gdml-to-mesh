#pragma once

#include "DetectorAssembly.hh"
#include <string>
#include <vector>

class VolumeSTLExporter {
public:
    // Export selected volumes to cad/volumes/<name>.stl
    // If names is empty, exports all volumes.
    void Export(
        const DetectorAssembly& assembly,
        const std::string& outDir = "cad/volumes",
        const std::vector<std::string>& names = {}
    );
};
