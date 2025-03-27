cmake_minimum_required(VERSION 3.4...3.18)
project(rh_file_parser)

# Include the submodules
# add_subdirectory(libs/libvrt)
add_subdirectory(external/pybind11)

# Create the module target
pybind11_add_module(rh_file_parser pybind_vita.cpp)

# Specify include directories for vita_socket
target_include_directories(rh_file_parser PRIVATE /usr/local/include)

# Link libraries
target_link_libraries(rh_file_parser PRIVATE VITA49 uuid pthread)


# INCLUDES = -I/usr/local/include/VITA49 
# LDFLAGS = -L/usr/local/lib
# LIBS = -lVITA49 -luuid


# Use Python to find the site-packages directory
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -c
            "from distutils.sysconfig import get_python_lib; print(get_python_lib())"
    OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Installing Python module to ${PYTHON_SITE_PACKAGES}")
install(TARGETS rh_file_parser LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES})
