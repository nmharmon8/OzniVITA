#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 
#include "vita_socket.cpp"  // Include your existing C++ code

namespace py = pybind11;

PYBIND11_MODULE(vita_socket_py, m) {
    m.doc() = "Python bindings for Vita Socket using pybind11";

    py::class_<VitaStream>(m, "VitaStream")
        .def(py::init<int>())
        .def("addPacket", &VitaStream::addPacket)
        .def("getPacketData", &VitaStream::getPacketData)
        .def("getStreamID", &VitaStream::getStreamID)
        .def("getSampleRate", &VitaStream::getSampleRate)
        .def("hasContextPacket", &VitaStream::hasContextPacket);

    m.def("addPacketToStream", &addPacketToStream);
    m.def("getStreamIDs", &getStreamIDs);
    m.def("getStream", &getStream, py::return_value_policy::reference);
    m.def("run_tcp", &run_tcp, pybind11::arg("host"), pybind11::arg("port"),
          "A function that sets up and starts the server on the given host and port.");
    m.def("run_udp", &run_udp, pybind11::arg("host"), pybind11::arg("port"),
          "A function that sets up and starts the server on the given host and port.");
    m.def("stop_vita_socket", &stop_vita_socket);
}

