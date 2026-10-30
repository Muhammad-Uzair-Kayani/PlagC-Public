#include "pgcpch.h"
#include <PlagC/Renderer/Renderer.h>
#include <PlagC/Thread.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005 4244 4324 4996)
#endif
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreIdString.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cmath>
#include <stdexcept>

namespace PlagC
{
void Renderer::SetCameraProjection(CameraProjection projection, float orthographicHeight)
{
    Thread::RequireMain();
    if (!std::isfinite(orthographicHeight) || orthographicHeight <= 0.0f)
        throw std::invalid_argument("orthographic camera height must be positive");

    auto *root = Ogre::Root::getSingletonPtr();
    if (!root)
        throw std::logic_error("renderer projection requested before Ogre initialization");

    Ogre::Camera *camera = nullptr;
    auto managers = root->getSceneManagerIterator();
    while (managers.hasMoreElements())
    {
        Ogre::SceneManager *scene = managers.getNext();
        if (!scene)
            continue;

        camera = scene->findCameraNoThrow(Ogre::IdString("PlagCCamera"));
        if (camera)
            break;
    }
    if (!camera)
        throw std::logic_error("PlagC renderer camera is unavailable");

    if (projection == CameraProjection::Orthographic)
    {
        camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
        camera->setOrthoWindowHeight(orthographicHeight);
    }
    else
    {
        camera->setProjectionType(Ogre::PT_PERSPECTIVE);
    }
}
} // namespace PlagC
