cmake_minimum_required(VERSION 3.21)

project(Kit
    VERSION 1.0.0
    DESCRIPTION "A little fox that lives on your macOS desktop"
    LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KIT_BUILD_TESTS "Build unit tests" ON)

# --------------------------------------------------------------------------
# Core: physics + behaviour. Plain C++20, no Qt, no OS -> unit-tested.
# --------------------------------------------------------------------------
add_library(kit_core STATIC
    src/core/World.h
    src/core/World.cpp
    src/core/PetSim.h
    src/core/PetSim.cpp
)
target_include_directories(kit_core PUBLIC src/core)

if(KIT_BUILD_TESTS)
    enable_testing()
    add_executable(tst_core tests/tst_core.cpp)
    target_link_libraries(tst_core PRIVATE kit_core)
    add_test(NAME core COMMAND tst_core)
endif()

# --------------------------------------------------------------------------
# Application
# --------------------------------------------------------------------------
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets Quick)
qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(Kit MACOSX_BUNDLE
    src/main.cpp
    src/app/Pet.h
    src/app/Pet.cpp
    src/app/Tray.h
    src/app/Tray.cpp
    src/platform/Native.h
)
target_include_directories(Kit PRIVATE src/app src/platform)
target_compile_definitions(Kit PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")

if(APPLE)
    target_sources(Kit PRIVATE src/platform/Native_mac.mm)
    set_source_files_properties(src/platform/Native_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(Kit PRIVATE
        "-framework AppKit"
        "-framework CoreGraphics"
        "-framework ApplicationServices"
        "-framework ServiceManagement")
else()
    target_sources(Kit PRIVATE src/platform/Platform_stub.cpp)
endif()

set(KIT_QML_FILES
    qml/Main.qml
    qml/Colors.qml
    qml/Blob.qml
    qml/Stroke.qml
    qml/Leg.qml
    qml/Tail.qml
    qml/Head.qml
    qml/Fox.qml
    qml/Particles.qml
    qml/DizzyStars.qml
)
foreach(qml_file IN LISTS KIT_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    # Interpreted/JIT-compiled instead of AOT C++: the rig is small, and it
    # keeps the build independent of qmlcachegen quirks between Qt versions.
    set_source_files_properties("${qml_file}" PROPERTIES
        QT_RESOURCE_ALIAS "${qml_name}"
        QT_QML_SKIP_CACHEGEN TRUE)
endforeach()
set_source_files_properties(qml/Colors.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Kit
    URI Kit
    VERSION 1.0
    QML_FILES ${KIT_QML_FILES}
)

target_link_libraries(Kit PRIVATE kit_core Qt6::Gui Qt6::Widgets Qt6::Quick)

set_target_properties(Kit PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.kit.Kit"
    MACOSX_BUNDLE_BUNDLE_NAME "Kit"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# resources/AppIcon.icns is generated from AppIcon.png by scripts/package.sh.
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
    set(app_icon "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
    set_source_files_properties(${app_icon} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(Kit PRIVATE ${app_icon})
    set_target_properties(Kit PROPERTIES MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
endif()
