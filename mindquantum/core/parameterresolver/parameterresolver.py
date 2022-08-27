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

# pylint: disable=too-many-lines
"""Parameter resolver."""

import json
import numbers
from typing import Iterable

import numpy as np

from mindquantum.mqbackend import complex_pr as complex_pr_
from mindquantum.utils.f import is_two_number_close
from mindquantum.utils.string_utils import join_without_empty, string_expression
from mindquantum.utils.type_value_check import _check_input_type, _check_int_type


def is_type_upgrade(origin_v, other_v):
    """Check whether type upgraded."""
    tmp = origin_v + other_v
    return not isinstance(tmp, type(origin_v))


# pylint: disable=invalid-overridden-method
class ParameterResolver(complex_pr_):  # pylint: disable=too-many-public-methods
    """
    A ParameterRsolver can set the parameter of parameterized quantum gate or parameterized quantum circuit.

    Args:
        data (Union[dict, numbers.Number, str, ParameterResolver]): initial parameter names and
            its values. If data is a dict, the key will be the parameter name
            and the value will be the parameter value. If data is a number, this
            number will be the constant value of this parameter resolver. If data
            is a string, then this string will be the only parameter with coefficient
            be 1. Default: None.
        const (number.Number): the constant part of this parameter resolver.
            Default: None.
        dtype (type): the value type of this parameter resolver. Default: None.

    Examples:
        >>> from mindquantum.core.parameterresolver import ParameterResolver
        >>> pr = ParameterResolver({'a': 0.3})
        >>> pr['b'] = 0.5
        >>> pr.no_grad_part('a')
        {'a': 0.3, 'b': 0.5}, const: 0.0
        >>> pr *= 2
        >>> pr
        {'a': 0.6, 'b': 1.0}, const: 0.0
        >>> pr.expression()
        '3/5*a + b'
        >>> pr.const = 0.5
        >>> pr.expression()
        '3/5*a + b + 1/2'
        >>> pr.no_grad_parameters
        {'a'}
        >>> ParameterResolver(3)
        {}, const: 3.0
        >>> ParameterResolver('a')
        {'a': 1.0}, const: 0.0
    """

    def __init__(self, data=None, const=None):
        """Initialize a ParameterResolver object."""
        if isinstance(data, complex_pr_):
            complex_pr_.__init__(self, data, isinstance(data, ParameterResolver))
        if isinstance(data, numbers.Number):
            complex_pr_.__init__(self, data)
        else:
            if const is None:
                const = 0.0
            if data is None:
                data = {}
            if isinstance(data, str):
                data = {data: 1.0}
            complex_pr_.__init__(self, data, const)

    @property
    def const(self) -> numbers.Number:
        """
        Get the constant part of this parameter resolver.

        Returns:
            numbers.Number, the constant part of this parameter resolver.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1}, 2.5)
            >>> pr.const
            2.5
        """
        return complex_pr_.const(self)

    @const.setter
    def const(self, const_value):
        """Setter method for const."""
        complex_pr_.set_const(self, const_value)

    def __len__(self) -> int:
        """
        Get the number of parameters in this parameter resolver.

        Please note that the parameter with 0 coefficient is also considered.

        Returns:
            int, the number of all parameters.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 0, 'b': 1})
            >>> a.expression()
            'b'
            >>> len(a)
            2
        """
        return complex_pr_.__len__(self)

    def keys(self):
        """
        Return an iterator that yields the name and value of all parameters.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 0, 'b': 1})
            >>> list(a.keys())
            ['a', 'b']
        """
        for k in range(len(self)):
            yield complex_pr_.get_key(self, k)

    def values(self):
        """
        Return an iterator that yields the name and value of all parameters.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 0, 'b': 1})
            >>> list(a.values())
            [0.0, 1.0]
        """
        for k in self.keys():
            yield complex_pr_.__getitem__(self, k)

    def items(self):
        """
        Return an iterator that yields the name and value of all parameters.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 0, 'b': 1})
            >>> list(a.items())
            [('a', 0.0), ('b', 1.0)]
        """
        for i in range(len(self)):
            key = complex_pr_.get_key(self, i)
            yield (key, complex_pr_.__getitem__(self, key))

    def is_const(self) -> bool:
        """
        Check whether this parameter resolver represents a constant number.

        This means that there is no parameter with non zero coefficient in this parameter resolver.

        Returns:
            bool, whether this parameter resolver represent a constant number.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR(1.0)
            >>> pr.is_const()
            True
        """
        return complex_pr_.is_const(self)

    def __bool__(self) -> bool:
        """
        Check whether this parameter resolver has non zero constant or parameter with non zero coefficient.

        Returns:
            bool, False if this parameter resolver represent zero and True if not.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR(0)
            >>> bool(pr)
            False
        """
        return complex_pr_.__bool__(self)

    def __setitem__(self, keys, values):
        """
        Set the value of parameter in this parameter resolver.

        You can set multiple values of multiple parameters with given iterable keys and values.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR(0)
            >>> pr['a'] = 2.5
            >>> pr.expression()
            '5/2*a'
        """
        if isinstance(keys, str):
            _check_input_type("parameter name", str, keys)
            _check_input_type("parameter value", numbers.Number, values)
            if not keys.strip():
                raise KeyError("parameter name cannot be empty string.")
            complex_pr_.__setitem__(self, keys, values)
        elif isinstance(keys, Iterable):
            if not isinstance(values, Iterable):
                raise ValueError("Values should be iterable.")
            if len(values) != len(keys):
                raise ValueError("Size of keys and values do not match.")
            for k, v in zip(keys, values):
                complex_pr_.__setitem__(self, k, v)

        else:
            raise TypeError(f"Parameter name should be a string, but get {type(keys)}!")

    def __getitem__(self, key: str) -> numbers.Number:
        """
        Get the parameter value from this parameter resolver.

        Returns:
            numbers.Number, the parameter value.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1, 'b': 2})
            >>> pr['a']
            1.0
        """
        _check_input_type('key', str, key)
        return complex_pr_.__getitem__(self, key)

    def __iter__(self):
        """
        Yield the parameter name.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1, 'b': 2})
            >>> list(pr)
            ['a', 'b']
        """
        yield from self.keys()

    def __contains__(self, key) -> bool:
        """
        Check whether the given key is in this parameter resolver or not.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1, 'b': 2})
            >>> 'c' in pr
            False
        """
        _check_input_type('key', str, key)
        return complex_pr_.__contains__(self, key)

    def __eq__(self, other) -> bool:
        """
        To check whether two parameter resolvers are equal.

        Args:
            other (Union[numbers.Number, str, ParameterResolver]): The parameter resolver
                or number you want to compare. If a number or string is given, this number will
                convert to a parameter resolver.

        Returns:
            bool, whether two parameter resolvers are equal.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> PR(3) == 3
            True
            >>> PR('a') == 3
            False
            >>> PR({'a': 2}, 3) == PR({'a': 2}) + 3
            True
        """
        if isinstance(other, numbers.Number):
            return complex_pr_.__eq__(self, other)
        if isinstance(other, str):
            if not is_two_number_close(self.const, 0):
                return False
            if len(self) == 1 and other in self:
                return is_two_number_close(self[other], 1)
            return False
        _check_input_type("other", ParameterResolver, other)
        return complex_pr_.__eq__(self, other)

    def __copy__(self) -> "ParameterResolver":
        """
        Copy a new parameter resolver.

        Returns:
            ParameterResolver, the new parameter resolver you copied.

        Examples:
            >>> import copy
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 2}, 3)
            >>> b = copy.copy(a)
            >>> c = a
            >>> a['a'] = 4
            >>> a.expression()
            '4*a + 3'
            >>> b.expression()
            '2*a + 3'
            >>> c.expression()
            '4*a + 3'
        """
        return ParameterResolver(complex_pr_.__copy__(self))

    def __deepcopy__(self, memo):
        """Deep copy operator."""
        return ParameterResolver(complex_pr_.__copy__(self))

    def __iadd__(self, other) -> "ParameterResolver":
        """
        Inplace add a number or parameter resolver.

        Args (Union[numbers.Number, ParameterResolver]): the number or parameter
            resolver you want add.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 3})
            >>> pr += 4
            >>> pr.expression()
            '3*a + 4'
            >>> pr += PR({'b': 1.5})
            >>> pr
            '3*a + 3/2*b + 4'
        """
        complex_pr_.__iadd__(self, other)
        return self

    def __add__(self, other) -> "ParameterResolver":
        """
        Add a number or parameter resolver.

        Args:
            other (Union[numbers.Number, ParameterResolver])

        Returns:
            ParameterResolver, the result of add.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 3})
            >>> pr + 1
            {'a': 3.0}, const: 1.0
            >>> pr + pr
            {'a': 6.0}, const: 0.0
        """
        return ParameterResolver(complex_pr_.__add__(self, other))

    def __radd__(self, other):
        """Add a number or ParameterResolver."""
        return ParameterResolver(complex_pr_.__radd__(self, other))

    def __isub__(self, other):
        """Self subtract a number or ParameterResolver."""
        complex_pr_.__isub__(self, other)
        return self

    def __sub__(self, other):
        """Subtract a number or ParameterResolver."""
        return ParameterResolver(complex_pr_.__sub__(self, other))

    def __rsub__(self, other):
        """Self subtract a number or ParameterResolver."""
        return ParameterResolver(complex_pr_.__rsub__(self, other))

    def __imul__(self, other):
        """Self multiply a number or ParameterResolver."""
        complex_pr_.__imul__(self, other)
        return self

    def __mul__(self, other):
        """Multiply a number or ParameterResolver."""
        return ParameterResolver(complex_pr_.__mul__(self, other))

    def __rmul__(self, other):
        """Multiply a number or ParameterResolver."""
        return ParameterResolver(complex_pr_.__rmul__(self, other))

    def __neg__(self):
        """Return the negative of this parameter resolver."""
        return ParameterResolver(complex_pr_.__neg__(self))

    def __itruediv__(self, other):
        """Divide a number inplace."""
        complex_pr_.__itruediv__(self, other)
        return self

    def __truediv__(self, other):
        """Divide a number."""
        return ParameterResolver(complex_pr_.__truediv__(self, other))

    def __str__(self):
        """Return the string expression of this parameter resolver."""
        return complex_pr_.__str__(self)

    def __repr__(self) -> str:
        """Return the repr of this parameter resolver."""
        return self.__str__()

    def __float__(self):
        """Convert the constant part to float. Raise error if it's not constant."""
        if not self.is_const():
            raise ValueError("parameter resolver is not constant, cannot convert to float.")
        return np.float64(self.const.real)

    def expression(self):
        """
        Get the expression string of this parameter resolver.

        Returns:
            str, the string expression of this parameter resolver.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> import numpy as np
            >>> pr = PR({'a': np.pi}, np.sqrt(2))
            >>> pr.expression()
            'π*a + √2'
        """
        string = {}
        for k, v in self.items():
            expr = string_expression(v)
            string[k] = expr
            if expr == '1':
                string[k] = ''
            if expr == '-1':
                string[k] = '-'

        const = string_expression(self.const)
        string[''] = const
        res = ''
        for k, v in string.items():
            current_s = v
            if current_s.endswith('j'):
                current_s = f'({current_s})'
            if res and (current_s.startswith('(') or not current_s.startswith('-')):
                res += ' + '
            if current_s != '0':
                res += join_without_empty('' if current_s == '-' else '*', [current_s, k])
        if res.endswith(' + '):
            res = res[:-3]
        return res if res else '0'

    @property
    def params_name(self):
        """
        Get the parameters name.

        Returns:
            list, a list of parameters name.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.params_name
            ['a', 'b']
        """
        return list(self.keys())

    @property
    def para_value(self):
        """
        Get the parameters value.

        Returns:
            list, a list of parameters value.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.para_value
            [1, 2]
        """
        return list(self.values())

    def requires_grad(self):
        """
        Set all parameters of this parameter resolver to require gradient calculation.

        Inplace operation.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.no_grad_part('a')
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.requires_grad()
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.requires_grad_parameters
            {'a', 'b'}
        """
        complex_pr_.requires_grad(self)
        return self

    def no_grad(self):
        """
        Set all parameters to not require gradient calculation. Inplace operation.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.no_grad()
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.requires_grad_parameters
            set()
        """
        complex_pr_.no_grad(self)
        return self

    def requires_grad_part(self, *names):
        """
        Set part of parameters that requires grad. Inplace operation.

        Args:
            names (tuple[str]): Parameters that requires grad.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.no_grad()
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.requires_grad_part('a')
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.requires_grad_parameters
            {'a'}
        """
        for name in names:
            _check_input_type('name', str, name)
        complex_pr_.requires_grad_part(self, names)
        return self

    def no_grad_part(self, *names):
        """
        Set part of parameters that not requires grad.

        Args:
            names (tuple[str]): Parameters that not requires grad.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.no_grad_part('a')
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.requires_grad_parameters
            {'b'}
        """
        for name in names:
            _check_input_type('name', str, name)
        complex_pr_.no_grad_part(self, names)
        return self

    def encoder_part(self, *names):
        """
        Set which part is encoder parameters.

        Args:
            names (tuple[str]): Parameters that will be serve as encoder.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.encoder_part('a')
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr.encoder_parameters
            {'a'}
        """
        for name in names:
            _check_input_type('name', str, name)
        complex_pr_.encoder_part(self, names)
        return self

    def ansatz_part(self, *names):
        """
        Set which part is ansatz parameters.

        Args:
            names (tuple[str]): Parameters that will be serve as ansatz.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2})
            >>> pr.as_encoder()
            >>> pr.ansatz_part('a')
            >>> pr.ansatz_parameters
            {'a'}
        """
        for name in names:
            _check_input_type('name', str, name)
        complex_pr_.ansatz_part(self, names)
        return self

    def as_encoder(self):
        """
        Set all the parameters as encoder.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1, 'b': 2})
            >>> pr.as_encoder()
            >>> pr.encoder_parameters
            {'a', 'b'}
        """
        complex_pr_.as_encoder(self)
        return self

    def as_ansatz(self):
        """
        Set all the parameters as ansatz.

        Returns:
            ParameterResolver, the parameter resolver itself.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1, 'b': 2})
            >>> pr.as_encoder()
            >>> pr.as_ansatz()
            >>> pr.ansatz_parameters
            {'a', 'b'}
        """
        complex_pr_.as_ansatz(self)
        return self

    def update(self, other):
        """
        Update this parameter resolver with other parameter resolver.

        Args:
            other (ParameterResolver): other parameter resolver.

        Raises:
            ValueError: If some parameters require grad and not require grad in other parameter resolver and vice versa
                and some parameters are encoder parameters and not encoder in other parameter resolver and vice versa.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr1 = ParameterResolver({'a': 1})
            >>> pr2 = ParameterResolver({'b': 2})
            >>> pr2.no_grad()
            {'b': 2.0}, const: 0.0
            >>> pr1.update(pr2)
            >>> pr1
            {'a': 1.0, 'b': 2.0}, const: 0.0
            >>> pr1.no_grad_parameters
            {'b'}
        """
        _check_input_type('other', ParameterResolver, other)
        complex_pr_.update(self, other)

    @property
    def requires_grad_parameters(self):
        """
        Get parameters that requires grad.

        Returns:
            set, the set of parameters that requires grad.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 1, 'b': 2})
            >>> a.requires_grad_parameters
            {'a', 'b'}
        """
        return set(self.params_name) - self.no_grad_parameters

    @property
    def no_grad_parameters(self):
        """
        Get parameters that do not require grad.

        Returns:
            set, the set of parameters that do not require grad.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 1, 'b': 2})
            >>> a.no_grad()
            >>> a.no_grad_parameters
            {'a', 'b'}
        """
        return complex_pr_.no_grad_parameters(self)

    @property
    def encoder_parameters(self):
        """
        Get parameters that is encoder parameters.

        Returns:
            set, the set of encoder parameters.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 1, 'b': 2})
            >>> a.as_encoder()
            >>> a.encoder_parameters
            {'a', 'b'}
        """
        return complex_pr_.encoder_parameters(self)

    @property
    def ansatz_parameters(self):
        """
        Get parameters that is ansatz parameters.

        Returns:
            set, the set of ansatz parameters.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 1, 'b': 2})
            >>> a.ansatz_parameters
            {'a', 'b'}
        """
        return set(self.params_name) - self.encoder_parameters

    def conjugate(self):
        """
        Get the conjugate of the parameter resolver.

        Returns:
            ParameterResolver, the conjugate version of this parameter resolver.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> import numpy as np
            >>> pr = PR({'a' : 1, 'b': 1j}, dtype=np.complex128)
            >>> pr.conjugate().expression()
            'a + (-1j)*b'
        """
        return ParameterResolver(complex_pr_.conjugate(self))

    def combination(self, other):
        """
        Apply linear combination between this parameter resolver with input parameter resolver.

        Args:
            other (Union[dict, ParameterResolver]): The parameter resolver you
                want to do linear combination.

        Returns:
            numbers.Number, the combination result.

        Examples:
            >>> from mindquantum import ParameterResolver
            >>> pr1 = ParameterResolver({'a': 1, 'b': 2})
            >>> pr2 = ParameterResolver({'a': 2, 'b': 3})
            >>> pr1.combination(pr2)
            {}, const: 8.0
        """
        _check_input_type('other', (ParameterResolver, dict), other)
        const = self.const
        for k, v in self.items():
            if k in other:
                const += v * other[k]
            else:
                raise ValueError(f"{k} not in input parameter resolver.")
        return self.__class__(const)

    def pop(self, v):
        """
        Pop out a parameter.

        Args:
            v (str): The parameter you want to pop.

        Returns:
            numbers.Number, the popped out parameter value.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> a = PR({'a': 1, 'b': 2})
            >>> a.pop('a')
            1.0
        """
        return complex_pr_.pop(self, v)

    @property
    def real(self):
        """
        Get the real part of every parameter value.

        Returns:
            ParameterResolver, real part parameter value.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR('a', 3) + 1j * PR('a', 4)
            >>> pr
            {'a': (1+1j)}, const: (3+4j)
            >>> pr.real
            {'a': 1.0}, const: 3.0
        """
        out = ParameterResolver(self)
        complex_pr_.keep_real(out)
        return out

    @property
    def imag(self):
        """
        Get the imaginary part of every parameter value.

        Returns:
            ParameterResolver, image part parameter value.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR('a', 3) + 1j * PR('a', 4)
            >>> pr
            {'a': (1+1j)}, const: (3+4j)
            >>> pr.imag
            {'a': 1.0}, const: 4.0
        """
        out = ParameterResolver(self)
        complex_pr_.keep_imag(out)
        return out

    def to_real_obj(self):
        """Convert to real type."""
        return complex_pr_.real(self)

    def is_hermitian(self):
        """
        To check whether the parameter value of this parameter resolver is hermitian or not.

        Returns:
            bool, whether the parameter resolver is hermitian or not.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1})
            >>> pr.is_hermitian()
            True
            >>> (pr + 3).is_hermitian()
            True
            >>> (pr * 1j).is_hermitian()
            False
        """
        return complex_pr_.is_hermitian(self)

    def is_anti_hermitian(self):
        """
        To check whether the parameter value of this parameter resolver is anti hermitian or not.

        Returns:
            bool, whether the parameter resolver is anti hermitian or not.

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver as PR
            >>> pr = PR({'a': 1})
            >>> pr.is_anti_hermitian()
            False
            >>> (pr + 3).is_anti_hermitian()
            False
            >>> (pr*1j).is_anti_hermitian()
            True
        """
        return complex_pr_.is_anti_hermitian(self)

    def dumps(self, indent=4):
        """
        Dump ParameterResolver into JSON(JavaScript Object Notation).

        Args:
            indent (int): Then JSON array elements and object members will be
                pretty-printed with that indent level. Default: 4.

        Returns:
            string(JSON), the JSON of ParameterResolver

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver
            >>> pr = ParameterResolver({'a': 1, 'b': 2}, const=3 + 4j, dtype=complex)
            >>> pr.no_grad_part('a', 'b')
            >>> print(pr.dumps())
            {
                "pr_data": {
                    "a": [
                        1.0,
                        0.0
                    ],
                    "b": [
                        2.0,
                        0.0
                    ]
                },
                "const": [
                    3.0,
                    4.0
                ],
                "dtype": "complex",
                "no_grad_parameters": [
                    "b",
                    "a"
                ],
                "encoder_parameters": []
            }
        """
        if indent is not None:
            _check_int_type('indent', indent)
        dic = {}
        dic['pr_data'] = {i: (j.real, j.imag) for i, j in self.items()}
        dic['const'] = (self.const.real, self.const.imag)
        dic['dtype'] = 'complex'
        dic['no_grad_parameters'] = list(self.no_grad_parameters)
        dic['encoder_parameters'] = list(self.encoder_parameters)
        return json.dumps(dic, indent=indent)

    @staticmethod
    def loads(strs):
        r"""
        Load JSON(JavaScript Object Notation) into FermionOperator.

        Args:
            strs (str): The dumped parameter resolver string.

        Returns:
            FermionOperator, the FermionOperator load from strings

        Examples:
            >>> from mindquantum.core.parameterresolver import ParameterResolver
            >>> ori = ParameterResolver({'a': 1, 'b': 2, 'c': 3, 'd': 4})
            >>> ori.no_grad_part('a', 'b')
            >>> string = ori.dumps()
            >>> obj = ParameterResolver.loads(string)
            >>> print(obj)
            {'a': 1, 'b': 2, 'c': 3, 'd': 4}, const: 0
            >>> print('requires_grad_parameters is:', obj.requires_grad_parameters)
            requires_grad_parameters is: {'c', 'd'}
            >>> print('no_grad_parameters is :', obj.no_grad_parameters)
            no_grad_parameters is : {'b', 'a'}
        """
        _check_input_type('strs', str, strs)
        dic = json.loads(strs)

        if 'dtype' not in dic:
            raise ValueError("Invalid string. Cannot convert it to ParameterResolver, no key dtype")
        dtype = np.float64 if dic['dtype'] == 'float' else np.complex128

        if 'pr_data' not in dic:
            raise ValueError("Invalid string. Cannot convert it to ParameterResolver, no key pr_data")
        pr_data = dic['pr_data']
        pr_data = {n: v_r if dtype == np.float64 else v_r + 1j * v_i for n, (v_r, v_i) in pr_data.items()}

        if 'const' not in dic:
            raise ValueError("Invalid string. Cannot convert it to ParameterResolver, no key const")
        const = dic['const']
        const = const[0] if dtype == np.float64 else const[0] + 1j * const[1]

        if 'no_grad_parameters' not in dic:
            raise ValueError("Invalid string. Cannot convert it to ParameterResolver, no key no_grad_parameters")
        no_grad_parameters_list = dic['no_grad_parameters']

        if 'encoder_parameters' not in dic:
            raise ValueError("Invalid string. Cannot convert it to ParameterResolver, no key encoder_parameters")
        encoder_parameters_list = dic['encoder_parameters']

        out = ParameterResolver(pr_data, const)
        out.encoder_part(*encoder_parameters_list)
        out.no_grad_part(*no_grad_parameters_list)
        return out
