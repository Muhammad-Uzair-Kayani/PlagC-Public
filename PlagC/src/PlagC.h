#pragma once
#include "pch.h"
//--------------Engine Side includes that should be available to the user-----------------
#include "PlagC/Application.h" //To provide the user with the Application class to create their own application
#include "PlagC/StartingPoint.h" //To provide the user with the main entry point of the application
#include "PlagC/log.h" //To provide the user with the logging functionality
#include "PlagC/Layer.h"
#include "PlagC/ImGui/ImGuiLayer.h"
#include "PlagC/Input.h"
#include "PlagC/KeyCodes.h"
#include "PlagC/MouseButtonCodes.h"
#include "PlagC/Core/Timestep.h"

//----------------Renderer ----------------------
#include "Platfrom/OpenGL/Shader.h"
#include "PlagC/Renderer/Buffer.h"
#include "PlagC/Renderer/VertexArray.h"
#include "PlagC/Renderer/OrthographicCamera.h"
#include "PlagC/Renderer/Renderer.h"