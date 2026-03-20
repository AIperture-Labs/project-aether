#pragma once

#include "config.hpp"
// Wavefront OBJ format reference:
// Paul Bourke — OBJ file format notes and examples
// https://paulbourke.net/dataformats/obj/
// This header/loader uses the conventions described on the above page.

/*
Notes about OBJ ASCII file :

```obj
# List of geometric vertices, with (x, y, z, [w]) coordinates, w is optional and defaults to 1.0.
v 0.123 0.234 0.345 1.0
v ...
...
# List of texture coordinates, in (u, [v, w]) coordinates, these will vary between 0 and 1. v, w are optional and
default to 0. vt 0.500 1 [0] vt ...
...
# List of vertex normals in (x,y,z) form; normals might not be unit vectors.
vn 0.707 0.000 0.707
vn ...
...
# Parameter space vertices in (u, [v, w]) form; free form geometry statement (see below)
vp 0.310000 3.210000 2.100000
vp ...
...
# Polygonal face element (see below)
f 1 2 3
f 3/1 4/2 5/3
f 6/4/1 3/5/3 7/6/5
f 7//1 8//2 9//3
f ...
...
# Line element (see below)
l 5 8 1 2 4 9
[object name]
...
g [group name]
...
```
*/

PROJECT_API void obj_test();