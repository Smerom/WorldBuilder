// --
//  MomentumTracker.hpp
//  WorldGenerator
//


#ifndef MomentumTracker_hpp
#define MomentumTracker_hpp

#include "Plate.hpp"

namespace WorldBuilder {
    class AngularMomentumTracker {
        std::unordered_map<uint32_t, std::shared_ptr<Plate>> plates;
        std::unordered_map<uint64_t, wb_float> momentumTransfers; // from plate with id of top 32 bits to plate of lower 32 bits
        std::unordered_map<uint64_t, size_t> collidedCellCount; // from plate with id of top 32 bits to plate of lower 32 bits
        std::unordered_map<uint32_t, wb_float> startingMomentum;
        std::unordered_map<uint32_t, size_t> startingCellCount;
    public:
        AngularMomentumTracker(std::unordered_map<uint32_t, std::shared_ptr<Plate>> plates);
        
        // momentum modification
        void transferMomentumOfCell(std::shared_ptr<Plate> source, std::shared_ptr<Plate> destination, std::shared_ptr<PlateCell> cell);
        void addCollision(std::shared_ptr<Plate> source, std::shared_ptr<Plate> destination);
        
        void commitTransfer();
        
    };
}

#endif /* MomentumTracker_hpp */
