// --
//  ErosionFlowGraph.hpp
//  WorldGenerator
//
//  Data structures and logic for moving rock along a directed acyclic graph


#ifndef ErosionFlowGraph_hpp
#define ErosionFlowGraph_hpp

#include <vector>
#include "Defines.h"
#include <cmath>
#include <unordered_set>
#include <set>
#include <queue>
#include <iostream>

namespace WorldBuilder {
    class FlowEdge;
    class MaterialFlowNode;
    class MaterialFlowGraph;
    class MaterialFlowBasin;
    
    class FlowEdge {
        friend class MaterialFlowNode;
        friend class MaterialFlowBasin;
        friend class World;
    private:
        MaterialFlowNode* source;
        MaterialFlowNode* destination;
        wb_float weight;
        wb_float materialHeight;
    public:
        wb_float get_materialHeight() {
            return materialHeight;
        }
    };
    
    
    /*************** Material Flow Node ***************/
    /*
     *
     *
     */
    class MaterialFlowNode {
        wb_float materialHeight;
        wb_float suspendedMaterialHeight;
        MaterialFlowBasin* basin;
    public:
        
        MaterialFlowNode() : materialHeight(0), suspendedMaterialHeight(0), touched(false), offsetHeight(0), basin(nullptr){};
        
        std::vector<std::shared_ptr<FlowEdge>> outflowTargets;
        std::vector<std::shared_ptr<FlowEdge>> inflowTargets;
        std::unordered_set<MaterialFlowNode*> equalNodes;
        bool touched;
        
        wb_float offsetHeight;
        
        
        uint32_t plateIndex;
        uint32_t cellIndex;
        
        // TODO rename, does not suspend additional material, but sets the amount of suspended material out of the total
        void suspendMaterial(wb_float height) {
            if (!std::isfinite(height) || height < 0) {
                throw std::invalid_argument("Suspension height must be positive and finite");
            }
            // convert all to normal material
            set_materialHeight(materialHeight + suspendedMaterialHeight);
            // modify height if needed
            if (height > materialHeight) {
                height = materialHeight;
            }
            // suspend the material
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
        
        wb_float get_suspendedMaterialHeight() const{
            return suspendedMaterialHeight;
        }
        
        MaterialFlowBasin* get_basin() const {
            return this->basin;
        }
        void set_basin(MaterialFlowBasin* newBasin) {
            this->basin = newBasin;
        }
        
        wb_float elevation() const{
            return materialHeight + offsetHeight + suspendedMaterialHeight; // include suspended material in elevation as it is effectively part of the cell when not flowing the graph
        }

        void log() const {
            std::cout << "Logging Node: " << std::endl;
            std::cout << "Basin: " << this->basin << std::endl;
            std::cout << "Inflow count: " << this->inflowTargets.size() << std::endl;
            std::cout << "Outflow count: " << this->outflowTargets.size() << std::endl;
            std::cout << "Equal count: " << this->equalNodes.size() << std::endl;
            std::cout << "Elevation: " << this->elevation() << std::endl;
        }
        
        bool checkWeight() const;
        
        //void touchConnectedSubgraph(); // for finding roots if I end up doing that
        void upTreeFlow();
        
        MaterialFlowBasin* downhillBasin() const;
    };
    
    
    // want smallest on top of queue
    struct NodeElevComp {
        bool operator()(const MaterialFlowNode* a, const MaterialFlowNode* b) const{
            return a->elevation() < b->elevation();
        }
    };
    
    enum FillingResult {
        Filled,
        Overflow,
        Continue,
        Collision
    };
    struct BasinFillingResult {
        FillingResult result;
        MaterialFlowBasin* collisionBasin;
        MaterialFlowNode* overflowStart;
        wb_float overflowThickness;
    };
    
    class MaterialFlowBasin {
        friend struct BasinElevComp;
        
        std::unordered_set<MaterialFlowNode*> nodes;
        std::set<MaterialFlowNode*, NodeElevComp> upslopeCandidates;
        wb_float currentElevation;
        wb_float remainingThickness;
        
    public:
        MaterialFlowBasin(wb_float elev, wb_float thickness, MaterialFlowNode* initialNode) : currentElevation(elev), remainingThickness(thickness){
            nodes.insert(initialNode);
            initialNode->set_basin(this);
            
            // add upslope
            for(auto&& upslopeEdge : initialNode->inflowTargets) {
                upslopeCandidates.insert(upslopeEdge->source);
            }
        };
        
        BasinFillingResult fillNext();
        void merge(MaterialFlowBasin* absorbedBasin);
        
        void addThickness(wb_float addedThickness) {
            this->remainingThickness += addedThickness;
        }
        
        wb_float elevation() const{
            return this->currentElevation;
        }
        
        void addUpslope(MaterialFlowNode* upslopeNode) {
            if (this->nodes.find(upslopeNode) == this->nodes.end()) {
                this->upslopeCandidates.insert(upslopeNode);
            }
        }

        void addSingleNode(MaterialFlowNode* node) {
            std::cout << "Logging add single" << std::endl;
            node->log();
            node->set_basin(this);
            this->nodes.insert(node);
        }

        void addEqualNodes(std::unordered_set<MaterialFlowNode*> equalNodes, MaterialFlowNode* testNode);
        void addNode(std::unordered_set<MaterialFlowNode*> uphillNodes, std::unordered_set<MaterialFlowNode*> downhillNodes, MaterialFlowNode* node);
    };
    
    // want smallest on top of queue
    struct BasinElevComp {
        bool operator()(const MaterialFlowBasin * a, const MaterialFlowBasin* b) const {
            return a->currentElevation > b->currentElevation;
        }
    };
    
    
    
    class MaterialFlowGraph {
        friend class World;
    private:
        size_t nodeCount;
        std::vector<MaterialFlowNode> nodes;
        
        std::unordered_set<MaterialFlowBasin*> inactiveBasins;
        std::priority_queue<MaterialFlowBasin*, std::vector<MaterialFlowBasin*>, BasinElevComp> activeBasins;
        
    public:
        MaterialFlowGraph(size_t count) : nodeCount(count), nodes(count) {};
        ~MaterialFlowGraph() {
            for(auto&& basin : this->inactiveBasins) {
                delete basin;
            }
        }
        
        void flowAll();
        void fillBasins();
        
        wb_float totalMaterial() const;
        
        wb_float inTransitOutMaterial() const;
        wb_float inTransitInMaterial() const;
        
        bool checkWeights() const;
        
#warning "Add deallocation of basins"
    };
}

#endif /* ErosionFlowGraph_hpp */
