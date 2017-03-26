// --
//  ErosionFlowGraph.cpp
//  WorldGenerator
//


#include "ErosionFlowGraph.hpp"

namespace WorldBuilder {
    void MaterialFlowNode::upTreeFlow(){
        if (this->touched == false) {
            // touch self
            this->touched = true;
            
            // flow up
            wb_float suspendedMaterial = 0;
            for (std::vector<std::shared_ptr<FlowEdge>>::iterator flowEdge = this->inflowTargets.begin();
                 flowEdge != this->inflowTargets.end();
                 flowEdge++)
            {
                (*flowEdge)->source->upTreeFlow();
                // collect material
                suspendedMaterial += (*flowEdge)->materialHeight;
            }
            
            // TODO, move to specialized flow node
            // TODO, dynamic sealevel
            if (this->offsetHeight + this->materialHeight < 9620 - 300) {
                //
                wb_float fillAmount = 9620 - 300 - (this->offsetHeight + this->materialHeight);
                if (fillAmount > suspendedMaterial) {
                    fillAmount = suspendedMaterial;
                }
                suspendedMaterial -= 0.95*fillAmount;
                this->materialHeight += 0.95*fillAmount;
            }
            
            // add material from self
            suspendedMaterial += this->suspendedMaterialHeight;
            
            // move material
            wb_float totalMaterialMoved = 0;
            for (std::vector<std::shared_ptr<FlowEdge>>::iterator flowEdge = this->outflowTargets.begin();
                 flowEdge != this->outflowTargets.end();
                 flowEdge++)
            {
                (*flowEdge)->materialHeight = (*flowEdge)->weight * suspendedMaterial;
                totalMaterialMoved += (*flowEdge)->weight * suspendedMaterial;
            }
            
            // remove material, suspended from this cell plus any discrepancy between suspended and total moved
            this->materialHeight = this->materialHeight - this->suspendedMaterialHeight + (suspendedMaterial - totalMaterialMoved);
        } // end if touched
    }// MaterialFlowNode::upTreeFlow()
    
    void MaterialFlowGraph::flowAll(){
        // clear touched status
        for (size_t nodeIndex = 0; nodeIndex < this->nodeCount; nodeIndex++) {
            this->nodes[nodeIndex].touched = false;
        }
        
        // flow entire graph, could be done from roots???
        for (size_t nodeIndex = 0; nodeIndex < this->nodeCount; nodeIndex++) {
            if (this->nodes[nodeIndex].touched == false) {
                this->nodes[nodeIndex].upTreeFlow();
            }
        }
    }// MaterialFlowGraph::flowAll()
}