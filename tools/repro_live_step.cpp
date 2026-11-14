#include "octo/dynamics.hpp"
#include <fstream>
#include <iostream>
using namespace octo::dynamics;
int main(int argc, char **argv) {
    try {
        if (argc < 2)
            return 2;
        System s;
        std::ifstream input(argv[1]);
        s.restore(input);
        if(argc>2)s.diagnostics=&std::cerr;
        std::cout << "loaded_tick=" << s.state.tick << " bodies=" << s.state.bodies.size()
                  << std::endl;
        auto r = s.step();
        std::cout << "converged=" << r.converged << " residual=" << r.residual
                  << " outer=" << r.outer_iterations << " geometry=" << r.constraint_error
                  << " dual=" << r.friction_error << " substeps=" << r.substeps
                  << " message=" << r.failure << std::endl;
        if (!r.converged || r.penetration >= .002)
            return 1;
        auto &box = s.state.bodies.back();
        box.force = {};
        box.torque = {};
        for (unsigned j = 0; j < 60; ++j) {
            auto next = s.step();
            if (!next.converged || next.penetration >= .002) {
                std::cerr << "continuation tick=" << s.state.tick << " " << next.failure << '\n';
                return 1;
            }
        }
        std::cout << "unloaded_continuation_ticks=60 passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
