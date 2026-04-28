cmake_minimum_required(VERSION 3.15)
project(CPLEX_TSP_Solver C)

# --- 1. CPLEX Configuration (Mac ARM64) ---
# We mirror the paths from your Makefile
set(CPLEX_PATH "/Applications/CPLEX_Studio2211/cplex")
set(CPLEX_INCLUDE_DIR "${CPLEX_PATH}/include")
set(CPLEX_LIB_DIR "${CPLEX_PATH}/lib/arm64_osx/static_pic")

# --- 2. Compiler & Flags ---
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra)

# Setup Build Profiles (Replacing your 'release', 'debug', and 'asan' targets)
# CMake handles -O3 and -g automatically via CMAKE_BUILD_TYPE
if(CMAKE_BUILD_TYPE STREQUAL "ASan")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -O1 -g)
    add_link_options(-fsanitize=address)
endif()

# --- 3. Sources and Directories ---
# This finds all .c files in your src directory recursively
file(GLOB_RECURSE SOURCES "src/*.c")

# --- 4. Define the Executable ---
add_executable(solver ${SOURCES})

# --- 5. Include Paths ---
# 'include' is for your headers, the other is for cplex.h
target_include_directories(solver PRIVATE 
    include 
    ${CPLEX_INCLUDE_DIR}
    "${CPLEX_INCLUDE_DIR}/ilcplex"
)

# --- 6. Link Libraries ---
# Find the specific cplex library file
find_library(CPLEX_LIB cplex PATHS ${CPLEX_LIB_DIR} NO_DEFAULT_PATH)

# Link everything together
target_link_libraries(solver PRIVATE 
    ${CPLEX_LIB} 
    pthread 
    m 
    dl
)

# --- 7. Output Directory ---
set_target_properties(solver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")