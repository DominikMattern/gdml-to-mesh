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
};