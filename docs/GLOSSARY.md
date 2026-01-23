# Glossary

## Scissor (Scissor Test)

A graphics pipeline feature that restricts rendering to a rectangular region of the framebuffer. Fragments generated outside the scissor rectangle are discarded before being written to the render target. Commonly used for clipping, UI rendering, and performance optimization.


## Geometry Instancing

A rendering technique that allows multiple instances of the same geometry to be drawn with a single draw call, while varying per-instance data such as position, orientation, scale, or material parameters. It reduces CPU overhead and improves performance when rendering many similar objects.

**In Graphical APIs**

A graphics API feature that enables drawing multiple instances of identical geometry in one draw call, using instance-specific data provided to the GPU.

## BAR (Base Address Register)

Base Address Register (BAR) is a PCIe mechanism that exposes device memory or I/O regions to the host by assigning base addresses. In the GPU context, BARs let the CPU and GPU map and access regions of each other's address space, enabling direct memory access patterns (for example Resizable BAR / Smart Access Memory) that can reduce or eliminate explicit staging copies.

## SAM (Smart Access Memory)

A technology that extends BAR by providing the GPU with direct access to a larger portion of system memory. This eliminates the need for staging buffers in many cases, as data can be copied directly from host memory to GPU VRAM.

## UMA (Unified Memory Architecture)

Unified Memory Architecture is a design where the CPU and GPU share the same pool of memory. This simplifies data transfer and can improve performance by allowing both processors to access the same memory space without explicit copies.

## Viewport

A rectangular region of the framebuffer that defines how normalized device coordinates (NDC) are mapped to screen-space coordinates. It controls the position, size, and depth range of rendered primitives on the screen.

## Layout (Vulkan)

In Vulkan, the layout of an image describes how its data is organized and accessed in GPU memory at a given moment. Each operation (copy, rendering, shader read, etc.) requires the image to be in a specific layout, for example:

- `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`: optimal for receiving data during a copy operation.
- `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`: optimal for reading by a shader.

The layout must be explicitly managed by the application, using transitions (barriers), to ensure access validity and memory coherence. It allows the GPU driver to optimize memory access according to the current usage of the image.

## Z-buffering

## Back-face culling

## Multisample anti-aliasing