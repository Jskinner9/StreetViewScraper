cmake_minimum_required(VERSION 3.10)
project(StreetViewDownloader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add definitions to silence warnings
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-D_SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING)

# Find required packages
find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED)

# Find TBB
find_package(TBB CONFIG)
if(TBB_FOUND)
    add_definitions(-DUSE_TBB)
    message(STATUS "Found TBB: ${TBB_DIR}")
else()
    message(STATUS "TBB not found, using standard C++ threading")
endif()

# Find OpenMP
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# Add optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

# Add source files
add_executable(streetview_downloader streetview_downloader.cpp)

# Include directories
target_include_directories(streetview_downloader PRIVATE 
    ${CURL_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# Prepare libraries list
set(LINKED_LIBS ${CURL_LIBRARIES} ${OpenCV_LIBS})

# Add TBB if found
if(TBB_FOUND)
    list(APPEND LINKED_LIBS TBB::tbb)
endif()

# Add OpenMP if found
if(OpenMP_CXX_FOUND)
    list(APPEND LINKED_LIBS OpenMP::OpenMP_CXX)
endif()

# Link all libraries at once
target_link_libraries(streetview_downloader ${LINKED_LIBS})