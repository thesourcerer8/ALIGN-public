#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;

#include "PnRdatabase.h"

using namespace PnRDB;
using std::string;

PYBIND11_MODULE(PnRdatabase, m) {
  m.doc() = "pybind11 plugin for PnRdatabase";

  py::class_<PnRdatabase>( m, "PnRdatabase")
    .def( py::init<string, string, string, string, string, string>())
    .def( py::init<>())
    .def( "TraverseHierTree", &PnRdatabase::TraverseHierTree)
    .def( "CheckoutHierNode", &PnRdatabase::CheckoutHierNode);
};
