// --
//  Grid.hpp
//  WorldGenerator
//


#ifndef Grid_hpp
#define Grid_hpp

#include "Basic.pb.h"
#include "math.hpp"

namespace WorldBuilder {
    class GridVertex;
    class Grid {
    /*************** Member Variables ***************/
    private:
        std::vector<GridVertex> verts;
        
    public:
        Grid(const api::Grid* wingedGrid);
        Grid(uint32_t vertexCount);
        
        
        void addGrpcGridPart(const api::Grid *theGrid);
        
        void buildCenters();
        
    /*************** Getters ***************/
        uint32_t verts_size(){
            return verts.size();
        }
        const std::vector<GridVertex>& get_vertices(){
            return verts;
        }
    };
    
    
    
    
    class GridVertex {
        friend Grid;
    /*************** Member Variables  ***************/
    private:
        uint32_t index;
        Vec3 vector;
        Vec3 neighborCenter;
        std::vector<GridVertex *> neighbors;
        
    public:
        
    /*************** Getters ***************/
        const Vec3 get_vector() const {
            return this->vector;
        }
        uint32_t get_index() const{
            return index;
        }
        const std::vector<GridVertex *>& get_neighbors() const {
            return neighbors;
        }
        const Vec3 get_neighborCenter() const {
            return neighborCenter;
        }
        const Vec3 displacementFromCenter() const {
            return vector - neighborCenter;
        }

    };
}

#endif /* Grid_hpp */
