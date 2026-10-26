// Ogre-Next 75643c399: compileAndLink validates before Hlms::applyTextureRegisters.
// OpenGL initializes ALL sampler uniforms to unit zero. Different sampler types
// then fail validation even though HLMS supplies valid bindings before drawing.
// Give active samplers distinct initial units without disabling validation. Ogre
// still replaces these initial values with its real per-program texture registers.
if (mLinked && mVertexShader && mVertexShader->getName().find("VertexShader_vs") != String::npos)
{
    GLint previousProgram = 0, uniformCount = 0, nameLength = 0, maxUnits = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits);
    glGetProgramiv(mGLProgramHandle, GL_ACTIVE_UNIFORMS, &uniformCount);
    glGetProgramiv(mGLProgramHandle, GL_ACTIVE_UNIFORM_MAX_LENGTH, &nameLength);
    std::vector<GLchar> uniformName(static_cast<size_t>(std::max(1, nameLength)));
    GLint nextUnit = 0;
    const bool directUniform = glProgramUniform1iv != nullptr;
    if (!directUniform)
        glUseProgram(mGLProgramHandle);
    for (GLint index = 0; index < uniformCount; ++index)
    {
        GLsizei length = 0;
        GLint count = 0;
        GLenum type = 0;
        glGetActiveUniform(mGLProgramHandle, static_cast<GLuint>(index), nameLength, &length,
                           &count, &type, uniformName.data());
        bool sampler = false;
        switch (type)
        {
        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_2D_RECT:
        case GL_SAMPLER_2D_RECT_SHADOW:
        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_BUFFER:
        case GL_SAMPLER_1D_ARRAY_SHADOW:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_INT_SAMPLER_1D:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_INT_SAMPLER_2D_RECT:
        case GL_INT_SAMPLER_1D_ARRAY:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_INT_SAMPLER_BUFFER:
        case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_BUFFER:
        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_SAMPLER_CUBE_MAP_ARRAY:
        case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
        case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
            sampler = true;
            break;
        default:
            break;
        }
        if (!sampler)
            continue;
        // Leave a genuine hardware-limit error to the normal validator.
        if (count <= 0 || count > maxUnits - nextUnit)
            break;
        std::vector<GLint> units(static_cast<size_t>(count));
        for (GLint i = 0; i < count; ++i)
            units[static_cast<size_t>(i)] = nextUnit++;
        const GLint location = glGetUniformLocation(mGLProgramHandle, uniformName.data());
        if (location >= 0)
        {
            if (directUniform)
                glProgramUniform1iv(mGLProgramHandle, location, count, units.data());
            else
                glUniform1iv(location, count, units.data());
        }
    }
    if (!directUniform)
        glUseProgram(glIsProgram(static_cast<GLuint>(previousProgram))
                         ? static_cast<GLuint>(previousProgram)
                         : 0u);
}
