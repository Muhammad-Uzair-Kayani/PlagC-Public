#pragma once
#include <PlagC/Application.h>
#include <PlagC/World/SceneRenderer.h>
#include <PlagC/Input/Input.h>
#include <PlagC/World/TransformSystem.h>

// Owned snapshots only; workers never access World, Input or Renderer.
unsigned AnimateIntegration(PlagC::JobSystem&, std::vector<PlagC::TransformUpdate>&,
                            float dt, float time, unsigned iterations);
unsigned AnimateTransforms(PlagC::JobSystem&, std::span<PlagC::Transform>, float dt, float time, unsigned iterations);
void TestTransformSystem();
void TestSceneSynchronization(PlagC::Renderer&);
void TestInputAndPacing();
void TestIntegrationWork(PlagC::JobSystem&);
