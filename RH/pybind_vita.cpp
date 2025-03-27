#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/complex.h>
#include "rh_file_parser.cpp"  // Include the VRTParser code

namespace py = pybind11;

PYBIND11_MODULE(rh_file_parser, m) {
    m.doc() = "Python bindings for VRT file parsing using pybind11";

    py::class_<VRTParser>(m, "VRTParser")
        .def(py::init<>())
        .def("parseVRTFile", &VRTParser::parseVRTFile)
        .def("getStreamID", &VRTParser::getStreamID)
        .def("getCenterFrequency", &VRTParser::getCenterFrequency)
        .def("getSampleRate", &VRTParser::getSampleRate)
        .def("getContextPacketCount", &VRTParser::getContextPacketCount)
        .def("getDataPacketCount", &VRTParser::getDataPacketCount)
        .def("getIQData", [](const VRTParser& self) {
            const auto& iqData = self.getIQData();
            return py::array_t<std::complex<float>>(
                {iqData.size()},                          // shape
                {sizeof(std::complex<float>)},            // stride
                iqData.data(),                           // data pointer
                py::cast(self)                           // keep object alive
            );
        });
}


