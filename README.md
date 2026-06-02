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

### `nifty extract`
Extract a set of basic blocks from a function into their own function.

`--func` specifies a function to extract from (no @).
`--regions` extracts all SESE regions from the given function.


### `nifty diff`
Find the differences between two IR files based on the program structure tree.
Uses the [GumTree](https://hal.science/hal-04855170v1/file/GumTree_simple__fine_grained__accurate_and_scalable_source_differencing.pdf) algorithm.

`--func` specifies a function to process (no @).
`--dump-gumtree` dump the GumTree representation of the function diff.


### `nifty strip-tbaa`
Strip TBAA metadata from an LLVM program.
This is useful when passes assume that TBAA metadata doesn't exist (like Alive2).