[![CircleCI](https://circleci.com/gh/Smerom/WorldBuilder/tree/master.svg?style=svg)](https://circleci.com/gh/Smerom/WorldBuilder/tree/master)

# WorldBuilder

Artistic planet generator with a strong foundation in science.

## Examples

Can be found [here](http://quinnmueller.me/planetGenerator). Most of them were created from the current main branch.

## How Do I Build It?

Everything (except main.cpp) under the WorldGenerator folder has no requirements other than the standard library. If you want to use the current main() for grpc, it requires linking against grpc and protobuf. You can take a look at the dockerfile in .circleci/docker to get a sense of how to set up the required build environment.

## Why No Tests?

Current expectations and design is not well defined enough to be reasonable to test. Tests will be added as things solidify.