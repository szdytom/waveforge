# Pixel physics benchmark

Configure and run a release build with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWAVEFORGE_BUILD_BENCHMARKS=ON
cmake --build build --target pixel_physics_benchmark
./build/pixel_physics_benchmark 25
```

The optional argument is the number of fresh-world samples. The benchmark
reports the median, excluding world construction and scenario setup.

## WebGPU production backend

The headless benchmark uses the same `GpuPhysicsBackend` as the game and the
pinned `wgpu-native` runtime. Configure and run it with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DWAVEFORGE_ENABLE_WEBGPU=ON -DWAVEFORGE_BUILD_BENCHMARKS=ON
cmake --build build --target webgpu_physics_benchmark
./build/webgpu_physics_benchmark
```

It warms up the backend, runs a 1024x576 burning oil/water tank for 96 ticks,
validates asynchronous RGBA and compact-query output, and fails when the
timestamped GPU pass average exceeds 8 ms. It also reports end-to-end queue
submission and readback completion separately. GPU device access may be
restricted inside containers and sandboxes.

On 2026-08-26, the full backend averaged about 6.1 ms of GPU work per tick on
the Intel Arc iGPU through Vulkan. This is a local development gate, not a
promise for every GPU.

## Baseline

Measured on 2026-08-26 with GCC 16.2.1 on an Intel Core Ultra 5 125H. Values
are milliseconds per call from the median of 25 samples.

| Dimensions | Thermal, cold air | Thermal, hot copper | Fluid, half water | Thermal, warm water | Fluid, warm water | Full tick, warm water |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 128x72 | 0.039 | 0.212 | 0.110 | 0.187 | 0.116 | 0.299 |
| 256x144 | 0.093 | 0.668 | 0.418 | 0.563 | 0.420 | 1.180 |
| 512x288 | 0.488 | 1.610 | 1.665 | 1.623 | 1.701 | 4.412 |
| 1024x576 | 2.126 | 8.191 | 7.539 | 7.000 | 7.519 | 19.052 |

Thermal analysis is not consistently the slowest pass. It dominates smaller
thermally active maps and the worst-case hot-copper scenario. At 512x288 and
1024x576, fluid analysis is slightly slower than thermal analysis on the same
warm-water world. Optimization priorities should therefore be based on the
expected map composition as well as its dimensions.

## Burning tank

The extreme scenario fills an open stone tank with equal-depth layers of water
and oil, then starts every oil pixel ignited at maximum heat. It runs for 96
consecutive ticks so the measurements include combustion, heat transfer, water
evaporation, steam movement, and smoke movement. These are average milliseconds
per tick from a representative run:

| Dimensions | Fluid | Thermal | Elements | Final steam pixels | Final smoke pixels |
| --- | ---: | ---: | ---: | ---: | ---: |
| 128x72 | 0.119 | 0.172 | 0.208 | 495 | 227 |
| 256x144 | 0.458 | 0.490 | 0.830 | 2,152 | 1,906 |
| 512x288 | 1.997 | 1.860 | 3.632 | 9,760 | 8,359 |
| 1024x576 | 8.659 | 6.346 | 15.542 | 24,879 | 31,033 |

Thermal analysis is not the main bottleneck in this case. At 1024x576, element
processing takes 50.9% of the measured physics time, fluid analysis takes
28.3%, and thermal analysis takes 20.8%. The same ranking was reproduced in a
second run despite the simulation's randomized combustion and movement.
