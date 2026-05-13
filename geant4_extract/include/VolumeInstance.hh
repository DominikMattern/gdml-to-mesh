#pragma once

#include <TopoDS_Shape.hxx>

#include <string>
#include <cstdint>

struct VolumeInstance {

    uint64_t id;

    std::string name;

    std::string material;

    TopoDS_Shape shape;
};
