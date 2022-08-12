//   Copyright 2022 <Huawei Technologies Co., Ltd>
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

#include "ops/gates/fermion_operator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>

#include <unsupported/Eigen/KroneckerProduct>
#include <unsupported/Eigen/src/KroneckerProduct/KroneckerTensorProduct.h>

#include <tweedledum/Operators/Standard/X.h>
#include <tweedledum/Operators/Standard/Y.h>
#include <tweedledum/Operators/Standard/Z.h>

#include <fmt/format.h>
#include <lru_cache/lru_cache.h>

#include "core/logging.hpp"
#include "core/parser/boost_x3_error_handler.hpp"
#include "details/boost_x3_complex_number.hpp"
#include "details/boost_x3_get_info_impl.hpp"
#include "details/boost_x3_parse_object.hpp"
#include "details/eigen_diagonal_identity.hpp"
#include "details/fmt_std_complex.hpp"
#include "ops/gates.hpp"
#include "ops/gates/terms_operator.hpp"

// -----------------------------------------------------------------------------

#if MQ_HAS_CXX20_SPAN
#    include <span>
namespace mindquantum::compat {
using std::span;
}  // namespace mindquantum::compat
#else
#    include <boost/core/span.hpp>
namespace mindquantum::compat {
using boost::span;
}  // namespace mindquantum::compat
#endif  // MQ_HAS_CXX20_SPAN

#if MQ_HAS_ABSEIL_CPP
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define DECLARE_MEMOIZE_CACHE(name, cache_size, function)                                                          \
        static auto name = lru_cache::node::memoize_function(cache_size, function)
#else
#    include "details/cache_impl.hpp"
   // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define DECLARE_MEMOIZE_CACHE(name, cache_size, function)                                                          \
        static auto name = lru_cache::staticc::memoize_function<cache_size>(function)
#endif  // MQ_HAS_ABSEIL_CPP

// -----------------------------------------------------------------------------

using namespace std::literals::string_literals;  // NOLINT(build/namespaces_literals)

// =============================================================================

namespace x3 = boost::spirit::x3;

namespace ast::fm_op {
using mindquantum::ops::TermValue;
using term_t = mindquantum::ops::FermionOperator::term_t;
using terms_t = mindquantum::ops::FermionOperator::terms_t;
using complex_term_dict_t = mindquantum::ops::FermionOperator::complex_term_dict_t;
using term_coeff_t = std::pair<mindquantum::ops::FermionOperator::complex_term_dict_t::key_type,
                               mindquantum::ops::FermionOperator::complex_term_dict_t::mapped_type>;

struct TermOp : x3::symbols<TermValue> {
    TermOp() {
        add("^", TermValue::adg);
        add("v", TermValue::a);
    }
} const term_op;
}  // namespace ast::fm_op

// -----------------------------------------------------------------------------

namespace parser::fm_op {
namespace ast = ::ast::fm_op;
using mindquantum::parser::complex;

struct term_class : mindquantum::parser::x3::rule::error_handler {};
const x3::rule<term_class, ast::term_t> term = "FermionOperator term (ie. [0-9]+^?)";
static const auto term_def = x3::lexeme[x3::uint_ > (ast::term_op | x3::attr(ast::TermValue::a))];

struct terms_class : mindquantum::parser::x3::rule::error_handler {};
const x3::rule<terms_class, ast::terms_t> terms = "FermionOperator terms list";
static const auto terms_def = +term;

// -------------------------------------

struct json_value_class : mindquantum::parser::x3::rule::error_handler {};
const x3::rule<json_value_class, ast::term_coeff_t> json_value_term
    = R"s(FermionOperator JSON key-value pair ("<term-list>": "<complex-num>"))s";
static const auto json_value_term_def = x3::expect[x3::lit('"')]
                                        >> (('"' >> x3::attr(ast::terms_t{})) | (x3::expect[+term] >> '"')) > ':'
                                        > x3::lit('"') > complex > '"';
struct json_dict_class : mindquantum::parser::x3::rule::error_handler {};
const x3::rule<json_dict_class, ast::complex_term_dict_t> json_dict = "JSON representation of a FermionOperator";
static const auto json_dict_def = x3::expect['{'] > (json_value_term % ',') > '}';

// -------------------------------------

BOOST_SPIRIT_DEFINE(term, terms, json_value_term, json_dict);
}  // namespace parser::fm_op

// -------------------------------------

namespace boost::spirit::x3 {
template <>
struct get_info<ast::fm_op::TermOp> {
    using result_type = std::string;
    result_type operator()(const ast::fm_op::TermOp& /* type */) const noexcept {
        using std::literals::string_literals::operator""s;
        return "ladder operator (a: '' or 'v', adg: '^')"s;
    }
};
}  // namespace boost::spirit::x3

// =============================================================================

namespace {
constexpr auto cache_size = 100UL;
using csr_matrix_t = mindquantum::ops::FermionOperator::csr_matrix_t;

auto n_identity(std::size_t n) -> csr_matrix_t {
    using scalar_t = csr_matrix_t::Scalar;
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MQ_CASE_FOR_NQUBITS(n)                                                                                         \
    case n:                                                                                                            \
        return csr_matrix_t{::generate_eigen_diagonal<scalar_t, 1U << (n)>()};                                         \
        break

    switch (n) {
        MQ_CASE_FOR_NQUBITS(0U);
        MQ_CASE_FOR_NQUBITS(1U);
        MQ_CASE_FOR_NQUBITS(2U);
        MQ_CASE_FOR_NQUBITS(3U);
        MQ_CASE_FOR_NQUBITS(4U);
        MQ_CASE_FOR_NQUBITS(5U);
        MQ_CASE_FOR_NQUBITS(6U);
        default:
            auto tmp = Eigen::DiagonalMatrix<csr_matrix_t::Scalar, Eigen::Dynamic>(1U << n);
            tmp.setIdentity();
            return csr_matrix_t{tmp};
            break;
    }
#undef MQ_CASE_FOR_NQUBITS
}

// =============================================================================

auto n_sz_impl(std::size_t n) -> csr_matrix_t {
    using scalar_t = csr_matrix_t::Scalar;

    if (n == 0) {
        return n_identity(0);
    }

    auto result = csr_matrix_t{2, 2};
    result.insert(0, 0) = 1;
    result.insert(1, 1) = -1;
    const auto tmp = result;

    --n;
    for (auto i(0UL); i < n; ++i) {
        result = Eigen::kroneckerProduct(result, tmp).eval();
    }

    return result;
}

// -----------------------------------------------------------------------------

auto n_sz(std::size_t n) {
    DECLARE_MEMOIZE_CACHE(cache_, cache_size, n_sz_impl);
    return cache_(n);
}

// =============================================================================

auto single_fermion_word_impl(const std::tuple<std::size_t, bool, std::size_t>& data) -> csr_matrix_t {
    const auto& [idx, is_adg, n_qubits] = data;
    auto result = csr_matrix_t{2, 2};
    if (is_adg) {
        result.insert(0, 1) = 1;
    } else {
        result.insert(1, 0) = 1;
    }
    return Eigen::kroneckerProduct(n_identity(n_qubits - 1 - idx), Eigen::kroneckerProduct(result, n_sz(idx)).eval())
        .eval();
}

// -----------------------------------------------------------------------------

auto single_fermion_word(std::size_t idx, bool is_adg, std::size_t n_qubits) {
    DECLARE_MEMOIZE_CACHE(cache_, cache_size, single_fermion_word_impl);
    return cache_(std::make_tuple(idx, is_adg, n_qubits));
}

// =============================================================================

auto two_fermion_word_impl(const std::tuple<std::size_t, bool, std::size_t, bool, std::size_t>& data) -> csr_matrix_t {
    const auto& [idx1, is_adg1, idx2, is_adg2, n_qubits] = data;
    return single_fermion_word(idx1, is_adg1, n_qubits) * single_fermion_word(idx2, is_adg2, n_qubits);
}

// -----------------------------------------------------------------------------

auto two_fermion_word(std::size_t idx1, bool is_adg1, std::size_t idx2, bool is_adg2, std::size_t n_qubits) {
    DECLARE_MEMOIZE_CACHE(cache_, cache_size, two_fermion_word_impl);
    return cache_(std::make_tuple(idx1, is_adg1, idx2, is_adg2, n_qubits));
}
// =============================================================================
}  // namespace

namespace mindquantum::ops {

FermionOperator::FermionOperator(std::string_view terms_string, coefficient_t coeff)
    : TermsOperator(parse_string_(terms_string), coeff) {
}

// =============================================================================

auto FermionOperator::matrix(std::optional<uint32_t> n_qubits) const -> std::optional<csr_matrix_t> {
    if (std::empty(terms_)) {
        return std::nullopt;
    }

    // NB: this is assuming that num_targets_ is up-to-date
    const auto& n_qubits_local = num_targets_;

    if (n_qubits_local == 0UL && !n_qubits) {
        std::cerr << "You should specify n_qubits for converting an identity qubit operator.";
        return std::nullopt;
    }

    if (n_qubits && n_qubits.value() < n_qubits_local) {
        std::cerr << fmt::format(
            "Given n_qubits {} is smaller than the number of qubits of this qubit operator, which is {}.\n",
            n_qubits.value(), n_qubits_local);
        return std::nullopt;
    }

    const auto n_qubits_value = n_qubits.value_or(n_qubits_local);

    const auto process_term = [n_qubits_value](const auto& local_ops) -> csr_matrix_t {
        if (std::empty(local_ops)) {
            return (Eigen::Matrix<std::complex<double>, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>::Identity(
                        1U << n_qubits_value, 1U << n_qubits_value))
                .sparseView();
        }

        constexpr auto size_groups = 2UL;
        std::vector<compat::span<const term_t>> groups;
        groups.reserve(std::size(local_ops) / size_groups + 1);

        const auto local_ops_end = end(local_ops);
        auto num_to_copy = size_groups;
        for (auto it(begin(local_ops)); it != local_ops_end; std::advance(it, num_to_copy)) {
            num_to_copy = std::min(static_cast<decltype(size_groups)>(std::distance(it, local_ops_end)), size_groups);
            groups.emplace_back(&*it, num_to_copy);
        }

        auto process_group = [n_qubits_value](const auto& group) constexpr {
            if (std::size(group) == 2) {
                return ::two_fermion_word(group[0].first, group[0].second == TermValue::adg, group[1].first,
                                          group[1].second == TermValue::adg, n_qubits_value);
            }
            return ::single_fermion_word(group[0].first, group[0].second == TermValue::adg, n_qubits_value);
        };

        assert(!std::empty(groups));
        auto tmp = process_group(groups.front());

        for (auto it(begin(groups) + 1); it != end(groups); ++it) {
            tmp = tmp * process_group(*it);
        }

        return tmp;
    };

    auto it = begin(terms_);
    auto result = (process_term(it->first) * it->second).eval();
    ++it;
    for (; it != end(terms_); ++it) {
        result = result * process_term(it->first) * it->second;
    }

    return result;
}

// =============================================================================

auto FermionOperator::split() const noexcept -> std::vector<FermionOperator> {
    std::vector<FermionOperator> result;
    for (const auto& [local_ops, coeff] : terms_) {
        result.emplace_back(local_ops, coeff);
    }
    return result;
}

// -----------------------------------------------------------------------------

// auto FermionOperator::normal_ordered() const -> FermionOperator {
//     auto ordered_op{*this};
//     for (const auto& [local_ops, coeff] : terms_) {
//         ordered_op += normal_ordered_term_(local_ops, coeff);
//     }
//     return ordered_op;
// }

// =============================================================================

namespace {
struct term_to_string {
    template <bool is_first_elem = false>
    static auto apply(const FermionOperator::term_t& term) {
        if constexpr (is_first_elem) {
            return fmt::format("{}{}", std::get<0>(term), std::get<1>(term) == TermValue::adg ? "^"s : "");
        } else {
            return fmt::format(" {}{}", std::get<0>(term), std::get<1>(term) == TermValue::adg ? "^"s : "");
        }
    }
};
}  // namespace

// -----------------------------------------------------------------------------

auto FermionOperator::to_string() const noexcept -> std::string {
#if MQ_STD_ACCUMULATE_USE_MOVE
    using acc_init_t = std::string&&;
#else
    using acc_init_t = std::string&;
#endif  // __cplusplus >= 202002L

    if (std::empty(terms_)) {
        return "0"s;
    }

    const auto process_term = [](const complex_term_dict_t::value_type& term_value,
                                 bool prepend_newline = false) -> std::string {
        const auto& [local_ops, coeff] = term_value;
        auto term_str = fmt::format("{}{} [", prepend_newline ? "\n" : "", coeff);
        if (std::empty(local_ops)) {
            term_str += ']';
        } else {
            const auto it = begin(local_ops);
            term_str += std::accumulate(begin(local_ops) + 1, end(local_ops), term_to_string::apply<true>(*it),
                                        [](acc_init_t init, const auto& term) -> decltype(auto) {
                                            return init += term_to_string::apply<false>(term);
                                        });
            term_str += ']';
        }
        return term_str;
    };

    return std::accumulate(++begin(terms_), end(terms_), process_term(*begin(terms_)),
                           [&process_term](acc_init_t init, const auto& term_value) -> decltype(auto) {
                               return init += process_term(term_value, true);
                           });
}

// =============================================================================

auto FermionOperator::dumps(std::size_t indent) const -> std::string {
    std::string result("{\n");
    for (const auto& [local_ops, coeff] : terms_) {
        result += std::string(indent, ' ') + '"';
        for (const auto& term : local_ops) {
            result += term_to_string::apply<true>(term) + ' ';
        }
        result += fmt::format(R"(": "{}")", coeff);
    }
    return result += "\n}";
}

// -----------------------------------------------------------------------------

auto FermionOperator::loads(std::string_view string_data) -> std::optional<FermionOperator> {
    if (complex_term_dict_t terms_dict; parser::parse_object_skipper(begin(string_data), end(string_data), terms_dict,
                                                                     ::parser::fm_op::json_dict, x3::space)) {
        return FermionOperator(terms_dict);
    }
    MQ_ERROR("FermionOperator JSON string parsing failed for '{}'", string_data);
    return {};
}

// =============================================================================

auto FermionOperator::normal_ordered_term_(terms_t local_ops, coefficient_t coeff)
    -> std::pair<terms_t, coefficient_t> {
    if (std::empty(local_ops)) {
        return {{}, coeff};
    }
    for (auto it(begin(local_ops) + 1); it != end(local_ops); ++it) {
        for (auto it_jm1(std::make_reverse_iterator(it)), it_j(std::make_reverse_iterator(it) - 1);
             it_jm1 != rend(local_ops); ++it_jm1, ++it_j) {
            // Swap operators if left operator is a and right operator is a^\dagger
            if (it_jm1->second == TermValue::a && it_j->second == TermValue::adg) {
                std::iter_swap(it_jm1, it_j);
                coeff *= -1.;
                // If indice are same, employ the anti-commutation relationship and generate the new term
                if (it_jm1->first == it_j->first) {
                    // NB: we need to skip skip elements j-1 and j. Since it_jm1 and it_j are reverse iterators:
                    //     (it_j + 1).base() is actually the j-1 element
                    //     it_jm1.base() is actually the j+1 element
                    auto new_terms = terms_t(begin(local_ops), (it_j + 1).base());
                    new_terms.reserve(std::size(local_ops) - 1);
                    std::copy(it_jm1.base(), end(local_ops), std::back_inserter(new_terms));
                }
            } else if (it_jm1->second == it_j->second) {
                // If indices are the same, evaluate to zero
                if (it_jm1->first == it_j->first) {
                    // TODO(dnguyen): Should we move? or can (N)RVO take care of that?
                    return {std::move(local_ops), coeff};
                }
                // Swap them if the same operator but lower index on the left
                if (it_jm1->first < it_j->first) {
                    std::iter_swap(it_jm1, it_j);
                    coeff *= -1.;
                }
            }
        }
    }

    // for (auto i(1UL); i < std::size(terms); ++i) {
    //     for (auto j(i); j < i; --i) {
    //         const auto& left_sub_term = terms[j - 1];
    //         const auto& right_sub_term = terms[j];
    //         // Swap operators if left operator is a and right operator is a^\dagger
    //         if (left_sub_term.second == TermValue::a && right_sub_term.second == TermValue::adg) {
    //             std::swap(terms[j - 1], terms[j]);
    //             coeff *= -1.;
    //             // If indice are same, employ the anti-commutation relationship and generate the new term
    //             if (left_sub_term.first == right_sub_term.first) {
    //                 // NB: skip elements j-1 and j
    //                 auto new_term = terms_t(begin(terms), begin(terms) + j - 1);
    //                 new_term.reserve(std::size(terms) - 1);
    //                 std::copy(begin(terms) + j + 1, end(terms), std::back_inserter(new_term));
    //             }
    //         } else if (left_sub_term.second == right_sub_term.second) {
    //             // TODO(dnguyen): Check that this is really ok? What if ordered_term is not zero?
    //             // If indices are the same, evaluate to zero
    //             if (left_sub_term.first == right_sub_term.first) {
    //                 return;
    //             }
    //             // Swap them if the same operator but lower index on the left
    //             if (left_sub_term.first < right_sub_term.first) {
    //                 std::swap(terms[j - 1], terms[j]);
    //                 coeff *= -1.;
    //             }
    //         }
    //     }
    // }

    return {std::move(local_ops), coeff};  // TODO(dnguyen): Should we move? or can (N)RVO take care of that?
}
}  // namespace mindquantum::ops

// =============================================================================

namespace mindquantum::ops {
auto FermionOperator::parse_string_(std::string_view terms_string) -> terms_t {
    MQ_INFO("Attempting to parse: '{}'", terms_string);
    if (terms_t terms; parser::parse_object_skipper(begin(terms_string), end(terms_string), terms,
                                                    ::parser::fm_op::terms, x3::space)) {
        return terms;
    }
    MQ_ERROR("FermionOperator terms string parsing failed for '{}'", terms_string);
    return {};
}

// =============================================================================

auto FermionOperator::simplify_(std::vector<term_t> terms, coefficient_t coeff)
    -> std::tuple<std::vector<term_t>, coefficient_t> {
    return sort_terms_(std::move(terms), coeff);
}

// =============================================================================

auto FermionOperator::sort_terms_(terms_t local_ops, coefficient_t coeff) -> std::pair<terms_t, coefficient_t> {
    return FermionOperator::normal_ordered_term_(std::move(local_ops), coeff);
}

// =============================================================================

}  // namespace mindquantum::ops
