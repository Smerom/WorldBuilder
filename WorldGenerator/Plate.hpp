// --
//  Plate.hpp
//  WorldGenerator
//
//  A self contained plate
//  Should hold logic for internal plate interaction (only)


#ifndef Plate_hpp
#define Plate_hpp

#include <unordered_map>
#include <unordered_set>

#include "Defines.h"
#include "PlateCell.hpp"

namespace WorldBuilder {
    /***************  Plate ***************/
    /*  Represents a tectonic plate
     *  Tracks movement via a rotation matrix
     *  Contains relevent PlateCells
     *  Tracks angular momentum
     *
     */
    class Plate {
        friend class AngularMomentumTracker;
        friend class World;
    private:
        Vec3 pole;
        wb_float angularSpeed;
        Matrix3x3 rotationMatrix;
        wb_float densityOffset;
        
        Vec3 center;
        const GridVertex* centerVertex;
        wb_float maxEdgeAngle;
        
        uint32_t id; // id in the world plate map
    public:
        // TODO, make not public!
        std::unordered_map<uint32_t, std::shared_ptr<PlateCell>> cells;
        std::unordered_map<uint32_t, std::shared_ptr<PlateCell>> edgeCells;
        std::unordered_set<uint32_t> riftingTargets;
        
        Plate(uint32_t initialCellCount, uint32_t ourId);
        
        void updateCellRadii(); // radius from plate pole, used for momentum tracking
        
        size_t surfaceSize() const; // number of surface cells, not threadsafe
        void move(wb_float timestep); // updates rotation matrix
        void homeostasis(const WorldAttributes, wb_float timestep);
        
        // transforms as point in local coordinates to world coordinates
        Vec3 localToWorld(Vec3 local){
            return math::affineRotaionMulVec(this->rotationMatrix, local);
        }

        
    }; // class Plate
} // namespace WorldBuilder

#endif /* Plate_hpp */
