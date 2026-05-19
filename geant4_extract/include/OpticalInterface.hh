#pragma once

#include <string>

#include <TopoDS_Shape.hxx>

struct OpticalInterface {

    int id;

    int volumeA;
    int volumeB;

    std::string nameA;
    std::string nameB;

    std::string materialA;
    std::string materialB;

    TopoDS_Shape boundary;

    // populated by SurfaceMesher
    int    n_triangles = 0;
    double area_mm2    = 0.0;

    // populated by InterfaceExtractor::Extract
    // "blackbody" | "specular" | "detector"
    std::string surface_hint = "specular";
};
