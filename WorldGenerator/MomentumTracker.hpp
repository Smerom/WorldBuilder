// --
//  MomentumTracker.hpp
//  WorldGenerator
//


#ifndef MomentumTracker_hpp
#define MomentumTracker_hpp

#include "Plate.hpp"

namespace WorldBuilder {
    class AngularMomentumTracker {
        size_t plateCount;
        std::unordered_map<uint32_t, std::shared_ptr<Plate>> plates;
        wb_float* startingMomentumMagnitude;
        wb_float* momentumDeltaMagnitude; // 2d array from i to j
        size_t* startingSurfaceCellCount;
        size_t* colidedSurfaceCellCount;
    public:
        AngularMomentumTracker(std::unordered_map<uint32_t, std::shared_ptr<Plate>> plates);
        ~AngularMomentumTracker();
        
        // momentum modification
        void transferMomentumMagnitude(size_t source, size_t destination, wb_float magnitude);
        void removeMomentumFromPlate(size_t plateIndex, wb_float magnitude);
        
        void addCollision(size_t i, size_t j);
        
        void commitTransfer();
        
    };
}

#endif /* MomentumTracker_hpp */
