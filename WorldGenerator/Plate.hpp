// --
//  Plate.hpp
//  WorldGenerator
//


#ifndef Plate_hpp
#define Plate_hpp

#include <vector>

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
        float angularSpeed;
        Matrix3x3 rotationMatrix;
        float densityOffset;
    public:
        // TODO, make not public!
        std::vector<PlateCell> cells;
        
        // does not set last nearest on plate cells, should it?
        Plate(Grid *worldGrid);
        
        void updateCellRadii();
        
        size_t surfaceSize() const; // number of surface cells, not threadsafe
        void move(float timestep);
        void homeostasis(const WorldAttributes, float timestep);
        Vec3 localToWorld(Vec3 local){
            return math::affineRotaionMulVec(this->rotationMatrix, local);
        }

        
    }; // class Plate
} // namespace WorldBuilder

#endif /* Plate_hpp */
