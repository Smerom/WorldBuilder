// --
//  ErosionFlowGraph.hpp
//  WorldGenerator
//


#ifndef ErosionFlowGraph_hpp
#define ErosionFlowGraph_hpp

#include <vector>
#include "Defines.h"
#include <cmath>

namespace WorldBuilder {
    class FlowEdge;
    class MaterialFlowNode;
    class MaterialFlowGraph;
    
    class FlowEdge {
        friend class MaterialFlowNode;
        friend class World;
    private:
        MaterialFlowNode* source;
        MaterialFlowNode* destination;
        wb_float weight;
        wb_float materialHeight;
    };
    
    class MaterialFlowNode {
        wb_float materialHeight;
        wb_float suspendedMaterialHeight;
    public:
        std::vector<std::shared_ptr<FlowEdge>> outflowTargets;
        std::vector<std::shared_ptr<FlowEdge>> inflowTargets;
        bool touched;
        
        wb_float offsetHeight;
        
        
        uint32_t plateIndex;
        uint32_t cellIndex;
        
        void suspendMaterial(wb_float height) {
            if (!std::isfinite(height) || height < 0) {
                throw std::invalid_argument("Suspension height must be positive and finite");
            }
            if (height > materialHeight) {
                height = materialHeight;
            }
            suspendedMaterialHeight = height;
            set_materialHeight(materialHeight - height);
        }
        
        wb_float get_materialHeight() const {
            return materialHeight;
        }
        void set_materialHeight(wb_float height) {
            if (!std::isfinite(height) || height < 0) {
                throw std::invalid_argument("Height must be positive and finite");
            }
            materialHeight = height;
        }
        
        bool checkWeight() const;
        
        //void touchConnectedSubgraph(); // for finding roots if I end up doing that
        void upTreeFlow();
    };
    
    class MaterialFlowGraph {
        friend class World;
    private:
        size_t nodeCount;
        MaterialFlowNode* nodes;
        
    public:
        MaterialFlowGraph(size_t count) : nodeCount(count) {
            this->nodes = new MaterialFlowNode[count];
        }
        
        ~MaterialFlowGraph(){
            delete [] this->nodes;
        }
        
        void flowAll();
        
        wb_float totalMaterial() const;
        
        bool checkWeights() const;
        
    };
}

#endif /* ErosionFlowGraph_hpp */
