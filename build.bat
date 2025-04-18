echo "building Orca Slicer..."

mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBBL_RELEASE_TO_PUBLIC=1 
cmake --build .            