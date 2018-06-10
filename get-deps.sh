cd ~/
sudo apt-get update
sudo apt-get install autoconf automake libtool curl make g++ unzip
sudo apt-get install build-essential pkg-config
curl -OL https://github.com/google/protobuf/releases/download/v3.5.1/protobuf-cpp-3.5.1.tar.gz
tar -xzf protobuf-cpp-3.5.1.tar.gz
cd protobuf-3.5.1
./configure
make
make check
sudo make install
sudo ldconfig # refresh shared library cache.

git clone -b $(curl -L https://grpc.io/release) https://github.com/grpc/grpc
cd grpc
git submodule update --init
make
sudo make install