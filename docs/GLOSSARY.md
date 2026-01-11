# Glossary

## Scissor (Scissor Test)

A graphics pipeline feature that restricts rendering to a rectangular region of the framebuffer. Fragments generated outside the scissor rectangle are discarded before being written to the render target. Commonly used for clipping, UI rendering, and performance optimization.


## Geometry Instancing

A rendering technique that allows multiple instances of the same geometry to be drawn with a single draw call, while varying per-instance data such as position, orientation, scale, or material parameters. It reduces CPU overhead and improves performance when rendering many similar objects.

**In Graphical APIs**

A graphics API feature that enables drawing multiple instances of identical geometry in one draw call, using instance-specific data provided to the GPU.

## Viewport

A rectangular region of the framebuffer that defines how normalized device coordinates (NDC) are mapped to screen-space coordinates. It controls the position, size, and depth range of rendered primitives on the screen.

## Z-buffering

## Back-face culling

## Multisample anti-aliasing