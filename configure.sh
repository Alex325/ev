DIR="./build/configure/$1"
cmake -S . -B "$DIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$1