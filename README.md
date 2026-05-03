# nifty
A collection of nifty LLVM utilities that I've amassed over the years.

![Martin in Flight](https://media.audubon.org/nas_birdapi_hero/h_purple-martin_006_shutterstock_texas_1220914552_adult-male_agamiphotoagency.jpg?height=236)

## Building
```sh
mkdir build
cd build
cmake ../ -G Ninja -DCMAKE_PREFIX_PATH=/path/to/llvm/build
ninja
```

## Tools

### `strip-tbaa`
Strip TBAA metadata from an LLVM program.
This is useful when passes assume that TBAA metadata doesn't exist (like Alive2).