// --
//  Grid.cpp
//  WorldGenerator
//


#include "Grid.hpp"

namespace WorldBuilder {
    Grid::Grid(const api::Grid *theGrid) : verts(theGrid->vertices_size()){
        // copy our verts over
        uint_fast32_t count = theGrid->vertices_size();
        for (uint_fast32_t index = 0; index < count; index++) {
            api::GridVertex apiVertex = theGrid->vertices(index);
            verts[index].index = index;
            verts[index].vector.coords[0] = apiVertex.xcoord();
            verts[index].vector.coords[1] = apiVertex.ycoord();
            verts[index].vector.coords[2] = apiVertex.zcoord();
            // set neighbors
            uint_fast32_t neighborCount = apiVertex.neighbors_size();
            std::vector<GridVertex *> temp(neighborCount);
            verts[index].neighbors = temp;
            for (uint_fast32_t neighborIndex = 0; neighborIndex < neighborCount; neighborIndex++) {
                verts[index].neighbors[neighborIndex] = &verts[apiVertex.neighbors(neighborIndex)];
            }
        }
    }
    
    Grid::Grid(size_t vertexCount) : verts(vertexCount) {
        
    }
    
    void Grid::addGrpcGridPart(const api::Grid *theGrid){
        // copy our verts over
        uint_fast32_t count = theGrid->vertices_size();
        for (uint_fast32_t index = 0; index < count; index++) {
            api::GridVertex apiVertex = theGrid->vertices(index);
            verts[apiVertex.index()].index = apiVertex.index();
            verts[apiVertex.index()].vector.coords[0] = apiVertex.xcoord();
            verts[apiVertex.index()].vector.coords[1] = apiVertex.ycoord();
            verts[apiVertex.index()].vector.coords[2] = apiVertex.zcoord();
            // set neighbors
            uint_fast32_t neighborCount = apiVertex.neighbors_size();
            std::vector<GridVertex *> temp(neighborCount);
            verts[apiVertex.index()].neighbors = temp;
            for (uint_fast32_t neighborIndex = 0; neighborIndex < neighborCount; neighborIndex++) {
                verts[apiVertex.index()].neighbors[neighborIndex] = &verts[apiVertex.neighbors(neighborIndex)];
            }
        }
    }
}