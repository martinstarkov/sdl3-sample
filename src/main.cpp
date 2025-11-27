#define SDL_MAIN_USE_CALLBACKS  // This is necessary for the new callbacks API.

#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>

constexpr uint32_t windowStartWidth = 400;
constexpr uint32_t windowStartHeight = 400;

// ------------------- Simple OpenGL helpers -------------------

struct GLTexture {
  GLuint id = 0;
  int width = 0;
  int height = 0;
};

struct GLRenderer {
  SDL_GLContext context = nullptr;
};

SDL_AppResult SDL_Fail() {
  SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
  return SDL_APP_FAILURE;
}

bool InitGL(SDL_Window* window, GLRenderer& out) {
  // Request a compatibility profile so immediate-mode works on most platforms.
  // (For macOS / strict core profile you'd switch to modern GL with shaders.)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  out.context = SDL_GL_CreateContext(window);
  if (!out.context) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL_GL_CreateContext failed: %s",
                 SDL_GetError());
    return false;
  }

  if (!SDL_GL_MakeCurrent(window, out.context)) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL_GL_MakeCurrent failed: %s",
                 SDL_GetError());
    return false;
  }

  // VSync
  SDL_GL_SetSwapInterval(1);

  int w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);

  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // Origin at top-left, y downwards, z in [-1,1]
  glOrtho(0, w, h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  return true;
}

void ShutdownGL(SDL_Window* window, GLRenderer& renderer) {
  if (renderer.context) {
    SDL_GL_MakeCurrent(window, nullptr);
    SDL_GL_DestroyContext(renderer.context);
    renderer.context = nullptr;
  }
}

GLTexture CreateTextureFromSurface(SDL_Surface* surface) {
  GLTexture tex;
  if (!surface) {
    return tex;
  }

  // Convert to RGBA32 so we know what we're uploading
  SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  if (!rgba) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL_ConvertSurfaceFormat failed: %s",
                 SDL_GetError());
    return tex;
  }

  glGenTextures(1, &tex.id);
  glBindTexture(GL_TEXTURE_2D, tex.id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  tex.width = rgba->w;
  tex.height = rgba->h;

  GLint prevAlign = 0;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba->pixels);

  glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);

  SDL_DestroySurface(rgba);

  return tex;
}

void DestroyTexture(GLTexture& tex) {
  if (tex.id) {
    glDeleteTextures(1, &tex.id);
    tex.id = 0;
  }
}

void BeginFrame(GLRenderer&, SDL_Window* window, float r, float g, float b) {
  int w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);

  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, h, 0, -1, 1);  // origin top-left
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void DrawTexture(const GLTexture& tex, float x, float y, float w, float h) {
  if (!tex.id) {
    return;
  }

  glBindTexture(GL_TEXTURE_2D, tex.id);

  glBegin(GL_TRIANGLES);
  // 1st triangle
  glTexCoord2f(0.f, 0.f);
  glVertex2f(x, y);
  glTexCoord2f(1.f, 0.f);
  glVertex2f(x + w, y);
  glTexCoord2f(1.f, 1.f);
  glVertex2f(x + w, y + h);
  // 2nd triangle
  glTexCoord2f(0.f, 0.f);
  glVertex2f(x, y);
  glTexCoord2f(1.f, 1.f);
  glVertex2f(x + w, y + h);
  glTexCoord2f(0.f, 1.f);
  glVertex2f(x, y + h);
  glEnd();
}

void EndFrame(SDL_Window* window) { SDL_GL_SwapWindow(window); }

// ------------------- App state -------------------

struct AppContext {
  SDL_Window* window = nullptr;
  GLRenderer gl;
  GLTexture messageTex;
  GLTexture imageTex;
  SDL_FRect messageDest{};
  MIX_Track* track = nullptr;
  SDL_AppResult app_quit = SDL_APP_CONTINUE;
};

// ------------------- SDL callbacks -------------------

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  // init the library
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    return SDL_Fail();
  }

  // init TTF
  if (!TTF_Init()) {
    return SDL_Fail();
  }

  // init Mixer
  if (!MIX_Init()) {
    return SDL_Fail();
  }

  // create a window (with OpenGL)
  SDL_Window* window = SDL_CreateWindow(
      "SDL Minimal Sample (OpenGL)", windowStartWidth, windowStartHeight,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL);
  if (!window) {
    return SDL_Fail();
  }

  // init our OpenGL renderer
  GLRenderer glRenderer;
  if (!InitGL(window, glRenderer)) {
    return SDL_Fail();
  }

  // load the font
#if __ANDROID__
  std::filesystem::path basePath = "assets";
#else
  auto basePathPtr = SDL_GetBasePath();
  if (!basePathPtr) {
    return SDL_Fail();
  }
  const std::filesystem::path basePath = basePathPtr;
#endif

  const auto fontPath = basePath / "assets/Inter-VariableFont.ttf";
  TTF_Font* font = TTF_OpenFont(fontPath.string().c_str(), 36);
  if (!font) {
    return SDL_Fail();
  }

  // render the font to a surface
  const std::string_view text = "Hello SDL!";
  SDL_Color white{255, 255, 255, 255};
  SDL_Surface* surfaceMessage =
      TTF_RenderText_Solid(font, text.data(), text.length(), white);

  if (!surfaceMessage) {
    TTF_CloseFont(font);
    return SDL_Fail();
  }

  // make an OpenGL texture from the surface
  GLTexture messageTex = CreateTextureFromSurface(surfaceMessage);

  // we no longer need the font or the surface, so we can destroy those now.
  TTF_CloseFont(font);
  SDL_DestroySurface(surfaceMessage);

  if (!messageTex.id) {
    return SDL_Fail();
  }

  // get the on-screen dimensions of the text
  SDL_FRect text_rect{
      .x = 0.0f,
      .y = 0.0f,
      .w = static_cast<float>(messageTex.width),
      .h = static_cast<float>(messageTex.height),
  };

  // load the image (SVG in the sample, but same idea for PNG/JPG)
  SDL_Surface* svg_surface =
      IMG_Load((basePath / "assets/logo.png").string().c_str());
  if (!svg_surface) {
    return SDL_Fail();
  }

  GLTexture imageTex = CreateTextureFromSurface(svg_surface);
  SDL_DestroySurface(svg_surface);

  if (!imageTex.id) {
    return SDL_Fail();
  }

  // init SDL Mixer
  MIX_Mixer* mixer =
      MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (mixer == nullptr) {
    return SDL_Fail();
  }

  MIX_Track* mixerTrack = MIX_CreateTrack(mixer);
  if (!mixerTrack) {
    return SDL_Fail();
  }

  // load the music
  auto musicPath = basePath / "assets/the_entertainer.ogg";
  MIX_Audio* music = MIX_LoadAudio(mixer, musicPath.string().c_str(), false);
  if (!music) {
    return SDL_Fail();
  }

  // play the music (does not loop)
  MIX_SetTrackAudio(mixerTrack, music);
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
  MIX_PlayTrack(mixerTrack, props);

  // print some information about the window
  SDL_ShowWindow(window);
  {
    int width, height, bbwidth, bbheight;
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
    SDL_Log("Window size: %ix%i", width, height);
    SDL_Log("Backbuffer size: %ix%i", bbwidth, bbheight);
    if (width != bbwidth) {
      SDL_Log("This is a highdpi environment.");
    }
  }

  auto* app = new AppContext{};
  app->window = window;
  app->gl = glRenderer;
  app->messageTex = messageTex;
  app->imageTex = imageTex;
  app->messageDest = text_rect;
  app->track = mixerTrack;

  *appstate = app;

  SDL_Log("Application started successfully (OpenGL renderer)!");

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto* app = static_cast<AppContext*>(appstate);

  if (event->type == SDL_EVENT_QUIT) {
    app->app_quit = SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto* app = static_cast<AppContext*>(appstate);

  // animated background colour
  float time = SDL_GetTicks() / 1000.0f;
  float red = (std::sin(time) + 1.0f) * 0.5f;
  float green = (std::sin(time / 2.0f) + 1.0f) * 0.5f;
  float blue = (std::sin(time * 2.0f) + 1.0f) * 0.5f;

  BeginFrame(app->gl, app->window, red, green, blue);

  int winW, winH;
  SDL_GetWindowSizeInPixels(app->window, &winW, &winH);

  // draw image to cover the window
  DrawTexture(app->imageTex, 0.0f, 0.0f, static_cast<float>(winW),
              static_cast<float>(winH));

  // draw text at its destination rect
  DrawTexture(app->messageTex, app->messageDest.x, app->messageDest.y,
              app->messageDest.w, app->messageDest.h);

  EndFrame(app->window);

  return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  (void)result;

  auto* app = static_cast<AppContext*>(appstate);
  if (app) {
    // fade out music a bit
    if (app->track) {
      MIX_StopTrack(app->track, MIX_TrackMSToFrames(app->track, 1000));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    DestroyTexture(app->messageTex);
    DestroyTexture(app->imageTex);

    ShutdownGL(app->window, app->gl);
    SDL_DestroyWindow(app->window);

    delete app;
  }

  TTF_Quit();
  MIX_Quit();

  SDL_Log("Application quit successfully!");
  SDL_Quit();
}
