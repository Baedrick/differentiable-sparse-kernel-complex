# Differentiable Sparse Kernel Complex

Dense convolution with large, arbitrary kernels is really expensive, which makes
it a poor fit for real-time applications like games, mobile UI, etc.
Differentiable Sparse Kernel Complex (Wu et al., 2026) replaces the dense kernel
with a cascade of sparse kernels. The offsets and weights are learned end-to-end
from the kernel's impulse response, and an ordered basis of filters is
interpolated at runtime, so the cost depends on the number of sparse taps instead
of the dense kernel size.

My contribution is a complete implementation of the method. The PyTorch trainer
fits the sparse basis to Gaussian or custom stamp targets, and the D3D11 runtime
applies the learned LUT with filter-space interpolation. I also added a direct
bokeh mask path so the sparse result can be compared against a straightforward
aperture convolution.

# AI Usage Declaration

I used Large Language Models (LLMs) to generate throwaway code to test whether
technical ideas are viable within academic timelines. Such code may appear on
development branches. All code on the `main` branch, except third-party code,
is written by me from scratch.

# Building

To build the binaries from source, you'll need [Microsoft C/C++ Build Tools v17
(2022) or later](https://aka.ms/vs/17/release/vs_buildtools.exe).

### Build Steps

TODO
