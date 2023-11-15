# -*- coding: utf-8 -*-
# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
import os
import openvino
from openvino.runtime.utils.node_factory import NodeFactory

from .convert_tokenizer import convert_tokenizer
from .node_factory import init_extension
from .str_pack import pack_strings, unpack_strings
from .utils import add_greedy_decoding, connect_models

_extension_path = os.path.join(os.path.dirname(__file__), "libs", "libuser_ov_extensions.so")

old_core_init = openvino.runtime.Core.__init__
def new_core_init(self, *k, **kw):
    old_core_init(self, *k, **kw)
    self.add_extension(_extension_path)
openvino.runtime.Core.__init__ = new_core_init

_factory = NodeFactory()
_factory.add_extension(_extension_path)
