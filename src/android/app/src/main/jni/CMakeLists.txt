add_library(citra-android SHARED
    android_common/android_common.cpp
    android_common/android_common.h
    applets/mii_selector.cpp
    applets/mii_selector.h
    applets/swkbd.cpp
    applets/swkbd.h
    input_manager.cpp
    input_manager.h
    camera/ndk_camera.cpp
    camera/ndk_camera.h
    camera/still_image_camera.cpp
    camera/still_image_camera.h
    cheats/cheat.cpp
    cheats/cheat.h
    cheats/cheat_engine.cpp
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window.cpp
    emu_window/emu_window.h
    emu_window/emu_window_gl.cpp
    emu_window/emu_window_gl.h
    emu_window/emu_window_vk.cpp
    emu_window/emu_window_vk.h
    game_info.cpp
    game_settings.cpp
    game_settings.h
    id_cache.cpp
    id_cache.h
    native.cpp
    native.h
    ndk_motion.cpp
    ndk_motion.h
    vr/CursorLayer.cpp
    vr/vr_main.cpp
    vr/OpenXR.cpp
    vr/gl/Egl.cpp
    vr/GameSurfaceLayer.cpp
    vr/XrController.cpp
    vr/utils/JniUtils.cpp
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-enum-enum-conversion")
target_link_libraries(citra-android PRIVATE audio_core citra_common citra_core input_common network adrenotools)
target_link_libraries(citra-android PRIVATE openxr_loader GLESv3 android
        camera2ndk EGL glad inih jnigraphics lodepng log mediandk yuv)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} citra-android)
