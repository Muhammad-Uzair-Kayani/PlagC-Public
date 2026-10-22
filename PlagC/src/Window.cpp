#include <PlagC/Window.h>
#include <PlagC/Events.h>
#include <PlagC/Thread.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifndef PC_OGRE_NULL_ONLY
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#endif
#include <stdexcept>
namespace PlagC {
namespace { bool activeWindow = false; }
struct Window::Impl {
    GLFWwindow* handle = nullptr;
    EventBus* events = nullptr;
    bool eventFailure = false;
    bool nullPlatform = false;
    void Emit(Event event) noexcept { try { if (!events->Post(event)) eventFailure = true; } catch (...) { eventFailure = true; } }
    static Impl& Self(GLFWwindow* window) { return *static_cast<Impl*>(glfwGetWindowUserPointer(window)); }
};
Window::Window(EventBus& events, const WindowConfig& config) : impl_(std::make_unique<Impl>()) {
    Thread::RequireMain();
    impl_->nullPlatform = config.nullPlatform;
    if (activeWindow) throw std::logic_error("Phase 1 supports one engine window");
    if (config.width <= 0 || config.height <= 0) throw std::invalid_argument("window size must be positive");
    if (config.nullPlatform) glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    else glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
    if (!glfwInit()) { const char* error=nullptr; glfwGetError(&error); throw std::runtime_error(std::string("GLFW initialization failed: ")+(error?error:"unknown error")); }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    impl_->handle = glfwCreateWindow(config.width,config.height,config.title.c_str(),nullptr,nullptr);
    if (!impl_->handle) { glfwTerminate(); throw std::runtime_error("GLFW window creation failed"); }
    activeWindow = true;
    impl_->events = &events;
    glfwSetWindowUserPointer(impl_->handle, impl_.get());
    glfwSetWindowCloseCallback(impl_->handle, [](GLFWwindow* w){ Impl::Self(w).Emit({EventType::Close}); });
    glfwSetWindowSizeCallback(impl_->handle, [](GLFWwindow* w,int x,int y){ Impl::Self(w).Emit({EventType::Resize, double(x), double(y)}); });
    glfwSetFramebufferSizeCallback(impl_->handle, [](GLFWwindow* w,int x,int y){ Impl::Self(w).Emit({EventType::FramebufferResize,double(x),double(y)}); });
    glfwSetWindowPosCallback(impl_->handle, [](GLFWwindow* w,int x,int y){ Impl::Self(w).Emit({EventType::Move,double(x),double(y)}); });
    glfwSetWindowFocusCallback(impl_->handle, [](GLFWwindow* w,int value){ Impl::Self(w).Emit({EventType::Focus,0,0,value}); });
    glfwSetWindowIconifyCallback(impl_->handle, [](GLFWwindow* w,int value){ Impl::Self(w).Emit({EventType::Minimize,0,0,value}); });
    glfwSetKeyCallback(impl_->handle, [](GLFWwindow* w,int key,int,int action,int mods){
        Impl::Self(w).Emit({action == GLFW_RELEASE ? EventType::KeyReleased : action == GLFW_REPEAT ? EventType::KeyRepeated : EventType::KeyPressed,0,0,key,mods});
    });
    glfwSetCharCallback(impl_->handle, [](GLFWwindow* w,unsigned code){ Impl::Self(w).Emit({EventType::TextInput,0,0,static_cast<int>(code)}); });
    glfwSetCursorPosCallback(impl_->handle, [](GLFWwindow* w,double x,double y){ Impl::Self(w).Emit({EventType::MouseMoved,x,y}); });
    glfwSetScrollCallback(impl_->handle, [](GLFWwindow* w,double x,double y){ Impl::Self(w).Emit({EventType::MouseScrolled,x,y}); });
    glfwSetMouseButtonCallback(impl_->handle, [](GLFWwindow* w,int button,int action,int mods){ Impl::Self(w).Emit({action == GLFW_PRESS ? EventType::MousePressed : EventType::MouseReleased,0,0,button,mods}); });
}
Window::~Window() { Thread::RequireMain(); if (impl_->handle) glfwDestroyWindow(impl_->handle); glfwTerminate(); activeWindow = false; }
void Window::Poll() { Thread::RequireMain(); glfwPollEvents(); if (impl_->eventFailure) { impl_->eventFailure = false; throw std::runtime_error("window event delivery failed/overflowed"); } }
bool Window::ShouldClose() const { Thread::RequireMain(); return glfwWindowShouldClose(impl_->handle) != 0; }
void Window::RequestClose() { Thread::RequireMain(); glfwSetWindowShouldClose(impl_->handle, GLFW_TRUE); }
void Window::SetTitle(const std::string& value) { Thread::RequireMain(); glfwSetWindowTitle(impl_->handle,value.c_str()); }
void Window::Resize(int w,int h) { Thread::RequireMain(); if (w<=0 || h<=0) throw std::invalid_argument("window size must be positive"); glfwSetWindowSize(impl_->handle,w,h); }
int Window::Width() const { Thread::RequireMain(); int w,h; glfwGetWindowSize(impl_->handle,&w,&h); return w; }
int Window::Height() const { Thread::RequireMain(); int w,h; glfwGetWindowSize(impl_->handle,&w,&h); return h; }
bool Window::Minimized() const { Thread::RequireMain(); return glfwGetWindowAttrib(impl_->handle,GLFW_ICONIFIED) != 0; }
NativeWindow Window::NativeHandle() const {
    Thread::RequireMain();
    if(impl_->nullPlatform)return {};
#if defined(PC_OGRE_NULL_ONLY)
    return {};
#elif defined(_WIN32)
    return {NativeWindow::Kind::Win32,reinterpret_cast<std::uintptr_t>(glfwGetWin32Window(impl_->handle))};
#elif defined(__APPLE__)
    return {NativeWindow::Kind::Cocoa,reinterpret_cast<std::uintptr_t>(glfwGetCocoaWindow(impl_->handle))};
#else
    return {NativeWindow::Kind::X11,static_cast<std::uintptr_t>(glfwGetX11Window(impl_->handle))};
#endif
}
}
