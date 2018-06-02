// --
//  Generator.hpp
//  WorldGenerator
//
//  Absract Class for initialization of landmass

#ifndef Generator_hpp
#define Generator_hpp

#include <memory>
#include "Random.hpp"

namespace WorldBuilder {
    class World;
/*************** Abstract Base Generator ***************/
/*  Responsible for initializing the landmass of the world
 *
 */
    class Generator {
    public:
        // constructor takes a shared Random for any
        // random elements that might be desired
        Generator(std::shared_ptr<Random> random) : randomSource(random){};
        
        // pure virtual function for generating initial worlds
        virtual void Generate(std::shared_ptr<World> theWorld) = 0;
        
    protected:
        std::shared_ptr<Random> randomSource;
    }; // class Generator
} // namespace WorldBuilder

#endif /* Generator_hpp */
