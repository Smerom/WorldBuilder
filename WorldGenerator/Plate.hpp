// --
//  Plate.hpp
//  WorldGenerator
//


#ifndef Plate_hpp
#define Plate_hpp

#include <unordered_map>

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
        
        Plate(uint32_t initialCellCount, uint32_t ourId);
        
        void updateCellRadii();
        
        size_t surfaceSize() const; // number of surface cells, not threadsafe
        void move(wb_float timestep);
        void homeostasis(const WorldAttributes, wb_float timestep);
        Vec3 localToWorld(Vec3 local){
            return math::affineRotaionMulVec(this->rotationMatrix, local);
        }

        
    }; // class Plate
} // namespace WorldBuilder

#endif /* Plate_hpp */
