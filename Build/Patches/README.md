# Pinned Ogre GLSL initialization workaround

`Ogre.lua` generates `Bin-Int/Generated/OgreGLSLMonolithicProgram.cpp` from the
unchanged Ogre pin `75643c3997f5b6d2aa1d7bd8400b9be6736d9908`. It inserts
`PlagCGLSLSamplerInitialization.inl` at the single existing pre-binding validation
site. Generation fails if that site disappears or becomes ambiguous. Only this
translation unit replaces its upstream counterpart in the GL3Plus project.
The generated source retains Ogre's MIT copyright/license header.

Ogre links a program and calls glValidateProgram before HLMS texture-register
parameters are installed. Mixed sampler types initially all use unit zero, so
that early validation reports an error even when draw-time bindings are valid.
For HLMS-generated programs only, the include assigns distinct initial units to
active sampler elements, then retains the original validation. Real HLMS register
values are still applied normally. It prefers direct program-uniform updates;
the OpenGL 3.3 fallback restores the previous valid program binding. Legacy custom
shader defaults and vendor source files are not changed.

`RuntimeAssetSlice.cpp` checks the actual PBR draw state and deliberately aliases
two different sampler types as a negative control. The invalid state must fail;
restoring the original bindings must pass. This workaround does not filter logs
or disable shader/driver validation. The legacy GL3.3 fallback and Windows driver
path have not been exercised in the Linux Mesa 4.5 validation environment.
