# -*- coding: utf-8 -*-
# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
import os
import sys
import openvino
from openvino.runtime.utils.node_factory import NodeFactory

from .convert_tokenizer import convert_tokenizer
from .node_factory import init_extension
from .str_pack import pack_strings, unpack_strings
from .utils import add_greedy_decoding, connect_models

_ext_libs_path = os.path.join(os.path.dirname(__file__), "libs")
_ext_path = os.path.join(_ext_libs_path, "libuser_ov_extensions.so")

if sys.platform == "win32":
    _ext_path = os.path.join(_ext_libs_path, "libuser_ov_extensions.dll")
    if os.path.isdir(_ext_libs_path):
        # On Windows, with Python >= 3.8, DLLs are no longer imported from the PATH.
        os.add_dll_directory(os.path.abspath(_ext_path))
    else:
        sys.exit(f'Error: extention libriary path {_ext_libs_path} not found')
elif sys.platform == "darwin":
    _ext_path = os.path.join(_ext_libs_path, "libuser_ov_extensions.dylib")

old_core_init = openvino.runtime.Core.__init__
def new_core_init(self, *k, **kw):
    old_core_init(self, *k, **kw)
    self.add_extension(_ext_path)
openvino.runtime.Core.__init__ = new_core_init

_factory = NodeFactory()
_factory.add_extension(_ext_path)
