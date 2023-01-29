# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""CT gate related decompose rule."""


import numpy as np

from mindquantum.core import gates
from mindquantum.core.circuit import Circuit
from mindquantum.utils.type_value_check import _check_control_num, _check_input_type


def ct_decompose(gate: gates.TGate):
    """
    Decompose ct gate.

    Args:
        gate (TGate): a TGate with one control qubits.

    Returns:
        List[Circuit], all possible decompose solution.

    Examples:
        >>> from mindquantum.algorithm.compiler.decompose import ct_decompose
        >>> from mindquantum.core.circuit import Circuit
        >>> from mindquantum.core.gates import T
        >>> ct = T.on(1, 0)
        >>> origin_circ = Circuit() + ct
        >>> decomposed_circ = ct_decompose(ct)[0]
        >>> origin_circ
        q0: ──●──
              │
        q1: ──T──
        >>> decomposed_circ
        q0: ──RS(π/8)────●────────────────●──
                         │                │
        q1: ──RZ(π/8)────X────RZ(-π/8)────X──
    """
    _check_input_type('gate', gates.TGate, gate)
    _check_control_num(gate.ctrl_qubits, 1)
    circuit1 = Circuit()
    control = gate.ctrl_qubits[0]
    target = gate.obj_qubits[0]
    circuit1 += gates.PhaseShift(np.pi / 8).on(control)
    circuit1 += gates.RZ(np.pi / 8).on(target)
    circuit1 += gates.X.on(target, control)
    circuit1 += gates.RZ(-np.pi / 8).on(target)
    circuit1 += gates.X.on(target, control)
    return [circuit1]


decompose_rules = ['ct_decompose']
__all__ = decompose_rules
