// --
//  main.cpp
//  WorldGenerator
//


#include <iostream>

#include <random>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "Basic.pb.h"
#include "Basic.grpc.pb.h"
#include "SimulationRunner.hpp"
#include "BasicGenerator.hpp"
#include "BombardmentGenerator.hpp"
#include "World.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

class WorldBuilderImpl final : public api::WorldBuilder::Service {
public:
    explicit WorldBuilderImpl(std::string theTag){
        this->tag = theTag;
    }
    
    Status GenerateWorld(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::api::SimulationInfo, ::api::Grid>* stream) override {
        
        std::cout << "Attempting to build world" << std::endl;
        
        // read the stream until end
        uint32_t gridVertexCount;
        uint32_t currentGridCount = 0;
        api::Grid readGrid;
        
        WorldBuilder::Grid *grid;
        stream->Read(&readGrid);
        if (readGrid.totalverts() == 0) {
            return Status::CANCELLED;
        } else {
            gridVertexCount = readGrid.totalverts();
            grid = new WorldBuilder::Grid(readGrid.totalverts());
            grid->addGrpcGridPart(&readGrid);
            currentGridCount += readGrid.vertices_size();
        }
        // read remaining grid
        while (currentGridCount < gridVertexCount) {
            stream->Read(&readGrid);
            grid->addGrpcGridPart(&readGrid);
            currentGridCount += readGrid.vertices_size();
        }
        
        std::random_device rd;
        uint seed = rd(); // get a random seed
        std::printf("Random Seed: %u\n", seed);
        // create the generator
        std::shared_ptr<WorldBuilder::Random> randomSource(new WorldBuilder::Random(seed));
        WorldBuilder::SimulationRunner runner(new WorldBuilder::BombardmentGenerator(randomSource), new WorldBuilder::World(grid, randomSource));
        runner.set_runTimesteps(1);
        runner.set_runMinTimestep(0.05);
        runner.shouldLogRunTiming = false;
        
        bool open = true;
        while (open) {
            
            try {
                runner.Run();
            } catch (...) {
                std::printf("Broken simulation on seed: %u\n", seed);
                break;
            }
            
            api::SimulationInfo info;
//            for (std::vector<WorldBuilder::WorldCell>::const_iterator cell = runner.get_world()->get_cells().begin();
//                 cell != runner.get_world()->get_cells().end();
//                 cell++)
//            {
//                int_fast32_t activeIndex = runner.get_world()->activePlateIndexForCell(&*cell);
//                float elevation = cell->get_elevation(activeIndex);
//                //std::cout << elevation << std::endl;
//                if (elevation > 18000) {
//                    //std::cout << elevation << std::endl;
//                }
//                float sedimentHeight = 0;
//                if (activeIndex >=0){
//                    sedimentHeight = cell->get_surfaceCells()[activeIndex][0]->rock.sediment.thickness;
//                } else {
//                    sedimentHeight = cell->get_strandedSegment().thickness;
//                }
//                
//                uint_fast32_t plateIndex = runner.get_world()->activePlateIndexForCell(&*cell);
//                
//                info.add_elevations(elevation);
//                info.add_sediment(sedimentHeight);
//                info.add_plates(plateIndex);
//            }
            
            info.set_age(runner.get_world()->get_age());
            open = stream->Write(info);
            
            //open = false; // only one to capture starting state
        }
        
        return Status::OK;
    }
    
private:
    std::string tag;
    
};



int main(int argc, const char * argv[]) {
    std::string server_address("0.0.0.0:18082");
    WorldBuilderImpl service("chicken");
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
    
    return 0;
}
