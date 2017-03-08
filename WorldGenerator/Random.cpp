// --
//  Random.cpp
//  WorldGenerator
//

#include "Random.hpp"

#include <cmath>

namespace WorldBuilder {
    Vec3 Random::getRandomPointUnitSphere(){
        Vec3 northPole;
        northPole.coords[0] = 0;
        northPole.coords[1] = 0;
        northPole.coords[2] = 1;
        
        // rotate twice
        Vec3 temp1 = getRandomPointAlongGreatCircle(northPole);
        return getRandomPointAlongGreatCircle(temp1);
    }
    
    Vec3 Random::getRandomPointAlongGreatCircle(Vec3 pole){
        Vec3 vector;
        
        // for smaller
        float a, b, c;
        float x, y, z;
        a = pole.coords[0];
        b = pole.coords[1];
        c = pole.coords[2];
        
        // get point along great circle (perp to pole)
        std::normal_distribution<float> normalDist(0.0f, 1.0f);
        if (pole.coords[2] != 0) {
            x = normalDist(this->randomSource);
            y = normalDist(this->randomSource);
            z = -(a*x + b*y) / c;
        } else if (pole.coords[2] != 0) {
            x = normalDist(this->randomSource);
            z = normalDist(this->randomSource);
            y = -(a*x + c*z) / b;
        } else if (pole.coords[2] != 0) {
            y = normalDist(this->randomSource);
            z = normalDist(this->randomSource);
            x = -(c*z + b*y) / a;
        } else {
            // pole is invalid, use north pole
            x = 0;
            y = 0;
            z = 1;
        }
        vector.coords[0] = x;
        vector.coords[1] = y;
        vector.coords[2] = z;
        // rotate about pole by a random angle
        std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
        Matrix3x3 rotation = math::rotationMatrixAboutAxis(pole, uniformDist(this->randomSource) * 2 * M_PI);
        
        return math::normalize3Vector(math::affineRotaionMulVec(rotation, vector));
    }
    
    float Random::randomNormal(float mean, float stdev){
        std::normal_distribution<float> normalDist(mean, stdev);
        return normalDist(this->randomSource);
    }
    uint_fast32_t Random::randomUniform(uint_fast32_t min, uint_fast32_t max){
        std::uniform_int_distribution<uint_fast32_t> uniformDistribution(min, max);
        return uniformDistribution(this->randomSource);
    }
    size_t Random::uniformIndex(size_t min, size_t max){
        std::uniform_int_distribution<size_t> uniformDistribution(min, max);
        return uniformDistribution(this->randomSource);
    }
}