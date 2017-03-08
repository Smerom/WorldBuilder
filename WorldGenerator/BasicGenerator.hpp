// --
//  BasicGenerator.hpp
//  WorldGenerator
//

#ifndef BasicGenerator_hpp
#define BasicGenerator_hpp

#include <memory>

#include "Generator.hpp"

namespace WorldBuilder {
    class World;
    /*************** Basic Generator ***************/
    /*  Our basic generator, 
     *  creates a domed circle of land around a random point
     */
    class BasicGenerator : public Generator {
    public:
        BasicGenerator(std::shared_ptr<Random> random) : Generator(random){};
        
        // required parent function
        void Generate(std::shared_ptr<World> theWorld);
        
    protected:
    }; // class Basic Generator
} // namespace WorldBuilder

#endif /* BasicGenerator_hpp */
