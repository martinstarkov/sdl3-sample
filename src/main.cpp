#define SDL_MAIN_USE_CALLBACKS // This is necessary for the new callbacks API.

#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL_opengles2.h> // GLES2 / WebGL-style API
#else
#include <SDL3/SDL_opengl.h> // Desktop GL
#endif

#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <chrono>
#include <cmath>
#include <cstdint>
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

#ifdef __EMSCRIPTEN__
  // Simple textured-quad shader pipeline for WebGL / GLES2
  GLuint program = 0;
  GLuint vbo = 0;
  GLint uResolutionLoc = -1;
  GLint uTextureLoc = -1;
  GLint aPosLoc = -1;
  GLint aUVLoc = -1;
#endif
};

SDL_AppResult SDL_Fail() {
  SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
  return SDL_APP_FAILURE;
}

#ifdef __EMSCRIPTEN__
// ------------ GL function pointers loaded via SDL3 ------------

// We avoid depending on GL-specific typedefs like GLsizeiptr
// and just use standard C++ types where possible.

using GLCreateShaderProc = GLuint (*)(GLenum);
using GLShaderSourceProc = void (*)(GLuint, GLsizei, const char *const *,
                                    const GLint *);
using GLCompileShaderProc = void (*)(GLuint);
using GLGetShaderivProc = void (*)(GLuint, GLenum, GLint *);
using GLGetShaderInfoLogProc = void (*)(GLuint, GLsizei, GLsizei *, char *);
using GLDeleteShaderProc = void (*)(GLuint);

using GLCreateProgramProc = GLuint (*)(void);
using GLAttachShaderProc = void (*)(GLuint, GLuint);
using GLLinkProgramProc = void (*)(GLuint);
using GLGetProgramivProc = void (*)(GLuint, GLenum, GLint *);
using GLGetProgramInfoLogProc = void (*)(GLuint, GLsizei, GLsizei *, char *);
using GLUseProgramProc = void (*)(GLuint);

using GLGetUniformLocationProc = GLint (*)(GLuint, const char *);
using GLGetAttribLocationProc = GLint (*)(GLuint, const char *);

using GLGenBuffersProc = void (*)(GLsizei, GLuint *);
using GLDeleteBuffersProc = void (*)(GLsizei, const GLuint *);
using GLBindBufferProc = void (*)(GLenum, GLuint);
using GLBufferDataProc = void (*)(GLenum, std::intptr_t, const void *, GLenum);

using GLEnableVertexAttribArrayProc = void (*)(GLuint);
using GLVertexAttribPointerProc = void (*)(GLuint, GLint, GLenum, GLboolean,
                                           GLsizei, const void *);
using GLDrawArraysProc = void (*)(GLenum, GLint, GLsizei);

using GLUniform2fProc = void (*)(GLint, GLfloat, GLfloat);
using GLUniform1iProc = void (*)(GLint, GLint);
using GLActiveTextureProc = void (*)(GLenum);
using GLDisableVertexAttribArrayProc = void (*)(GLuint);

// Pointers
static GLCreateShaderProc pglCreateShader = nullptr;
static GLShaderSourceProc pglShaderSource = nullptr;
static GLCompileShaderProc pglCompileShader = nullptr;
static GLGetShaderivProc pglGetShaderiv = nullptr;
static GLGetShaderInfoLogProc pglGetShaderInfoLog = nullptr;
static GLDeleteShaderProc pglDeleteShader = nullptr;

static GLCreateProgramProc pglCreateProgram = nullptr;
static GLAttachShaderProc pglAttachShader = nullptr;
static GLLinkProgramProc pglLinkProgram = nullptr;
static GLGetProgramivProc pglGetProgramiv = nullptr;
static GLGetProgramInfoLogProc pglGetProgramInfoLog = nullptr;
static GLUseProgramProc pglUseProgram = nullptr;

static GLGetUniformLocationProc pglGetUniformLocation = nullptr;
static GLGetAttribLocationProc pglGetAttribLocation = nullptr;

static GLGenBuffersProc pglGenBuffers = nullptr;
static GLDeleteBuffersProc pglDeleteBuffers = nullptr;
static GLBindBufferProc pglBindBuffer = nullptr;
static GLBufferDataProc pglBufferData = nullptr;

static GLEnableVertexAttribArrayProc pglEnableVertexAttribArray = nullptr;
static GLVertexAttribPointerProc pglVertexAttribPointer = nullptr;
static GLDrawArraysProc pglDrawArrays = nullptr;

static GLUniform2fProc pglUniform2f = nullptr;
static GLUniform1iProc pglUniform1i = nullptr;
static GLActiveTextureProc pglActiveTexture = nullptr;
static GLDisableVertexAttribArrayProc pglDisableVertexAttribArray = nullptr;

static bool LoadGLES2Functions() {
#define LOAD_GL_FUNC(name)                                                     \
  do {                                                                         \
    p##name =                                                                  \
        reinterpret_cast<decltype(p##name)>(SDL_GL_GetProcAddress(#name));     \
    if (!p##name) {                                                            \
      SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to load GL function %s",   \
                   #name);                                                     \
      return false;                                                            \
    }                                                                          \
  } while (0)

  LOAD_GL_FUNC(glCreateShader);
  LOAD_GL_FUNC(glShaderSource);
  LOAD_GL_FUNC(glCompileShader);
  LOAD_GL_FUNC(glGetShaderiv);
  LOAD_GL_FUNC(glGetShaderInfoLog);
  LOAD_GL_FUNC(glDeleteShader);

  LOAD_GL_FUNC(glCreateProgram);
  LOAD_GL_FUNC(glAttachShader);
  LOAD_GL_FUNC(glLinkProgram);
  LOAD_GL_FUNC(glGetProgramiv);
  LOAD_GL_FUNC(glGetProgramInfoLog);
  LOAD_GL_FUNC(glUseProgram);

  LOAD_GL_FUNC(glGetUniformLocation);
  LOAD_GL_FUNC(glGetAttribLocation);

  LOAD_GL_FUNC(glGenBuffers);
  LOAD_GL_FUNC(glDeleteBuffers);
  LOAD_GL_FUNC(glBindBuffer);
  LOAD_GL_FUNC(glBufferData);

  LOAD_GL_FUNC(glEnableVertexAttribArray);
  LOAD_GL_FUNC(glVertexAttribPointer);
  LOAD_GL_FUNC(glDrawArrays);

  LOAD_GL_FUNC(glUniform2f);
  LOAD_GL_FUNC(glUniform1i);
  LOAD_GL_FUNC(glActiveTexture);
  LOAD_GL_FUNC(glDisableVertexAttribArray);

#undef LOAD_GL_FUNC
  return true;
}

// ------------ Shader helpers using loaded functions ------------

static GLuint CompileShader(GLenum type, const char *source) {
  GLuint shader = pglCreateShader(type);
  if (!shader) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "glCreateShader failed");
    return 0;
  }

  pglShaderSource(shader, 1, &source, nullptr);
  pglCompileShader(shader);

  GLint ok = GL_FALSE;
  pglGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint logLen = 0;
    pglGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    std::string log(logLen, '\0');
    GLsizei written = 0;
    if (logLen > 0) {
      pglGetShaderInfoLog(shader, logLen, &written, log.data());
    }
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Shader compile failed: %s",
                 log.c_str());
    pglDeleteShader(shader);
    return 0;
  }

  return shader;
}

static GLuint CreateTexturedQuadProgram() {
  // No #version so this works both in desktop GL 2.x and GLES 2 / WebGL1.
  static const char *kVertexShaderSrc = R"(
#ifdef GL_ES
precision mediump float;
#endif

attribute vec2 aPos;
attribute vec2 aUV;
varying vec2 vUV;
uniform vec2 uResolution;

void main() {
    // Convert from pixel coordinates (0..width, 0..height) to clip space.
    vec2 zeroToOne = aPos / uResolution;
    vec2 zeroToTwo = zeroToOne * 2.0;
    vec2 clipSpace = zeroToTwo - 1.0;

    // Flip Y so origin is top-left, y goes down.
    clipSpace.y = -clipSpace.y;

    gl_Position = vec4(clipSpace, 0.0, 1.0);
    vUV = aUV;
}
)";

  static const char *kFragmentShaderSrc = R"(
#ifdef GL_ES
precision mediump float;
#endif

varying vec2 vUV;
uniform sampler2D uTexture;

void main() {
    gl_FragColor = texture2D(uTexture, vUV);
}
)";

  GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
  if (!vs) {
    return 0;
  }
  GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
  if (!fs) {
    pglDeleteShader(vs);
    return 0;
  }

  GLuint prog = pglCreateProgram();
  if (!prog) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "glCreateProgram failed");
    pglDeleteShader(vs);
    pglDeleteShader(fs);
    return 0;
  }

  pglAttachShader(prog, vs);
  pglAttachShader(prog, fs);
  pglLinkProgram(prog);

  pglDeleteShader(vs);
  pglDeleteShader(fs);

  GLint ok = GL_FALSE;
  pglGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint logLen = 0;
    pglGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
    std::string log(logLen, '\0');
    GLsizei written = 0;
    if (logLen > 0) {
      pglGetProgramInfoLog(prog, logLen, &written, log.data());
    }
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Program link failed: %s",
                 log.c_str());
    glDeleteProgram(prog);
    return 0;
  }

  return prog;
}
#endif // __EMSCRIPTEN__

bool InitGL(SDL_Window *window, GLRenderer &out) {
  // Request a compatibility-ish profile for desktop.
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

#ifdef __EMSCRIPTEN__
  // Load all GLES2/WebGL functions via SDL3
  if (!LoadGLES2Functions()) {
    return false;
  }
#endif

  // VSync
  SDL_GL_SetSwapInterval(1);

  int w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);

  glViewport(0, 0, w, h);

#ifndef __EMSCRIPTEN__
  // Immediate-mode projection only for native builds.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // Origin at top-left, y downwards, z in [-1,1]
  glOrtho(0, w, h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_TEXTURE_2D);
#endif

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef __EMSCRIPTEN__
  // WebGL / GLES2 shader path (no legacy GL emulation).
  out.program = CreateTexturedQuadProgram();
  if (!out.program) {
    return false;
  }

  pglUseProgram(out.program);

  out.uResolutionLoc = pglGetUniformLocation(out.program, "uResolution");
  out.uTextureLoc = pglGetUniformLocation(out.program, "uTexture");
  out.aPosLoc = pglGetAttribLocation(out.program, "aPos");
  out.aUVLoc = pglGetAttribLocation(out.program, "aUV");

  if (out.uResolutionLoc == -1 || out.uTextureLoc == -1 || out.aPosLoc == -1 ||
      out.aUVLoc == -1) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Failed to get shader locations");
    return false;
  }

  pglGenBuffers(1, &out.vbo);
  if (!out.vbo) {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "glGenBuffers failed");
    return false;
  }

  // Use texture unit 0.
  pglActiveTexture(GL_TEXTURE0);
  pglUniform1i(out.uTextureLoc, 0);

  // 2D rendering only.
  glDisable(GL_DEPTH_TEST);
#endif

  return true;
}

void ShutdownGL(SDL_Window *window, GLRenderer &renderer) {
#ifdef __EMSCRIPTEN__
  if (renderer.vbo) {
    pglDeleteBuffers(1, &renderer.vbo);
    renderer.vbo = 0;
  }
  if (renderer.program) {
    glDeleteProgram(renderer.program);
    renderer.program = 0;
  }
#endif

  if (renderer.context) {
    SDL_GL_MakeCurrent(window, nullptr);
    SDL_GL_DestroyContext(renderer.context);
    renderer.context = nullptr;
  }
}

GLTexture CreateTextureFromSurface(SDL_Surface *surface) {
  GLTexture tex;
  if (!surface) {
    return tex;
  }

  // Convert to RGBA32 so we know what we're uploading
  SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
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

void DestroyTexture(GLTexture &tex) {
  if (tex.id) {
    glDeleteTextures(1, &tex.id);
    tex.id = 0;
  }
}

void BeginFrame(GLRenderer &renderer, SDL_Window *window, float r, float g,
                float b) {
  int w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);

  glViewport(0, 0, w, h);

#ifndef __EMSCRIPTEN__
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, h, 0, -1, 1); // origin top-left
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
#else
  pglUseProgram(renderer.program);
  pglUniform2f(renderer.uResolutionLoc, static_cast<float>(w),
               static_cast<float>(h));
#endif

  glClearColor(r, g, b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void DrawTexture(GLRenderer &renderer, const GLTexture &tex, float x, float y,
                 float w, float h) {
  if (!tex.id) {
    return;
  }

#ifdef __EMSCRIPTEN__
  struct Vertex {
    float x, y;
    float u, v;
  };

  // Two triangles forming the quad.
  Vertex verts[6] = {
      // 1st triangle
      {x, y, 0.0f, 0.0f},
      {x + w, y, 1.0f, 0.0f},
      {x + w, y + h, 1.0f, 1.0f},
      // 2nd triangle
      {x, y, 0.0f, 0.0f},
      {x + w, y + h, 1.0f, 1.0f},
      {x, y + h, 0.0f, 1.0f},
  };

  pglUseProgram(renderer.program);

  pglActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex.id);

  pglBindBuffer(GL_ARRAY_BUFFER, renderer.vbo);
  pglBufferData(GL_ARRAY_BUFFER, static_cast<std::intptr_t>(sizeof(verts)),
                verts, GL_DYNAMIC_DRAW);

  pglEnableVertexAttribArray(renderer.aPosLoc);
  pglVertexAttribPointer(renderer.aPosLoc, 2, GL_FLOAT, GL_FALSE,
                         sizeof(Vertex), reinterpret_cast<const void *>(0));

  pglEnableVertexAttribArray(renderer.aUVLoc);
  pglVertexAttribPointer(renderer.aUVLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<const void *>(2 * sizeof(float)));

  pglDrawArrays(GL_TRIANGLES, 0, 6);

  pglDisableVertexAttribArray(renderer.aPosLoc);
  pglDisableVertexAttribArray(renderer.aUVLoc);
#else
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
#endif
}

void EndFrame(SDL_Window *window) { SDL_GL_SwapWindow(window); }

// ------------------- App state -------------------

struct AppContext {
  SDL_Window *window = nullptr;
  GLRenderer gl;
  GLTexture messageTex;
  GLTexture imageTex;
  SDL_FRect messageDest{};
  MIX_Track *track = nullptr;
  SDL_AppResult app_quit = SDL_APP_CONTINUE;
};

// ------------------- SDL callbacks -------------------

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
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
  SDL_Window *window = SDL_CreateWindow(
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
  TTF_Font *font = TTF_OpenFont(fontPath.string().c_str(), 36);
  if (!font) {
    return SDL_Fail();
  }

  // render the font to a surface
  const std::string_view text = "Hello SDL!";
  SDL_Color white{255, 255, 255, 255};
  SDL_Surface *surfaceMessage =
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

  // load the image (PNG in the sample)
  SDL_Surface *svg_surface =
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
  MIX_Mixer *mixer =
      MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (mixer == nullptr) {
    return SDL_Fail();
  }

  MIX_Track *mixerTrack = MIX_CreateTrack(mixer);
  if (!mixerTrack) {
    return SDL_Fail();
  }

  // load the music
  auto musicPath = basePath / "assets/the_entertainer.ogg";
  MIX_Audio *music = MIX_LoadAudio(mixer, musicPath.string().c_str(), false);
  if (!music) {
    return SDL_Fail();
  }

  // play the music (loops)
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

  auto *app = new AppContext{};
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

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  auto *app = static_cast<AppContext *>(appstate);

  if (event->type == SDL_EVENT_QUIT) {
    app->app_quit = SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  auto *app = static_cast<AppContext *>(appstate);

  // animated background colour
  float time = SDL_GetTicks() / 1000.0f;
  float red = (std::sin(time) + 1.0f) * 0.5f;
  float green = (std::sin(time / 2.0f) + 1.0f) * 0.5f;
  float blue = (std::sin(time * 2.0f) + 1.0f) * 0.5f;

  BeginFrame(app->gl, app->window, red, green, blue);

  int winW, winH;
  SDL_GetWindowSizeInPixels(app->window, &winW, &winH);

  // draw image to cover the window
  DrawTexture(app->gl, app->imageTex, 0.0f, 0.0f, static_cast<float>(winW),
              static_cast<float>(winH));

  // draw text at its destination rect
  DrawTexture(app->gl, app->messageTex, app->messageDest.x, app->messageDest.y,
              app->messageDest.w, app->messageDest.h);

  EndFrame(app->window);

  return app->app_quit;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  (void)result;

  auto *app = static_cast<AppContext *>(appstate);
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
