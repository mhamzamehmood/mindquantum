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

#include <cmath>

#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <ratio>
#include <stdexcept>
#include <vector>

#include "config/openmp.hpp"

#include "core/utils.hpp"
#include "simulator/densitymatrix/detail/cpu_densitymatrix_policy.hpp"
#include "simulator/types.hpp"
#include "simulator/utils.hpp"

namespace mindquantum::sim::densitymatrix::detail {
void CPUDensityMatrixPolicyBase::ApplyTwoQubitsMatrix(qs_data_p_t src, qs_data_p_t des, const qbits_t& objs, const qbits_t& ctrls,
                                                      const matrix_t& m, index_t dim) {
    DoubleQubitGateMask mask(objs, ctrls);
    if (!mask.ctrl_mask) {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b <= a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;
                    VT<VT<qs_data_t>> tmp_mat(4, VT<qs_data_t>(4));
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            tmp_mat[i][j] = m[i][0] * GetValue(src, row[0], col[j])
                                            + m[i][1] * GetValue(src, row[1], col[j])
                                            + m[i][2] * GetValue(src, row[2], col[j])
                                            + m[i][3] * GetValue(src, row[3], col[j]);
                        }
                    }

                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            auto new_value = tmp_mat[i][0] * std::conj(m[j][0]) + tmp_mat[i][1] * std::conj(m[j][1])
                                             + tmp_mat[i][2] * std::conj(m[j][2]) + tmp_mat[i][3] * std::conj(m[j][3]);
                            SetValue(des, row[i], col[j], new_value);
                        }
                    }
                }
            })
    } else {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b <= a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    if (((row[0] & mask.ctrl_mask) != mask.ctrl_mask)
                        && ((col[0] & mask.ctrl_mask) != mask.ctrl_mask)) {  // both not in control
                        continue;
                    }
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;
                    VT<VT<qs_data_t>> tmp_mat(4, VT<qs_data_t>(4));
                    if ((row[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // row in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                tmp_mat[i][j] = m[i][0] * GetValue(src, row[0], col[j])
                                                + m[i][1] * GetValue(src, row[1], col[j])
                                                + m[i][2] * GetValue(src, row[2], col[j])
                                                + m[i][3] * GetValue(src, row[3], col[j]);
                            }
                        }
                    } else {  // row not in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                tmp_mat[i][j] = GetValue(src, row[i], col[j]);
                            }
                        }
                    }
                    if ((col[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // column in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                auto new_value = tmp_mat[i][0] * std::conj(m[j][0]) + tmp_mat[i][1] * std::conj(m[j][1])
                                                 + tmp_mat[i][2] * std::conj(m[j][2]) + tmp_mat[i][3] * std::conj(m[j][3]);
                                SetValue(des, row[i], col[j], new_value);
                            }
                        }
                    } else {  // column not in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                SetValue(des, row[i], col[j], tmp_mat[i][j]);
                            }
                        }
                    }
                }
            })
    }
}

void CPUDensityMatrixPolicyBase::ApplySWAP(qs_data_p_t qs, const qbits_t& objs, const qbits_t& ctrls, index_t dim) {
    DoubleQubitGateMask mask(objs, ctrls);
    if (!mask.ctrl_mask) {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                index_t r0;  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              r0);
                auto r3 = r0 + mask.obj_mask;
                auto r1 = r0 + mask.obj_min_mask;
                auto r2 = r0 + mask.obj_max_mask;
                for (index_t b = 0; b < a; b++) {
                    index_t c0;  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, c0);
                    auto c3 = c0 + mask.obj_mask;
                    auto c1 = c0 + mask.obj_min_mask;
                    auto c2 = c0 + mask.obj_max_mask;
                    SwapValue(qs, r0, c1, r0, c2, 1);
                    SwapValue(qs, r3, c1, r3, c2, 1);
                    SwapValue(qs, r1, c0, r2, c0, 1);
                    SwapValue(qs, r1, c3, r2, c3, 1);
                    SwapValue(qs, r1, c1, r2, c2, 1);
                    SwapValue(qs, r1, c2, r2, c1, 1);
                }
                // diagonal case
                qs_data_t tmp;
                tmp = qs[IdxMap(r3, r1)];
                qs[IdxMap(r3, r1)] = qs[IdxMap(r3, r2)];
                qs[IdxMap(r3, r2)] = tmp;

                tmp = qs[IdxMap(r1, r0)];
                qs[IdxMap(r1, r0)] = qs[IdxMap(r2, r0)];
                qs[IdxMap(r2, r0)] = tmp;

                tmp = qs[IdxMap(r1, r1)];
                qs[IdxMap(r1, r1)] = qs[IdxMap(r2, r2)];
                qs[IdxMap(r2, r2)] = tmp;

                qs[IdxMap(r2, r1)] = std::conj(qs[IdxMap(r2, r1)]);
            })
    } else {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                index_t r0;  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              r0);
                auto r3 = r0 + mask.obj_mask;
                auto r1 = r0 + mask.obj_min_mask;
                auto r2 = r0 + mask.obj_max_mask;
                for (index_t b = 0; b < a; b++) {
                    index_t c0;  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, c0);
                    if (((r0 & mask.ctrl_mask) != mask.ctrl_mask)
                        && ((c0 & mask.ctrl_mask) != mask.ctrl_mask)) {  // both not in control
                        continue;
                    }
                    auto c3 = c0 + mask.obj_mask;
                    auto c1 = c0 + mask.obj_min_mask;
                    auto c2 = c0 + mask.obj_max_mask;
                    if ((r0 & mask.ctrl_mask) == mask.ctrl_mask) {
                        if ((c0 & mask.ctrl_mask) == mask.ctrl_mask) {  // both in control
                            SwapValue(qs, r0, c1, r0, c2, 1);
                            SwapValue(qs, r3, c1, r3, c2, 1);
                            SwapValue(qs, r1, c0, r2, c0, 1);
                            SwapValue(qs, r1, c3, r2, c3, 1);
                            SwapValue(qs, r1, c1, r2, c2, 1);
                            SwapValue(qs, r1, c2, r2, c1, 1);
                        } else {  // only row in control
                            SwapValue(qs, r1, c0, r2, c0, 1);
                            SwapValue(qs, r1, c1, r2, c1, 1);
                            SwapValue(qs, r1, c2, r2, c2, 1);
                            SwapValue(qs, r1, c3, r2, c3, 1);
                        }
                    } else {  // only column in control
                        SwapValue(qs, r0, c1, r0, c2, 1);
                        SwapValue(qs, r1, c1, r1, c2, 1);
                        SwapValue(qs, r2, c1, r2, c2, 1);
                        SwapValue(qs, r3, c1, r3, c2, 1);
                    }
                }
                // diagonal case
                if ((r0 & mask.ctrl_mask) == mask.ctrl_mask) {
                    qs_data_t tmp;
                    tmp = qs[IdxMap(r3, r1)];
                    qs[IdxMap(r3, r1)] = qs[IdxMap(r3, r2)];
                    qs[IdxMap(r3, r2)] = tmp;

                    tmp = qs[IdxMap(r1, r0)];
                    qs[IdxMap(r1, r0)] = qs[IdxMap(r2, r0)];
                    qs[IdxMap(r2, r0)] = tmp;

                    tmp = qs[IdxMap(r1, r1)];
                    qs[IdxMap(r1, r1)] = qs[IdxMap(r2, r2)];
                    qs[IdxMap(r2, r2)] = tmp;

                    qs[IdxMap(r2, r1)] = std::conj(qs[IdxMap(r2, r1)]);
                }
            })
    }
}

void CPUDensityMatrixPolicyBase::ApplyISWAP(qs_data_p_t qs, const qbits_t& objs, const qbits_t& ctrls, index_t dim) {
    DoubleQubitGateMask mask(objs, ctrls);
    if (!mask.ctrl_mask) {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                index_t r0;  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              r0);
                auto r3 = r0 + mask.obj_mask;
                auto r1 = r0 + mask.obj_min_mask;
                auto r2 = r0 + mask.obj_max_mask;
                for (index_t b = 0; b < a; b++) {
                    index_t c0;  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, c0);
                    auto c3 = c0 + mask.obj_mask;
                    auto c1 = c0 + mask.obj_min_mask;
                    auto c2 = c0 + mask.obj_max_mask;
                    SwapValue(qs, r0, c1, r0, c2, IMAGE_MI);
                    SwapValue(qs, r3, c1, r3, c2, IMAGE_MI);
                    SwapValue(qs, r1, c0, r2, c0, IMAGE_I);
                    SwapValue(qs, r1, c3, r2, c3, IMAGE_I);
                    SwapValue(qs, r1, c1, r2, c2, 1);
                    SwapValue(qs, r1, c2, r2, c1, 1);
                }
                // diagonal case
                qs_data_t tmp;
                tmp = qs[IdxMap(r3, r1)];
                qs[IdxMap(r3, r1)] = IMAGE_MI * qs[IdxMap(r3, r2)];
                qs[IdxMap(r3, r2)] = IMAGE_MI * tmp;

                tmp = qs[IdxMap(r1, r0)];
                qs[IdxMap(r1, r0)] = IMAGE_I * qs[IdxMap(r2, r0)];
                qs[IdxMap(r2, r0)] = IMAGE_I * tmp;

                tmp = qs[IdxMap(r1, r1)];
                qs[IdxMap(r1, r1)] = qs[IdxMap(r2, r2)];
                qs[IdxMap(r2, r2)] = tmp;

                qs[IdxMap(r2, r1)] = std::conj(qs[IdxMap(r2, r1)]);
            })
    } else {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                index_t r0;  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              r0);
                auto r3 = r0 + mask.obj_mask;
                auto r1 = r0 + mask.obj_min_mask;
                auto r2 = r0 + mask.obj_max_mask;
                for (index_t b = 0; b < a; b++) {
                    index_t c0;  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, c0);
                    if (((r0 & mask.ctrl_mask) != mask.ctrl_mask)
                        && ((c0 & mask.ctrl_mask) != mask.ctrl_mask)) {  // both not in control
                        continue;
                    }
                    auto c3 = c0 + mask.obj_mask;
                    auto c1 = c0 + mask.obj_min_mask;
                    auto c2 = c0 + mask.obj_max_mask;
                    if ((r0 & mask.ctrl_mask) == mask.ctrl_mask) {
                        if ((c0 & mask.ctrl_mask) == mask.ctrl_mask) {  // both in control
                            SwapValue(qs, r0, c1, r0, c2, IMAGE_MI);
                            SwapValue(qs, r3, c1, r3, c2, IMAGE_MI);
                            SwapValue(qs, r1, c0, r2, c0, IMAGE_I);
                            SwapValue(qs, r1, c3, r2, c3, IMAGE_I);
                            SwapValue(qs, r1, c1, r2, c2, 1);
                            SwapValue(qs, r1, c2, r2, c1, 1);
                        } else {  // only row in control
                            SwapValue(qs, r1, c0, r2, c0, IMAGE_I);
                            SwapValue(qs, r1, c1, r2, c1, IMAGE_I);
                            SwapValue(qs, r1, c2, r2, c2, IMAGE_I);
                            SwapValue(qs, r1, c3, r2, c3, IMAGE_I);
                        }
                    } else {  // only column in control
                        SwapValue(qs, r0, c1, r0, c2, IMAGE_MI);
                        SwapValue(qs, r1, c1, r1, c2, IMAGE_MI);
                        SwapValue(qs, r2, c1, r2, c2, IMAGE_MI);
                        SwapValue(qs, r3, c1, r3, c2, IMAGE_MI);
                    }
                }
                // diagonal case
                if ((r0 & mask.ctrl_mask) == mask.ctrl_mask) {
                    qs_data_t tmp;
                    tmp = qs[IdxMap(r3, r1)];
                    qs[IdxMap(r3, r1)] = IMAGE_MI * qs[IdxMap(r3, r2)];
                    qs[IdxMap(r3, r2)] = IMAGE_MI * tmp;

                    tmp = qs[IdxMap(r1, r0)];
                    qs[IdxMap(r1, r0)] = IMAGE_I * qs[IdxMap(r2, r0)];
                    qs[IdxMap(r2, r0)] = IMAGE_I * tmp;

                    tmp = qs[IdxMap(r1, r1)];
                    qs[IdxMap(r1, r1)] = qs[IdxMap(r2, r2)];
                    qs[IdxMap(r2, r2)] = tmp;

                    qs[IdxMap(r2, r1)] = std::conj(qs[IdxMap(r2, r1)]);
                }
            })
    }
}

void CPUDensityMatrixPolicyBase::ApplyXX(qs_data_p_t qs, const qbits_t& objs, const qbits_t& ctrls, calc_type val,
                                         index_t dim, bool diff) {
    DoubleQubitGateMask mask(objs, ctrls);
    auto c = std::cos(val);
    auto s = std::sin(val) * IMAGE_MI;
    if (diff) {
        c = -std::sin(val);
        s = std::cos(val) * IMAGE_MI;
    }
    if (!mask.ctrl_mask) {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b <= a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;

                    VT<VT<qs_data_t>> tmp_mat(4, VT<qs_data_t>(4));
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            tmp_mat[i][j] = c * GetValue(qs, row[i], col[j]) + s * GetValue(qs, row[3 - i], col[j]);
                        }
                    }
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            SetValue(qs, row[i], col[j], c * tmp_mat[i][j] - s * tmp_mat[i][3 - j]);
                        }
                    }
                }
            })
    } else {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b <= a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    if (((row[0] & mask.ctrl_mask) != mask.ctrl_mask)
                        && ((col[0] & mask.ctrl_mask) != mask.ctrl_mask)) {  // both not in control
                        continue;
                    }
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;
                    VT<VT<qs_data_t>> tmp_mat(4, VT<qs_data_t>(4));
                    if ((row[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // row in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                tmp_mat[i][j] = c * GetValue(qs, row[i], col[j]) + s * GetValue(qs, row[3 - i], col[j]);
                            }
                        }
                    } else {  // row not in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                tmp_mat[i][j] = GetValue(qs, row[i], col[j]);
                            }
                        }
                    }
                    if ((col[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // column in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                SetValue(qs, row[i], col[j], c * tmp_mat[i][j] - s * tmp_mat[i][3 - j]);
                            }
                        }
                    } else {  // column not in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                SetValue(qs, row[i], col[j], tmp_mat[i][j]);
                            }
                        }
                    }
                }
            })
        if (diff) {
            CPUDensityMatrixPolicyBase::SetToZeroExcept(qs, mask.ctrl_mask, dim);
        }
    }
}

void CPUDensityMatrixPolicyBase::ApplyYY(qs_data_p_t qs, const qbits_t& objs, const qbits_t& ctrls, calc_type val,
                                         index_t dim, bool diff) {
    DoubleQubitGateMask mask(objs, ctrls);
    auto c = std::cos(val);
    auto s = std::sin(val) * IMAGE_I;
    if (diff) {
        c = -std::sin(val);
        s = std::cos(val) * IMAGE_I;
    }
    if (!mask.ctrl_mask) {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b <= a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;
                    VT<VT<qs_data_t>> tmp_mat(4, VT<qs_data_t>(4));
                    for (int j = 0; j < 4; j++) {
                        tmp_mat[0][j] = c * GetValue(qs, row[0], col[j]) + s * GetValue(qs, row[3], col[j]);
                        tmp_mat[1][j] = c * GetValue(qs, row[1], col[j]) - s * GetValue(qs, row[2], col[j]);
                        tmp_mat[2][j] = c * GetValue(qs, row[2], col[j]) - s * GetValue(qs, row[1], col[j]);
                        tmp_mat[3][j] = c * GetValue(qs, row[3], col[j]) + s * GetValue(qs, row[0], col[j]);
                    }
                    VT<VT<qs_data_t>> res_mat(4, VT<qs_data_t>(4));
                    for (int i = 0; i < 4; i++) {
                        res_mat[i][0] = c * tmp_mat[i][0] - s * tmp_mat[i][3];
                        res_mat[i][1] = c * tmp_mat[i][1] + s * tmp_mat[i][2];
                        res_mat[i][2] = c * tmp_mat[i][2] + s * tmp_mat[i][1];
                        res_mat[i][3] = c * tmp_mat[i][3] - s * tmp_mat[i][0];
                    }
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            SetValue(qs, row[i], col[j], res_mat[i][j]);
                        }
                    }
                }
            })
    } else {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b <= a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    if (((row[0] & mask.ctrl_mask) != mask.ctrl_mask)
                        && ((col[0] & mask.ctrl_mask) != mask.ctrl_mask)) {  // both not in control
                        continue;
                    }
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;
                    VT<VT<qs_data_t>> tmp_mat(4, VT<qs_data_t>(4));
                    if ((row[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // row in control
                        for (int j = 0; j < 4; j++) {
                            tmp_mat[0][j] = c * GetValue(qs, row[0], col[j]) + s * GetValue(qs, row[3], col[j]);
                            tmp_mat[1][j] = c * GetValue(qs, row[1], col[j]) - s * GetValue(qs, row[2], col[j]);
                            tmp_mat[2][j] = c * GetValue(qs, row[2], col[j]) - s * GetValue(qs, row[1], col[j]);
                            tmp_mat[3][j] = c * GetValue(qs, row[3], col[j]) + s * GetValue(qs, row[0], col[j]);
                        }
                    } else {  // row not in control
                        for (int i = 0; i < 4; i++) {
                            for (int j = 0; j < 4; j++) {
                                tmp_mat[i][j] = GetValue(qs, row[i], col[j]);
                            }
                        }
                    }
                    if ((col[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // column in control
                        VT<VT<qs_data_t>> res_mat(4, VT<qs_data_t>(4));
                        for (int i = 0; i < 4; i++) {
                            res_mat[i][0] = c * tmp_mat[i][0] - s * tmp_mat[i][3];
                            res_mat[i][1] = c * tmp_mat[i][1] + s * tmp_mat[i][2];
                            res_mat[i][2] = c * tmp_mat[i][2] + s * tmp_mat[i][1];
                            res_mat[i][3] = c * tmp_mat[i][3] - s * tmp_mat[i][0];
                        }
                        tmp_mat.swap(res_mat);
                    }  // do nothing if column not in control

                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 4; j++) {
                            SetValue(qs, row[i], col[j], tmp_mat[i][j]);
                        }
                    }
                }
            })
        if (diff) {
            CPUDensityMatrixPolicyBase::SetToZeroExcept(qs, mask.ctrl_mask, dim);
        }
    }
}

void CPUDensityMatrixPolicyBase::ApplyZZ(qs_data_p_t qs, const qbits_t& objs, const qbits_t& ctrls, calc_type val,
                                         index_t dim, bool diff) {
    DoubleQubitGateMask mask(objs, ctrls);
    auto c = std::cos(val);
    auto s = std::sin(val);
    if (diff) {
        c = -std::sin(val);
        s = std::cos(val);
    }
    auto e = c + IMAGE_I * s;
    auto me = c + IMAGE_MI * s;
    auto me2 = me * me;
    auto e2 = e * e;
    if (!mask.ctrl_mask) {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b < a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;

                    SelfMultiply(qs, row[0], col[1], me2);
                    SelfMultiply(qs, row[0], col[2], me2);
                    SelfMultiply(qs, row[3], col[1], me2);
                    SelfMultiply(qs, row[3], col[2], me2);

                    SelfMultiply(qs, row[1], col[0], e2);
                    SelfMultiply(qs, row[2], col[0], e2);
                    SelfMultiply(qs, row[1], col[3], e2);
                    SelfMultiply(qs, row[2], col[3], e2);
                }
                // diagonal case
                SelfMultiply(qs, row[1], row[0], e2);
                SelfMultiply(qs, row[2], row[0], e2);
                SelfMultiply(qs, row[3], row[1], me2);
                SelfMultiply(qs, row[3], row[2], me2);
            })
    } else {
        THRESHOLD_OMP_FOR(
            dim, DimTh, for (omp::idx_t a = 0; a < (dim / 4); a++) {
                VT<index_t> row(4);  // row index of reduced matrix entry
                SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask, a,
                              row[0]);
                row[3] = row[0] + mask.obj_mask;
                row[1] = row[0] + mask.obj_min_mask;
                row[2] = row[0] + mask.obj_max_mask;
                for (index_t b = 0; b < a; b++) {
                    VT<index_t> col(4);  // column index of reduced matrix entry
                    SHIFT_BIT_TWO(mask.obj_low_mask, mask.obj_rev_low_mask, mask.obj_high_mask, mask.obj_rev_high_mask,
                                  b, col[0]);
                    if (((row[0] & mask.ctrl_mask) != mask.ctrl_mask)
                        && ((col[0] & mask.ctrl_mask) != mask.ctrl_mask)) {  // both not in control
                        continue;
                    }
                    col[3] = col[0] + mask.obj_mask;
                    col[1] = col[0] + mask.obj_min_mask;
                    col[2] = col[0] + mask.obj_max_mask;
                    if ((row[0] & mask.ctrl_mask) == mask.ctrl_mask) {
                        if ((col[0] & mask.ctrl_mask) == mask.ctrl_mask) {  // both in control
                            SelfMultiply(qs, row[0], col[1], me2);
                            SelfMultiply(qs, row[0], col[2], me2);
                            SelfMultiply(qs, row[3], col[1], me2);
                            SelfMultiply(qs, row[3], col[2], me2);

                            SelfMultiply(qs, row[1], col[0], e2);
                            SelfMultiply(qs, row[2], col[0], e2);
                            SelfMultiply(qs, row[1], col[3], e2);
                            SelfMultiply(qs, row[2], col[3], e2);
                        } else {  // only row in control
                            for (int j = 0; j < 4; j++) {
                                SelfMultiply(qs, row[0], col[j], me);
                                SelfMultiply(qs, row[3], col[j], me);
                                SelfMultiply(qs, row[1], col[j], e);
                                SelfMultiply(qs, row[2], col[j], e);
                            }
                        }
                    } else {  // only column in control
                        for (int i = 0; i < 4; i++) {
                            SelfMultiply(qs, row[i], col[0], e);
                            SelfMultiply(qs, row[i], col[3], e);
                            SelfMultiply(qs, row[i], col[1], me);
                            SelfMultiply(qs, row[i], col[2], me);
                        }
                    }
                }
                // diagonal case
                if ((row[0] & mask.ctrl_mask) == mask.ctrl_mask) {
                    SelfMultiply(qs, row[1], row[0], e2);
                    SelfMultiply(qs, row[2], row[0], e2);
                    SelfMultiply(qs, row[3], row[1], me2);
                    SelfMultiply(qs, row[3], row[2], me2);
                }
            })
        if (diff) {
            CPUDensityMatrixPolicyBase::SetToZeroExcept(qs, mask.ctrl_mask, dim);
        }
    }
}
}  // namespace mindquantum::sim::densitymatrix::detail
