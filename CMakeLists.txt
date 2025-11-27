cmake_minimum_required(VERSION 3.16)

# set the output directory for built objects. This makes sure that the dynamic
# library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories.
set(CMAKE_INSTALL_PREFIX
    "${CMAKE_BINARY_DIR}"
    CACHE INTERNAL "")

# Declare the project
project(sdl-min)

# Put downloaded dependencies into a local "external" folder under the build dir
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/external")
include(FetchContent)

# ---------------------------------------------------------------------------
# SDL core: only what we actually use
# ---------------------------------------------------------------------------

# We need VIDEO (window + keyboard/mouse events) and AUDIO (SDL_mixer output).
# Everything else can go.
set(SDL_AUDIO ON) # required for SDL_mixer to talk to the audio device
set(SDL_VIDEO ON)

set(SDL_RENDER OFF) # we have our own renderer
set(SDL_GPU OFF)
set(SDL_CAMERA OFF)
set(SDL_JOYSTICK OFF) # set ON if you actually use controllers
set(SDL_HAPTIC OFF)
set(SDL_HIDAPI OFF)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
set(SDL_TRAY OFF)

# Optional: if SDL tries to build extra tools/tests when used as a subproject,
# these keep things lean. They are usually off by default in subdirs, but
# harmless.
set(SDL_TEST OFF)
set(SDL_INSTALL OFF)

if((APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin") OR EMSCRIPTEN)
  set(BUILD_SHARED_LIBS
      OFF
      CACHE INTERNAL "") # Disable shared builds on platforms where it does not
                         # make sense to use them
  set(SDL_SHARED OFF)
else()
  set(SDL_SHARED ON)
endif()

if(MSVC)
  if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    add_definitions(/MP) # parallelize each target, unless Ninja is the
                         # generator
  endif()
endif()

# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

# Create an executable or a shared library based on the platform and add our
# sources to it
if(ANDROID)
  # The SDL java code is hardcoded to load libmain.so on android, so we need to
  # change EXECUTABLE_NAME
  set(EXECUTABLE_NAME main)
  add_library(${EXECUTABLE_NAME} SHARED)
else()
  add_executable(${EXECUTABLE_NAME})
endif()

# Add your sources to the target
target_sources(${EXECUTABLE_NAME} PRIVATE src/main.cpp
                                          src/iosLaunchScreen.storyboard)
# What is iosLaunchScreen.storyboard? This file describes what Apple's mobile
# platforms should show the user while the application is starting up. If you
# don't include one, then you get placed in a compatibility mode that does not
# allow HighDPI. This file is referenced inside Info.plist.in, where it is
# marked as the launch screen file. It is also ignored on non-Apple platforms.

# To get an app icon on Apple platforms, add it to your executable. Afterward,
# the image file in Info.plist.in.
if(APPLE)
  target_sources("${EXECUTABLE_NAME}" PRIVATE "src/logo.png")
endif()

# Set C++ version
target_compile_features(${EXECUTABLE_NAME} PUBLIC cxx_std_20)

# on Web targets, we need CMake to generate a HTML webpage.
if(EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX
      ".html"
      CACHE INTERNAL "")
endif()

# Configure SDL by calling its CMake file. we use EXCLUDE_FROM_ALL so that its
# install targets and configs don't pollute upwards into our configuration.
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG main # or a specific release tag
)

# If you don't want SDL_ttf, then remove this section. SDL_ttf – we just want
# TTF rendering, no extra samples/install/emoji/etc.
set(SDLTTF_VENDORED ON) # bundling freetype/harfbuzz/plutosvg if used

# Optional features — disable for minimum footprint
set(SDLTTF_HARFBUZZ OFF) # OFF if you don't need complex text shaping
set(SDLTTF_PLUTOSVG OFF) # OFF if you don't need color emoji
set(SDLTTF_SAMPLES OFF)
set(SDLTTF_INSTALL OFF)
set(SDLTTF_STRICT OFF)
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG main)

# SDL_mixer – we only want: WAV (built-in), OGG, MP3
set(SDLMIXER_VENDORED ON)

# Enable decoders we care about
set(SDLMIXER_MP3_DRMP3 ON) # MP3 via dr_mp3 (no external mpg123)
set(SDLMIXER_VORBIS_STB ON) # OGG Vorbis via stb_vorbis

# WAV is built-in; there's no separate toggle.

# Disable everything else
set(SDLMIXER_MP3_MPG123 OFF)
set(SDLMIXER_FLAC_DRFLAC OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_WAVPACK OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MOD_XMP OFF)

set(SDLMIXER_MIDI_NATIVE OFF)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF)
set(SDLMIXER_MIDI_TIMIDITY OFF)

# If your SDL_mixer version has it:
set(SDLMIXER_STRICT OFF)
FetchContent_Declare(
  SDL3_mixer
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
  GIT_TAG main)

# SDL_image (used for loading various image formats) SDL_image (only PNG + JPEG)
set(SDLIMAGE_VENDORED ON)

# Backends: stb is enough in most cases (no extra system libs), keep the others
# off
set(SDLIMAGE_BACKEND_STB ON)
set(SDLIMAGE_BACKEND_WIC OFF) # Windows imaging; turn ON only if you want it
set(SDLIMAGE_BACKEND_IMAGEIO OFF) # On Apple, you *can* leave this ON if you
                                  # like

# Format support: ON only for PNG + JPEG, OFF for everything else
set(SDLIMAGE_PNG ON)
set(SDLIMAGE_JPG ON) # Some versions use SDLIMAGE_JPG...
set(SDLIMAGE_JPEG ON) # ... others use SDLIMAGE_JPEG; set both just in case.

set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_GIF OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_TIFF OFF) # (depending on exact option name)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_SVG OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_XV OFF)

# If present in your version, keep strict/off so missing libs just disable
# formats, instead of killing the configure step.
set(SDLIMAGE_STRICT OFF)
FetchContent_Declare(
  SDL3_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG main)

FetchContent_MakeAvailable(SDL3 SDL3_ttf SDL3_mixer SDL3_image)

# --- OpenGL for our custom renderer ---
if(WIN32)
  # On Windows, OpenGL is in opengl32.lib
  set(OPENGL_LIB opengl32)
elseif(APPLE)
  # On macOS, link against the OpenGL framework
  find_library(OPENGL_LIB OpenGL REQUIRED)
else()
  # On Linux and others, use CMake's OpenGL package
  find_package(OpenGL REQUIRED)
  set(OPENGL_LIB OpenGL::GL)
endif()

# Link SDL to our executable. This also makes its include directory available to
# us.
target_link_libraries(
  ${EXECUTABLE_NAME}
  PUBLIC SDL3_ttf::SDL3_ttf # remove if you are not using SDL_ttf
         SDL3_mixer::SDL3_mixer # remove if you are not using SDL_mixer
         SDL3_image::SDL3_image # remove if you are not using SDL_image
         SDL3::SDL3 # If using satelite libraries, SDL must be the last item in
                    # the list.
         ${OPENGL_LIB})

# SDL_Image bug: https://github.com/libsdl-org/SDL_image/issues/506
if(APPLE AND NOT BUILD_SHARED_LIBS)
  find_library(IO_LIB ImageIO REQUIRED)
  find_library(CS_LIB CoreServices REQUIRED)
  find_library(CT_LIB CoreText REQUIRED)
  find_library(CG_LIB CoreGraphics REQUIRED)
  find_library(CF_LIB CoreFoundation REQUIRED)
  target_link_libraries(${EXECUTABLE_NAME} PUBLIC ${CF_LIB} ${CT_LIB} ${IO_LIB}
                                                  ${CS_LIB} ${CG_LIB})
endif()

# Dealing with assets We have some non-code resources that our application needs
# in order to work. How we deal with those differs per platform.
if(APPLE)
  # on Apple targets, the application bundle has a "resources" subfolder where
  # we can place our assets. SDL_GetBasePath() gives us easy access to that
  # location.
  set(input_root "${CMAKE_CURRENT_LIST_DIR}/src")
  macro(add_resource FILE)
    file(RELATIVE_PATH relpath "${input_root}" "${FILE}")
    get_filename_component(relpath "${relpath}" DIRECTORY)
    target_sources(${EXECUTABLE_NAME} PRIVATE ${FILE})
    set_property(SOURCE ${FILE} PROPERTY MACOSX_PACKAGE_LOCATION
                                         "Resources/${relpath}")
  endmacro()
  add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Inter-VariableFont.ttf")
  add_resource("${CMAKE_CURRENT_LIST_DIR}/src/the_entertainer.ogg")
  add_resource("${CMAKE_CURRENT_LIST_DIR}/src/gs_tiger.svg")
elseif(EMSCRIPTEN)
  # on the web, we have to put the files inside of the webassembly somewhat
  # unintuitively, this is done via a linker argument.
  target_link_libraries(
    ${EXECUTABLE_NAME}
    PRIVATE
      "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Inter-VariableFont.ttf@/\""
      "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/the_entertainer.ogg@/\""
      "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/gs_tiger.svg@/\"")
else()
  if(ANDROID)
    if(NOT MOBILE_ASSETS_DIR)
      message(
        FATAL_ERROR "Building for Android, but MOBILE_ASSETS_DIR is not set")
    endif()
    file(MAKE_DIRECTORY "${MOBILE_ASSETS_DIR}") # we need to create the project
                                                # Assets dir otherwise CMake
                                                # won't copy our assets.
  endif()

  macro(copy_helper filename)
    if(ANDROID)
      # MOBILE_ASSETS_DIR is set in the gradle file via the cmake command line
      # and points to the Android Studio Assets folder. when we copy assets
      # there, the Android build pipeline knows to add them to the apk.
      set(outname "${MOBILE_ASSETS_DIR}/${filename}")
    else()
      # for platforms that do not have a good packaging format, all we can do is
      # copy the assets to the process working directory.
      set(outname "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${filename}")
    endif()
    add_custom_command(
      POST_BUILD TARGET "${EXECUTABLE_NAME}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_CURRENT_LIST_DIR}/src/${filename}" "${outname}")
  endmacro()
  copy_helper("Inter-VariableFont.ttf")
  copy_helper("the_entertainer.ogg")
  copy_helper("gs_tiger.svg")
endif()

# set some extra configs for each platform
set(RESOURCE_FILES "src/logo.png")
if(IOS)
  set(RESOURCE_FILES "src/iosLaunchScreen.storyboard;src/logo.png") # make the
                                                                    # splash
                                                                    # screen
                                                                    # show up on
                                                                    # iOS
endif()

set_target_properties(
  ${EXECUTABLE_NAME}
  PROPERTIES # On macOS, make a proper .app bundle instead of a bare executable
             MACOSX_BUNDLE TRUE
             # Set the Info.plist file for Apple Mobile platforms. Without this
             # file, your app
             # will not launch.
             MACOSX_BUNDLE_INFO_PLIST
             "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"
             # in Xcode, create a Scheme in the schemes dropdown for the app.
             XCODE_GENERATE_SCHEME TRUE
             # Identification for Xcode
             XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
             XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
             XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
             RESOURCE "${RESOURCE_FILES}")

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# On macOS Platforms, ensure that the bundle is valid for distribution by
# calling fixup_bundle. note that fixup_bundle does not work on iOS, so you will
# want to use static libraries or manually copy dylibs and set rpaths
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  # tell Install about the target, otherwise fixup won't know about the
  # transitive dependencies
  install(
    TARGETS ${EXECUTABLE_NAME}
    BUNDLE DESTINATION ./install COMPONENT Runtime
    RUNTIME DESTINATION ./install/bin COMPONENT Runtime)

  set(DEP_DIR "${CMAKE_BINARY_DIR}") # where to look for dependencies when
                                     # fixing up
  install(
    CODE "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
        ")
  set(CPACK_GENERATOR "DragNDrop")
  include(CPack)
endif()
