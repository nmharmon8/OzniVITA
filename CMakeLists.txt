cmake_minimum_required(VERSION 3.4...3.18)
project(vita_socket)

# Include the submodules
add_subdirectory(libs/libvrt)
add_subdirectory(external/pybind11)

# Create the module target
pybind11_add_module(vita_socket pybind_vita.cpp)

# Specify include directories for vita_socket
target_include_directories(vita_socket PRIVATE libs/libvrt/include)

# Link libraries
target_link_libraries(vita_socket PRIVATE vrt pthread)

# Use Python to find the site-packages directory
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -c
            "from distutils.sysconfig import get_python_lib; print(get_python_lib())"
    OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Installing Python module to ${PYTHON_SITE_PACKAGES}")
install(TARGETS vita_socket LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES})
