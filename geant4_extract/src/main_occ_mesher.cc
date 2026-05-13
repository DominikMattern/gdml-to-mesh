#include "occ_mesher.hh"

#include <iostream>

int main(int argc, char** argv) {

    if (argc < 2) {

        std::cerr
            << "Usage:\n"
            << "  occ_mesher <gdml_file>\n";

        return 1;
    }

    OCCMesher mesher;

    mesher.run(argv[1]);

    return 0;
}
