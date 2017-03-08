// --
//  ErosionFlowGraph.hpp
//  WorldGenerator
//


#ifndef ErosionFlowGraph_hpp
#define ErosionFlowGraph_hpp

#include <vector>

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
        float weight;
        float materialHeight;
    };
    
    class MaterialFlowNode {
    public:
        std::vector<std::shared_ptr<FlowEdge>> outflowTargets;
        std::vector<std::shared_ptr<FlowEdge>> inflowTargets;
        bool touched;
        float materialHeight;
        float offsetHeight;
        float suspendedMaterialHeight;
        
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
        
    };
}

#endif /* ErosionFlowGraph_hpp */
