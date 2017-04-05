// --
//  Random.hpp
//  WorldGenerator
//


#ifndef Random_hpp
#define Random_hpp

#include <random>

#include "math.hpp"

namespace WorldBuilder {
    class Random {
    public:
        Random(uint_fast32_t seed){
            std::minstd_rand source (seed);
            this->randomSource = source;
        }
        
        Vec3 getRandomPointUnitSphere();
        Vec3 getRandomPointAlongGreatCircle(Vec3 pole);
        wb_float randomNormal(wb_float mean, wb_float stdev);
        uint_fast32_t randomUniform(uint_fast32_t min, uint_fast32_t max);
        size_t uniformIndex(size_t min, size_t max);
    private:
        std::minstd_rand randomSource;
    };
}

#endif /* Random_hpp */
