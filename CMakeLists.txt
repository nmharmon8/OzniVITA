cmake_minimum_required(VERSION 3.4...3.18)
project(vita_socket_py)

# Include the submodules
add_subdirectory(external/libvrt)
add_subdirectory(external/pybind11)

# Assuming pybind11 is correctly set up by add_subdirectory
# find_package(pybind11 REQUIRED) might not be necessary

pybind11_add_module(vita_socket_py pybind_vita.cpp)
target_link_libraries(vita_socket_py PRIVATE vrt pthread)

# Use Python to find the site-packages directory
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -c
            "from distutils.sysconfig import get_python_lib; print(get_python_lib())"
    OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Installing Python module to ${PYTHON_SITE_PACKAGES}")
install(TARGETS vita_socket_py LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES})
