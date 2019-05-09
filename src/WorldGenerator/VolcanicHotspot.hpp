//
//  VolcanicHotspot.hpp
//  WorldGenerator
//
//

#ifndef VolcanicHotspot_hpp
#define VolcanicHotspot_hpp

#include <unordered_map>

#include "math.hpp"

namespace WorldBuilder {
    class PlateCell;
    class Plate;

class VolcanicHotspot {
public:
    wb_float weight;
    wb_float normWeight;
    Vec3 worldLocation;
    std::unordered_map<uint32_t, uint32_t> closestPlateCellIndex;

    std::weak_ptr<PlateCell> lastCell;
    std::weak_ptr<Plate> lastPlate;
    
};
    
}
#endif /* VolcanicHotspot_hpp */
