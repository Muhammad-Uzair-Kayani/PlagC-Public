#include "pch.h"
#include "OpenGLBuffer.h"
#include <glad/glad.h>

PlagC::OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size)
{
	PC_PROFILE_FUNCTION();
	glGenBuffers(1, &m_RendererID);
	glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void PlagC::OpenGLVertexBuffer::Bind() const
{
	PC_PROFILE_FUNCTION();
	glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
}

void PlagC::OpenGLVertexBuffer::UnBind() const
{
	PC_PROFILE_FUNCTION();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

PlagC::OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count) 
							: m_Count(count)
{
	PC_PROFILE_FUNCTION();
	glGenBuffers(1, &m_RendererID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}

void PlagC::OpenGLIndexBuffer::Bind() const
{
	PC_PROFILE_FUNCTION();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
}

void PlagC::OpenGLIndexBuffer::UnBind() const
{
	PC_PROFILE_FUNCTION();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
