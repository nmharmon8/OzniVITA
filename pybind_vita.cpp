#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 
#include "vita_socket.cpp"  // Include your existing C++ code

namespace py = pybind11;

PYBIND11_MODULE(vita_socket, m) {
    m.doc() = "Python bindings for Vita Socket using pybind11";

    // py::class_<VitaStream>(m, "VitaStream")
    //     .def(py::init<int>())
    //     .def("addPacket", &VitaStream::addPacket)
    //     .def("getPacketData", &VitaStream::getPacketData)
    //     .def("getStreamID", &VitaStream::getStreamID)
    //     .def("getSampleRate", &VitaStream::getSampleRate)
    //     .def("hasContextPacket", &VitaStream::hasContextPacket)
    //     .def("getFrequency", &VitaStream::getFrequency);

    py::class_<VitaStream>(m, "VitaStream")
        .def(py::init<int, size_t>()) // Assuming you want to expose the max_packets parameter to Python as well
        .def("addPacket", &VitaStream::addPacket)
        .def("getPacketData", &VitaStream::getPacketData)
        .def("getStreamID", &VitaStream::getStreamID)
        .def("getSampleRate", &VitaStream::getSampleRate) // No change needed here
        .def("hasContextPacket", &VitaStream::hasContextPacket)
        .def("getFrequency", &VitaStream::getFrequency);


    py::class_<VitaSocket>(m, "VitaSocket")
        .def(py::init<int, bool>())
        .def("stop_vita_socket", &VitaSocket::stop_vita_socket)
        .def("getStreamIDs", &VitaSocket::getStreamIDs)
        .def("getStream", &VitaSocket::getStream, py::return_value_policy::reference)
        .def("join", &VitaSocket::join)
        .def("run_tcp", &VitaSocket::run_tcp, py::arg("host"), py::arg("port"))
        .def("run_udp", &VitaSocket::run_udp, py::arg("host"), py::arg("port"))
        .def("run_multicast", &VitaSocket::run_multicast, py::arg("host"), py::arg("port"));

    // m.def("addPacketToStream", &addPacketToStream);
    // m.def("getStreamIDs", &getStreamIDs);
    // m.def("getStream", &getStream, py::return_value_policy::reference);
    // m.def("run_tcp", &run_tcp, pybind11::arg("host"), pybind11::arg("port"),
    //       "A function that sets up and starts the server on the given host and port.");
    // m.def("run_udp", &run_udp, pybind11::arg("host"), pybind11::arg("port"),
    //       "A function that sets up and starts the server on the given host and port.");
    // m.def("stop_vita_socket", &stop_vita_socket);
}

