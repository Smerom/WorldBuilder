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

class VolcanicHotspot {
public:
    wb_float weight;
    Vec3 worldLocation;
    std::unordered_map<uint32_t, uint32_t> closestPlateCellIndex;
    
};
    
}
#endif /* VolcanicHotspot_hpp */
