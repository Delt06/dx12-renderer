cmake_minimum_required(VERSION 3.8.0)

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Make sure DLL and EXE targets go to the same directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # Output directory for static lib (.LIB)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for shared lib (.DLL)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for executables (.EXE)

project(LearningDirectX12 LANGUAGES CXX)

# Libraries
add_subdirectory(DX12Library)
add_subdirectory(Framework)

# Demos
add_subdirectory(Demos/AnimationsDemo)
add_subdirectory(Demos/DeferredLightingDemo)
add_subdirectory(Demos/LightingDemo)
add_subdirectory(Demos/ToonDemo)
add_subdirectory(Demos/GrassDemo)
add_subdirectory(Demos/RenderGraphDemo)

# Set the startup project.
set_directory_properties(PROPERTIES
        VS_STARTUP_PROJECT LightingDemo
        )