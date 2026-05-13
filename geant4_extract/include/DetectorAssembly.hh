#pragma once

#include "VolumeInstance.hh"
#include "OpticalInterface.hh"

#include <TopoDS_Compound.hxx>

#include <vector>

// ============================================================
// full detector container
// ============================================================

struct DetectorAssembly {

    // full CAD assembly
    TopoDS_Compound assembly;

    // all placed detector volumes
    std::vector<VolumeInstance> volumes;

    // touching optical boundaries
    std::vector<OpticalInterface> interfaces;
};
