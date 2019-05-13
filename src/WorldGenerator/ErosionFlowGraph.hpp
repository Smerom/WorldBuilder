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
#include "RockColumn.hpp"

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
        wb_float waterVolume;
        wb_float weight;
        wb_float materialHeight;
    public:
        FlowEdge() : source(nullptr), destination(nullptr), waterVolume(0), weight(0), materialHeight(0){};
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
        std::shared_ptr<PlateCell> source;
        MaterialFlowBasin* basin;
    public:
        wb_float downhillSlope;
        
        MaterialFlowNode() : source(nullptr), basin(nullptr), downhillSlope(0), touched(false), offsetHeight(0){};
        
        std::vector<std::shared_ptr<FlowEdge>> outflowTargets;
        std::vector<std::shared_ptr<FlowEdge>> inflowTargets;
        std::unordered_set<MaterialFlowNode*> equalNodes;
        bool touched;
        
        wb_float offsetHeight;
        
        
        uint32_t plateIndex;
        uint32_t cellIndex;

        void set_source(std::shared_ptr<PlateCell> s) {
            this->source = s;
        }
        
        MaterialFlowBasin* get_basin() const {
            return this->basin;
        }
        void set_basin(MaterialFlowBasin* newBasin) {
            this->basin = newBasin;
        }

        wb_float sedimentHeight() const {
            return this->source->rock.sediment.get_thickness();
        }

        void set_sedimentHeight(wb_float height) {
            this->source->rock.sedment.set_thickness(height);
        }
        
        wb_float elevation() const{
            return this->source->get_elevation(); // include suspended material in elevation as it is effectively part of the cell when not flowing the graph
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
        void upTreeFlow(wb_float sealevel, wb_float timestep);
        
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

        // adds a single node, and removes from upslope if present.
        // ignores equal nodes
        void addSingleNode(MaterialFlowNode* node) {
            // std::cout << "Logging add single" << std::endl;
            // node->log();
            this->upslopeCandidates.erase(node);
            node->set_basin(this);
            this->nodes.insert(node);
        }

        void addEqualNodes(std::unordered_set<MaterialFlowNode*> &equalNodes, MaterialFlowNode* testNode);
        void addNode(std::unordered_set<MaterialFlowNode*> &uphillNodes, std::unordered_set<MaterialFlowNode*> &downhillNodes, MaterialFlowNode* node);
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
        
        void flowAll(wb_float sealevel, wb_float timestep);
        void fillBasins();
        
        bool checkWeights() const;
        
#warning "Add deallocation of basins"
    };
}

#endif /* ErosionFlowGraph_hpp */
