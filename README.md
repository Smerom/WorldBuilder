# WorldBuilder
Master status: [![CircleCI](https://circleci.com/gh/Smerom/WorldBuilder/tree/master.svg?style=svg)](https://circleci.com/gh/Smerom/WorldBuilder/tree/master)
Artistic planet generator with a strong foundation in science.

## Examples
Can be found [here](http://quinnmueller.me/planetGenerator). Most of them were created from the current main branch.

## How Do I Build It?
Good question. I'm working on a portable build. Until then, everything (except main.cpp) under the WorldGenerator folder has no requirements other than the standard library. If you want to use the current main() for grpc, it requires linking against libgrpc++ and libprotobuf.10.
