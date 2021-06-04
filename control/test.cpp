#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h> // everything needed for embedding

namespace py = pybind11;

void say_hello()
{
    // Try to import scipy
    py::object scipy = py::module::import("/home/johannes/Develop/snapcast/control/meta_mpd.py");

    // Equivalent to "from decimal import Decimal"
    py::object Decimal = py::module::import("decimal").attr("Decimal");

    // Construct a Python object of class Decimal
    py::object pi = Decimal("3.14159");
    py::print(py::str(pi));
    py::print(pi);

    py::print("Hello, World!"); // use the Python API
}


int main(int argc, char** argv)
{
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive
    say_hello();
}
