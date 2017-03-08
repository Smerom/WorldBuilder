// --
//  BombardmentGenerator.hpp
//  WorldGenerator
//

#ifndef BombardmentGenerator_hpp
#define BombardmentGenerator_hpp

#include <memory>

#include "Generator.hpp"

namespace WorldBuilder {
    class World;
    /*************** Bombardment Generator ***************/
    /*  Our bombardment generator
     *  throws a bunch of rocks at the ground
     */
    class BombardmentGenerator : public Generator {
    public:
        BombardmentGenerator(std::shared_ptr<Random> random) : Generator(random){};
        
        // required parent function
        void Generate(std::shared_ptr<World> theWorld);
        
    protected:
    }; // class Bombardment Generator
} // namespace WorldBuilder

#endif /* BombardmentGenerator_hpp */
