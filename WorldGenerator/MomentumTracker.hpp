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
        std::vector<std::shared_ptr<Plate>> plates;
        float* startingMomentumMagnitude;
        float* momentumDeltaMagnitude; // 2d array from i to j
        size_t* startingSurfaceCellCount;
        size_t* colidedSurfaceCellCount;
    public:
        AngularMomentumTracker(std::vector<std::shared_ptr<Plate>> plates);
        ~AngularMomentumTracker();
        
        // momentum modification
        void transferMomentumMagnitude(size_t source, size_t destination, float magnitude);
        void removeMomentumFromPlate(size_t plateIndex, float magnitude);
        
        void addCollision(size_t i, size_t j);
        
        void commitTransfer();
        
    };
}

#endif /* MomentumTracker_hpp */
