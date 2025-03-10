#pragma once

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <bezman/src/bezier_spline.hpp>
#include <bezman/src/point.hpp>
#include <type_traits>

namespace py = pybind11;

template<std::size_t para_dim, std::size_t dim>
using Bezier = bezman::BezierSpline<
    static_cast<std::size_t>(para_dim),
    std::conditional_t<(dim > 1), bezman::Point<dim>, double>,
    double>;

template<std::size_t para_dim, std::size_t dim>
class PyRationalBezier;

template<std::size_t para_dim, std::size_t dim>
class PyBezier {
private:
  // Alias to the internal Bezier type
  using BezierSpline_ = Bezier<para_dim, dim>;

public:
  int para_dim_ = para_dim;
  int dim_ = dim;

  // python arrays
  py::array_t<int> p_degrees;
  py::array_t<double> p_control_points;

  bool skip_update = false;

  Bezier<para_dim, dim> c_bezier;

  const std::string whatami =
      "Bezier, parametric dimension: " + std::to_string(para_dim)
      + ", physical dimension: " + std::to_string(dim);

  PyBezier() = default;
  PyBezier(py::array_t<int> degrees, py::array_t<double> control_points)
      : p_degrees(degrees),
        p_control_points(control_points) {
    update_c();
  }

  PyBezier(BezierSpline_ rhs) {
    // Init c_bezier using move constructor
    c_bezier = rhs;
    p_control_points.resize({(int) c_bezier.control_points.size(), (int) dim});
    p_degrees.resize({(int) para_dim});
    update_p();
  }

  ~PyBezier() = default;

  void update_c() {
    std::array<std::size_t, para_dim> c_degrees{};

    // update degrees
    const int* degree_ptr = static_cast<int*>(p_degrees.request().ptr);

    for (std::size_t i = 0; i < para_dim; i++) {
      c_degrees[i] = degree_ptr[i];
    }

    // (re)init
    c_bezier = Bezier<para_dim, dim>{c_degrees};

    // update cps
    const double* ctps_ptr =
        static_cast<double*>(p_control_points.request().ptr);
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(p_control_points.request().shape[0]);
         i++) {
      if constexpr (dim > 1) {
        for (std::size_t j = 0; j < dim; j++) {
          c_bezier.control_points[i][j] = ctps_ptr[i * dim + j];
        }
      } else {
        c_bezier.control_points[i] = ctps_ptr[i];
      }
    }
  }

  void update_p() {
    // degrees
    int* ds_ptr = static_cast<int*>(p_degrees.request().ptr);
    // control points
    double* cps_ptr = static_cast<double*>(p_control_points.request().ptr);

    // update degrees
    for (std::size_t i = 0; i < para_dim; i++) {
      ds_ptr[i] = c_bezier.GetDegrees()[i];
    }

    // update control_points
    const std::size_t number_of_ctps = c_bezier.control_points.size();
    // Check if shape changed
    if (static_cast<long int>(number_of_ctps)
        != p_control_points.request().shape[0]) {
      // Update Control Point Vector
      p_control_points = py::array_t<double>(number_of_ctps * dim);
      p_control_points.resize({(int) number_of_ctps, (int) dim});
      // Update pointers
      cps_ptr = static_cast<double*>(p_control_points.request().ptr);
    }

    // Update point coordinates
    for (std::size_t i = 0; i < number_of_ctps; i++) {
      if constexpr (dim > 1) {
        for (std::size_t j = 0; j < dim; j++) {
          cps_ptr[i * dim + j] = c_bezier.control_points[i][j];
        }
      } else {
        cps_ptr[i] = c_bezier.control_points[i];
      }
    }
  }

  void update_unless_skip() {
    if (!skip_update) {
      update_c();
    }
  }

  py::array_t<double> evaluate(py::array_t<double> queries) {
    update_unless_skip();

    py::buffer_info q_buf = queries.request();
    double* query_ptr = static_cast<double*>(q_buf.ptr);

    // prepare result arr
    py::array_t<double> results(q_buf.shape[0] * dim);
    py::buffer_info r_buf = results.request();
    double* result_ptr = static_cast<double*>(r_buf.ptr);

    for (int i = 0; i < q_buf.shape[0]; i++) {
      bezman::Point<static_cast<unsigned>(para_dim)> qpt; // query
      for (std::size_t j = 0; j < para_dim; j++) {
        qpt[j] = query_ptr[i * para_dim + j];
      }
      const auto& eqpt = c_bezier.ForwardEvaluate(qpt); // evaluated query pt
      if constexpr (dim > 1) {
        for (std::size_t j = 0; j < dim; j++) {
          result_ptr[i * dim + j] = eqpt[j];
        }
      } else {
        result_ptr[i * dim] = eqpt;
      }
    }

    results.resize({(int) q_buf.shape[0], (int) dim});

    return results;
  }

  // Derivative.
  py::array_t<double> derivative(py::array_t<double> queries,
                                 py::array_t<int> orders) {
    if (!skip_update) {
      update_c();
    }

    // Extract input arrays info.
    double* query_ptr = static_cast<double*>(queries.request().ptr);
    int* order_ptr = static_cast<int*>(orders.request().ptr);

    // Init results array.
    std::size_t num_queries =
        static_cast<std::size_t>(queries.request().shape[0]);
    py::array_t<double> results(num_queries * dim);
    double* result_ptr = static_cast<double*>(results.request().ptr);
    results.resize({(int) num_queries, (int) dim});

    // transform order format
    std::array<std::size_t, para_dim> derivative{};
    assert(para_dim == orders.request().shape[0]);
    for (std::size_t i_para_dim{}; i_para_dim < para_dim; i_para_dim++) {
      derivative[i_para_dim] = order_ptr[i_para_dim];
    }

    // Loop - Queries.
    for (std::size_t i_query{}; i_query < num_queries; i_query++) {
      bezman::Point<para_dim, double> pc{};
      for (std::size_t i_para_dim = 0; i_para_dim < para_dim; i_para_dim++) {
        pc[i_para_dim] = query_ptr[i_query * para_dim + i_para_dim];
      }
      // Evaluate derivate
      const auto c_result = c_bezier.EvaluateDerivative(pc, derivative);

      // Write `c_result` to `results`.
      if constexpr (dim == 1) {
        result_ptr[i_query] = c_result;
      } else {
        for (std::size_t i_dim{}; i_dim < dim; ++i_dim) {
          result_ptr[i_query * dim + i_dim] = c_result[i_dim];
        }
      }
    }

    return results;
  }

  py::array_t<double> pseudorecursive_evaluate(py::array_t<double> queries) {
    update_unless_skip();

    py::buffer_info q_buf = queries.request();
    double* query_ptr = static_cast<double*>(q_buf.ptr);

    // prepare result arr
    py::array_t<double> results(q_buf.shape[0] * dim);
    py::buffer_info r_buf = results.request();
    double* result_ptr = static_cast<double*>(r_buf.ptr);

    for (std::size_t i = 0; i < static_cast<std::size_t>(q_buf.shape[0]); i++) {
      bezman::Point<static_cast<unsigned>(para_dim)> qpt; // query
      for (std::size_t j = 0; j < para_dim; j++) {
        qpt[j] = query_ptr[i * para_dim + j];
      }
      const auto eqpt = c_bezier.Evaluate(qpt); // evaluated query pt
      if constexpr (dim > 1) {
        for (std::size_t j = 0; j < dim; j++) {
          result_ptr[i * dim + j] = eqpt[j];
        }
      } else {
        result_ptr[i * dim] = eqpt;
      }
    }

    results.resize({(int) q_buf.shape[0], (int) dim});

    return results;
  }

  void elevate_degree(int p_dim) {
    update_unless_skip();

    c_bezier.OrderElevateAlongParametricDimension(
        static_cast<std::size_t>(p_dim));

    update_p();
  }

  // Multiplication routines
  PyBezier<para_dim, 1> multiply_with_spline(const PyBezier& a) {
    PyBezier<para_dim, 1> result{(*this).c_bezier * a.c_bezier};
    result.update_p();
    return result;
  }

  // Addition routines
  PyBezier add_spline(const PyBezier& a) {
    PyBezier result{(*this).c_bezier + a.c_bezier};
    result.update_p();
    return result;
  }

  PyBezier multiply_with_scalar_spline(const PyBezier<para_dim, 1>& a) {
    PyBezier result{(*this).c_bezier * a.c_bezier};
    result.update_p();
    return result;
  }

  // Composition Routine
  template<int par_dim_inner_function>
  PyBezier<par_dim_inner_function, dim>
  ComposePP(const PyBezier<par_dim_inner_function, para_dim>& inner_function) {
    // Use Composition routine
    PyBezier<par_dim_inner_function, dim> result{
        (*this).c_bezier.Compose(inner_function.c_bezier)};
    result.update_p();
    return result;
  }

  // Composition Routine with Rational Spline
  template<int par_dim_inner_function>
  PyRationalBezier<par_dim_inner_function, dim>
  ComposePR(const PyRationalBezier<par_dim_inner_function, para_dim>&
                inner_function) {
    // Use Composition routine
    PyRationalBezier<par_dim_inner_function, dim> result{
        (*this).c_bezier.Compose(inner_function.c_rational_bezier)};
    result.update_p();
    return result;
  }
};

template<int para_dim, int dim>
void add_bezier_pyclass(py::module& m, const char* class_name) {
  py::class_<PyBezier<para_dim, dim>> klasse(m, class_name);

  klasse.def(py::init<>())
      .def(py::init<py::array_t<int>, py::array_t<double>>(),
           py::arg("degrees"),
           py::arg("control_points"))
      .def_readonly("whatami", &PyBezier<para_dim, dim>::whatami)
      .def_readonly("dim", &PyBezier<para_dim, dim>::dim_)
      .def_readonly("para_dim", &PyBezier<para_dim, dim>::para_dim_)
      .def_readwrite("skip_update", &PyBezier<para_dim, dim>::skip_update)
      .def_readwrite("degrees", &PyBezier<para_dim, dim>::p_degrees)
      .def_readwrite("control_points",
                     &PyBezier<para_dim, dim>::p_control_points)
      .def("update_c", &PyBezier<para_dim, dim>::update_c)
      .def("update_p", &PyBezier<para_dim, dim>::update_p)
      .def("evaluate", &PyBezier<para_dim, dim>::evaluate, py::arg("queries"))
      .def("recursive_evaluate",
           &PyBezier<para_dim, dim>::pseudorecursive_evaluate,
           py::arg("queries"))
      .def("elevate_degree",
           &PyBezier<para_dim, dim>::elevate_degree,
           py::arg("p_dim"))
      .def("multiply_with_spline",
           &PyBezier<para_dim, dim>::multiply_with_spline,
           py::arg("factor"))
      .def("multiply_with_scalar_spline",
           &PyBezier<para_dim, dim>::multiply_with_scalar_spline,
           py::arg("factor"))
      .def("add_spline",
           &PyBezier<para_dim, dim>::add_spline,
           py::arg("summand"))
      .def("compose_line_pp",
           &PyBezier<para_dim, dim>::template ComposePP<1>,
           py::arg("inner_function"))
      .def("compose_surface_pp",
           &PyBezier<para_dim, dim>::template ComposePP<2>,
           py::arg("inner_function"))
      .def("compose_volume_pp",
           &PyBezier<para_dim, dim>::template ComposePP<3>,
           py::arg("inner_function"))
      .def("compose_line_pr",
           &PyBezier<para_dim, dim>::template ComposePR<1>,
           py::arg("inner_function"))
      .def("compose_surface_pr",
           &PyBezier<para_dim, dim>::template ComposePR<2>,
           py::arg("inner_function"))
      .def("derivative",
           &PyBezier<para_dim, dim>::derivative,
           py::arg("queries"),
           py::arg("orders"),
           py::return_value_policy::move)
      .def("compose_volume_pr",
           &PyBezier<para_dim, dim>::template ComposePR<3>,
           py::arg("inner_function"))
      .def(py::pickle(
          [](const PyBezier<para_dim, dim>& bezier) {
            return py::make_tuple(bezier.p_degrees, bezier.p_control_points);
          },
          [](py::tuple t) {
            if (t.size() != 2) {
              throw std::runtime_error("Invalid PyBezier state!");
            }

            PyBezier<para_dim, dim> pyb(t[0].cast<py::array_t<int>>(),
                                        t[1].cast<py::array_t<double>>());

            return pyb;
          }));
}
