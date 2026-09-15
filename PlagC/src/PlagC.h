#pragma once
#include "pch.h"
//--------------Engine Side includes that should be available to the user-----------------
#include "PlagC/Core/Application.h" //To provide the user with the Application class to create their own application
#include "PlagC/Core/log.h" //To provide the user with the logging functionality
#include "PlagC/Core/Layer.h"
#include "PlagC/ImGui/ImGuiLayer.h"
#include "PlagC/Core/Input.h"
#include "PlagC/Core/KeyCodes.h"
#include "PlagC/Core/MouseButtonCodes.h"
#include "PlagC/Core/Timestep.h"

//----------------Renderer ----------------------
#include "PlagC/Renderer/Shader.h"
#include "PlagC/Renderer/Buffer.h"
#include "PlagC/Renderer/VertexArray.h"
#include "PlagC/Renderer/OrthographicCameraController.h"
#include "PlagC/Renderer/Renderer.h"
#include "PlagC/Renderer/Renderer2D.h"
#include "PlagC/Renderer/Texture.h"
#include "Platfrom/OpenGL/OpenGLShader.h"

//-------------ASSESTS MANAGER-------------------
