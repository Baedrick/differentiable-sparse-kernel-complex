# Linear Dual Kawase Blur

Dual Kawase (or Dual Filtering) Blur (Marius Bjørge, 2015) is really simple to
implement, runs really fast, is memory bandwidth friendly, and looks really good.
It fails for gradually blurring, which is a common usecase for any real-time
applications like games, mobile UI, etc. makes separable gaussian or box blur
the preferred technique.

My contribution is a set of closed-form equations that map a linear value to the
inputs of Dual Kawase Blur to achieve a gradual blur. I derived them using a
PyTorch curve-fitting pipeline that maximizes SSIM against a 3σ Gaussian
reference (99.7% of the Gaussian shape) on synthetic images across various blur
radii, then fit the final curves via regression.

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
