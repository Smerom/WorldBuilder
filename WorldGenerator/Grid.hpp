// --
//  Grid.hpp
//  WorldGenerator
//


#ifndef Grid_hpp
#define Grid_hpp

#include "Basic.pb.h"

#include <iostream>

#include "math.hpp"

namespace WorldBuilder {
    class GridVertex;
    class Grid {
    /*************** Member Variables ***************/
    private:
        std::vector<GridVertex> verts;
        
    public:
        Grid(const api::Grid* wingedGrid);
        Grid(size_t vertexCount);
        
        
        void addGrpcGridPart(const api::Grid *theGrid);
        
    /*************** Getters ***************/
        size_t verts_size(){
            return this->verts.size();
        }
        const std::vector<GridVertex>& get_vertices(){
            return this->verts;
        }
    };
    
    
    
    
    class GridVertex {
        friend Grid;
    /*************** Member Variables  ***************/
    private:
        size_t index;
        Vec3 vector;
        std::vector<GridVertex *> neighbors;
        
    public:
        
    /*************** Getters ***************/
        const Vec3 get_vector() const {
            return this->vector;
        }
        size_t get_index() const{
            return this->index;
        }
        const std::vector<GridVertex *>& get_neighbors() const {
            return this->neighbors;
        }

    };
}

#endif /* Grid_hpp */
