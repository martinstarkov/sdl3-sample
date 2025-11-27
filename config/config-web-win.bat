@echo OFF

cd ..
rm rf build-web
mkdir build-web
cd ../build-web
emcmake cmake -S .. -B .
cmake --build .