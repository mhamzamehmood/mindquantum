//   Copyright 2023 <Huawei Technologies Co., Ltd>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#ifndef MATH_TENSOR_TRAITS_HPP_
#define MATH_TENSOR_TRAITS_HPP_

#include <complex>
#include <stdexcept>
#include <string>
#include <type_traits>
namespace tensor {
enum class TDevice : int {
    CPU,
    GPU,
};

enum class TDtype : int {
    Float32,
    Float64,
    Complex64,
    Complex128,
};

// -----------------------------------------------------------------------------

template <TDtype dtype>
struct to_device;

template <>
struct to_device<TDtype::Float32> {
    using type = float;
};

template <>
struct to_device<TDtype::Float64> {
    using type = double;
};

template <>
struct to_device<TDtype::Complex64> {
    using type = std::complex<float>;
};

template <>
struct to_device<TDtype::Complex128> {
    using type = std::complex<double>;
};

template <typename T>
struct to_dtype;

template <>
struct to_dtype<float> {
    static constexpr auto dtype = TDtype::Float32;
};

template <>
struct to_dtype<double> {
    static constexpr auto dtype = TDtype::Float64;
};

template <>
struct to_dtype<std::complex<float>> {
    static constexpr auto dtype = TDtype::Complex64;
};

template <>
struct to_dtype<std::complex<double>> {
    static constexpr auto dtype = TDtype::Complex128;
};

template <typename T>
struct is_arithmetic {
    static constexpr bool v = false;
};

template <>
struct is_arithmetic<float> {
    static constexpr bool v = true;
};

template <>
struct is_arithmetic<double> {
    static constexpr bool v = true;
};

template <>
struct is_arithmetic<std::complex<float>> {
    static constexpr bool v = true;
};
template <>
struct is_arithmetic<std::complex<double>> {
    static constexpr bool v = true;
};

// -----------------------------------------------------------------------------

template <typename T>
static constexpr bool is_arithmetic_v = is_arithmetic<T>::v;

template <TDtype dtype>
using to_device_t = typename to_device<dtype>::type;

template <typename T>
static constexpr TDtype to_dtype_v = to_dtype<T>::dtype;
// -----------------------------------------------------------------------------

template <TDtype dtype>
std::string to_string() {
    if constexpr (dtype == TDtype::Float32) {
        return "Float32";
    } else if constexpr (dtype == TDtype::Float64) {
        return "Float64";
    } else if constexpr (dtype == TDtype::Complex64) {
        return "Complex64";
    } else if constexpr (dtype == TDtype::Complex128) {
        return "Complex128";
    } else {
        throw std::runtime_error("Unknown dtype.");
    }
}

std::string to_string(TDtype dtype);

// -----------------------------------------------------------------------------

template <TDevice device>
std::string to_string() {
    if constexpr (device == TDevice::CPU) {
        return "CPU";
    } else if constexpr (device == TDevice::GPU) {
        return "GPU";
    } else {
        throw std::runtime_error("Unknown device.");
    }
}

std::string to_string(TDevice device);

template <typename T>
std::string to_string(const std::complex<T>& a) {
    return "(" + std::to_string(a.real()) + ", " + std::to_string(a.imag()) + ")";
}
}  // namespace tensor
#endif
