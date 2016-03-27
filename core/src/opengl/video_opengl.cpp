#include <SDL.h>
#include <GL/glew.h>
#include "halley_gl.h"
#include "../api/video_api.h"
#include "SDL_syswm.h"
#include "gl_util.h"
using namespace Halley;

#ifdef _MSC_VER
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")
#endif

void VideoAPI::init()
{
	
}

void VideoAPI::deInit()
{
	if (running) {
		running = false;
		//loaderThread.join();
	}

	SDL_GL_MakeCurrent(window, nullptr);
	SDL_GL_DeleteContext(context);
	context = nullptr;
	SDL_DestroyWindow(window);
	window = nullptr;
	SDL_VideoQuit();

	std::cout << "Video terminated." << std::endl;
}


///////////////
// Constructor
VideoAPI::VideoAPI()
	: border(0)
	, windowType(WindowType::None)
	, initialized(false)
	, running(false)
{
}


/////////////
// Set video
void VideoAPI::setVideo(WindowType _windowType, const Vector2i _fullscreenSize, const Vector2i _windowedSize, const Vector2f _virtualSize, int screen)
{
	bool wasInit = initialized;

	// Initialize video mode
	if (!wasInit) {
		SDL_VideoInit(nullptr);
	}

	// Window size
	Vector2i windowSize = _windowType == WindowType::Fullscreen ? _fullscreenSize : _windowedSize;

#ifdef __ANDROID__
	// Android-specific overrides, since it should always be fullscreen and on the actual window size
	_fullscreen = true;
	windowSize = VideoAPI::getScreenSize();
#endif

	// Debug info
	//char buffer[1024];
	std::cout << std::endl << ConsoleColor(Console::GREEN) << "Initializing OpenGL...\n" << ConsoleColor();
	std::cout << "Drivers available:\n";
	for (int i = 0; i < SDL_GetNumVideoDrivers(); i++) {
		std::cout << "\t" << i << ": " << SDL_GetVideoDriver(i) << "\n";
	}
	std::cout << "Video driver: " << ConsoleColor(Console::DARK_GREY) << SDL_GetCurrentVideoDriver() << ConsoleColor() << std::endl;
	std::cout << "Window size: " << ConsoleColor(Console::DARK_GREY) << windowSize.x << "x" << windowSize.y << ConsoleColor() << std::endl;

	// Store values
	fullscreenSize = _fullscreenSize;
	windowedSize = _windowedSize;
	windowType = _windowType;
	virtualSize = _virtualSize;
	setWindowSize(windowSize);

	// Window name
	//String name = game.getName();
	//if (game.isDevBuild()) name += " [DEV BUILD]";
	String name = "Halley game";

	if (!wasInit) {
		// Set flags and GL attributes
		int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS;
		if (_windowType == WindowType::BorderlessWindow) {
			flags |= SDL_WINDOW_BORDERLESS;
		} else {
			flags |= SDL_WINDOW_RESIZABLE;
		}
		//if (_fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
		
		// Context options
#if defined(WITH_OPENGL_ES2)
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 0);
#else
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#ifndef _DEBUG
		//SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

		// Window position
		Vector2i winPos(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		if (screen < SDL_GetNumVideoDisplays()) {
			SDL_Rect rect;
			SDL_GetDisplayBounds(screen, &rect);
			winPos.x = rect.x + (rect.w - windowSize.x) / 2;
			winPos.y = rect.y + (rect.h - windowSize.y) / 2;
		}

		// Create window
		window = SDL_CreateWindow(name.c_str(), winPos.x, winPos.y, windowSize.x, windowSize.y, flags);
		if (!window)
			throw Exception(String("Error creating SDL window: ") + SDL_GetError());
#ifndef __ANDROID__
		SDL_SetWindowFullscreen(window, _windowType == WindowType::Fullscreen ? SDL_TRUE : SDL_FALSE);
#endif

		// Create OpenGL context
		SDL_GLContext context = SDL_GL_CreateContext(window);
		if (!context)
			throw Exception(String("Error creating OpenGL context: ") + SDL_GetError());
		if (SDL_GL_MakeCurrent(window, context) < 0)
			throw Exception(String("Error setting OpenGL context: ") + SDL_GetError());
		context = context;
		SDL_GL_SetSwapInterval(0);

		// Start loader thread
		if (!running) {
#ifdef ____WIN32__
			vid->loaderThread = TextureLoadQueue::startLoaderThread(window, &vid->running);
			vid->running = true;
#endif
		}

		// Initialize GLEW
#ifdef WITH_OPENGL
		GLenum err = glewInit();
		if (err != GLEW_OK)
			throw Exception(String("Error initializing GLEW: ") + (char*)(glewGetErrorString(err)));
#endif

		// Print OpenGL data
		bool extsOK = true;
		std::cout << "OpenGL initialized." << std::endl;
		std::cout << "\tVersion: " << ConsoleColor(Console::DARK_GREY) << glGetString(GL_VERSION) << ConsoleColor() << std::endl;
		std::cout << "\tVendor: " << ConsoleColor(Console::DARK_GREY) << glGetString(GL_VENDOR) << ConsoleColor() << std::endl;
		std::cout << "\tRenderer: " << ConsoleColor(Console::DARK_GREY) << glGetString(GL_RENDERER) << ConsoleColor() << std::endl;

		// Check for shader support
#if defined(WITH_OPENGL_ES2)
		bool shaderSupport = true;
#elif defined(WITH_OPENGL_ES)
		bool shaderSupport = false;
#else
		bool shaderSupport = GLEW_ARB_shading_language_100 != 0;
#endif

		// Retrieve and print shader data
		if (shaderSupport) {
			std::cout << "\tShaders: " << ConsoleColor(Console::DARK_GREY) << "enabled" << ConsoleColor() << std::endl;
#if defined(WITH_OPENGL)
			const char* glslVer = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION_ARB);
			std::cout << "\tGLSL Version: " << ConsoleColor(Console::DARK_GREY) << glslVer << ConsoleColor() << std::endl;
#elif defined(WITH_OPENGL_ES2)
			const char* glslVer = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
			std::cout << "\tGLSL Version: " << ConsoleColor(Console::DARK_GREY) << glslVer << ConsoleColor() << std::endl;
#endif
		} else {
			std::cout << "\tShaders: " << ConsoleColor(Console::DARK_GREY) << "disabled" << ConsoleColor() << std::endl;
			extsOK = false;
			throw Exception("Halley requires shader support.");
		}

		// Render-to-texture support
		bool fboSupport = true;//TextureRenderTargetFBO::isSupported();
		std::cout << "\tFramebuffer Objects: " << ConsoleColor(Console::DARK_GREY) << (fboSupport ? "enabled" : "disabled") << ConsoleColor() << std::endl;
#if defined(WITH_OPENGL)
		int n;
		glGetIntegerv(GL_AUX_BUFFERS, &n);
		//if (n > 0) TextureRenderTargetCopy::hasAuxBuffer = true;
#endif

		// Print extensions
		if (!extsOK) {
			std::cout << "\tExtensions: " << ConsoleColor(Console::DARK_GREY) << glGetString(GL_EXTENSIONS) << ConsoleColor() << std::endl;

			/*
			std::cout << "\tExtensions: " << ConsoleColor(Console::DARK_GREY);
			int nExtensions;
			glGetIntegerv(GL_NUM_EXTENSIONS, &nExtensions);
			for (int i = 0; i < n; i++) {
				String str = (const char*) glGetStringi(GL_EXTENSIONS, i);
				std::cout << str << " ";
			}
			std::cout << ConsoleColor() << std::endl;
			*/
		}
	} else {
		// Re-initializing

		// Update window
#ifndef __ANDROID__
		if (_windowType != WindowType::Fullscreen) SDL_SetWindowFullscreen(window, SDL_FALSE);
		SDL_SetWindowSize(window, windowSize.x, windowSize.y);
		if (_windowType == WindowType::Fullscreen) SDL_SetWindowFullscreen(window, SDL_TRUE);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#endif
	}

	// Clear buffer
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glCheckError();

	// Swap buffers
	flip();

	// Show window
	SDL_ShowWindow(window);
	initialized = true;
	std::cout << ConsoleColor(Console::GREEN) << "Video init done.\n" << ConsoleColor() << std::endl;
}

void VideoAPI::setWindowSize(Vector2i winSize)
{
	windowSize = winSize;
	updateWindowDimensions();
}

void VideoAPI::setVirtualSize(Vector2f vs)
{
	virtualSize = vs;
	updateWindowDimensions();
}

void VideoAPI::updateWindowDimensions()
{
	border = 0;
	if (virtualSize.x == 0 || virtualSize.y == 0) {
		virtualSize = p2 = Vector2f(windowSize);
		scale = 1;
	} else {
		float wAR = (float) windowSize.x / (float) windowSize.y;
		float vAR = virtualSize.x / virtualSize.y;
		p1 = Vector2f();
		p2 = virtualSize;
		if (wAR > vAR) {
			// Letterbox on left/right
			scale = windowSize.y / virtualSize.y;
			border = (virtualSize.y * wAR - virtualSize.x) * 0.5f * scale;
			p2 *= scale;
			p1.x += border;
			p2.x += border;
		} else {
			// Letterbox on top/bottom
			//float border = windowSize.y - windowSize.x / vAR;
			scale = windowSize.x / virtualSize.x;
			border = (virtualSize.x / wAR - virtualSize.y) * 0.5f * scale;
			p2 *= scale;
			p1.y += border;
			p2.y += border;
		}
	}
}

Vector2i VideoAPI::getScreenSize(int n) const
{
	if (n >= SDL_GetNumVideoDisplays()) {
		return Vector2i();
	}
	SDL_DisplayMode info;
	SDL_GetDesktopDisplayMode(n, &info);
	return Vector2i(info.w, info.h);
}

void VideoAPI::flip()
{
	SDL_GL_SwapWindow(window);
}

void VideoAPI::setFullscreen(bool fs)
{
	if (fs != (windowType == WindowType::Fullscreen)) {
		setVideo(fs ? WindowType::Fullscreen : WindowType::Window, fullscreenSize, windowedSize, virtualSize);
	}
}

void VideoAPI::toggleFullscreen()
{
	setFullscreen(!isFullscreen());
}

void VideoAPI::processEvent(SDL_Event& event)
{
	if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
		Vector2i size = Vector2i(event.window.data1, event.window.data2);
		setWindowSize(size);
	}
}

Rect4i VideoAPI::getWindowRect() const
{
	int x, y, w, h;
	SDL_GetWindowPosition(window, &x, &y);
	SDL_GetWindowSize(window, &w, &h);
	return Rect4i(x, y, w, h);
}

Rect4i VideoAPI::getDisplayRect() const
{
	SDL_Rect rect;
	SDL_GetDisplayBounds(0, &rect);
	return Rect4i(rect.x, rect.y, rect.w, rect.h);
}


/*
static void drawBox(spPainter painter, float x, float y, float w, float h)
{
	GLUtils glUtils;
	glUtils.setNumberOfTextureUnits(1);
	glUtils.setTextureUnit(0);
	glUtils.bindTexture(0);
	glUtils.setBlendType(Blend::Opaque);
	float vs[] = { x, y, x + w + 1, y, x + w + 1, y + h + 1, x, y + h + 1 };
	painter->drawQuad(Shader::getDefault(), vs);
}
*/

static void drawLetterbox() {
	// TODO
	/*
	Debug::trace("Game::RenderScreen drawing letterboxes");
	Camera::bindScreen();
	Vector2f p = Video::getOrigin();

	// Check if there's any need for it, i.e. window doesn't match game AR
	if (p.y > 0 || p.x > 0) {
		GLUtils glUtils;
		Vector2f s = Video::getVirtualSize();
		Rect4i oldView = glUtils.getViewPort();
		Rect4i view = Rect4i(0, 0, Video::getWindowSize().x, Video::getWindowSize().y);

		// Setting the viewport is necessary to draw outside game bounds, will work on certain drivers even with this off, so be careful
		glUtils.setViewPort(view, false);

		float border = Video::getBorder() / Video::getScale();

		if (p.y > 0) {
			// Top and bottom
			border *= float(oldView.getHeight()) / view.getHeight();
			drawBox(painter, 0, 0, s.x, border);
			drawBox(painter, 0, s.y + 1 - border, s.x, border);
		}
		if (p.x > 0) {
			// Left and right
			border *= float(oldView.getWidth()) / view.getWidth();
			drawBox(painter, 0, 0, border, s.y);
			drawBox(painter, s.x + 1 - border, 0, border, s.y);
		}

		glUtils.setViewPort(oldView);
	}
	Camera::resetBind();
	*/
}

void VideoAPI::startRender()
{
	Debug::trace("Game::RenderScreen begin");
	glClear(GL_COLOR_BUFFER_BIT);

	// TODO
	/*
	painter->resetDrawCalls();

	if (!TextureLoadQueue::hasLoaderThread()) {
		Debug::trace("Game::RenderScreen loading texture");
		TextureLoadQueue::get()->load(1);
		Debug::trace("Game::RenderScreen loaded texture");
	}
	*/
}

void VideoAPI::finishRender()
{
	// TODO
	// painter->flushQueue();

	drawLetterbox();

	Debug::trace("Game::RenderScreen flipping");
	flip();
	Debug::trace("Game::RenderScreen end");

	glCheckError();
}