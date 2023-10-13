# -*- coding: utf-8 -*-
# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import logging
from typing import Dict, Optional, Sequence, Tuple, Union

from constants import GREEDY_DECODER_NAME, LOGITS_OUTPUT_NAME, TOKEN_IDS_OUTPUT_NAME, DecodingType
from openvino import Model, PartialShape, Type
from openvino.runtime import op
from openvino.runtime import opset12 as opset


logger = logging.getLogger(__name__)


def get_greedy_decoding_ov_model() -> Model:
    logits = op.Parameter(Type.i32, PartialShape(["?", "?", "?"]))
    logits.set_friendly_name(LOGITS_OUTPUT_NAME)
    argmax = opset.topk(
        data=logits,
        k=1,
        axis=-1,
        mode="max",
        sort="none",
        name="ArgMax",
    )
    token_ids = opset.squeeze(
        data=argmax.output(1),
        axes=-1,
    )
    token_ids.output(0).tensor.add_names({TOKEN_IDS_OUTPUT_NAME})
    return Model(token_ids.outputs(), [logits], name=GREEDY_DECODER_NAME)


def connect_models(
    first: Model,
    second: Model,
    name_map: Optional[Union[Sequence[Tuple[str, str]], Dict[str, str]]] = None,
    by_indices: bool = False,
    keep_second_model_unaligned_inputs: bool = True,
    keep_remaining_first_model_outputs: bool = False,
) -> Model:
    if by_indices:
        min_len = min(len(first.outputs), len(second.inputs))
        aligned_first_outputs = first.outputs[:min_len]
        aligned_second_inputs = second.inputs[:min_len]
    elif name_map is None:
        aligned_first_outputs = first.outputs
        aligned_second_inputs = [second.input(model1_output.get_any_name()) for model1_output in aligned_first_outputs]
    else:
        if isinstance(name_map, dict):
            name_map = list(name_map.items())
        aligned_first_outputs = [first.output(name1) for name1, _ in name_map]
        aligned_second_inputs = [second.input(name2) for _, name2 in name_map]

    for second_input, first_output in zip(aligned_second_inputs, aligned_first_outputs):
        logger.debug(f"Connecting: {first_output.get_any_name()} -> {second_input.get_any_name()}")
        for target in second_input.get_target_inputs():
            target.replace_source_output(first_output.get_node().input_value(0))
            # target.replace_source_output(model1_output)  # TODO: Produces incorrect topology

    new_inputs = first.get_parameters()
    remaining_inputs = [input_ for input_ in second.inputs if input_ not in aligned_second_inputs]
    if keep_second_model_unaligned_inputs:
        new_inputs.extend(remaining_inputs)
    elif remaining_inputs:
        logger.info(
            "Some inputs of the second model were left uncovered and not included in the connected model: "
            + ", ".join(input_.name for input_ in remaining_inputs)
            + ". To add them set `keep_unaligned_inputs` to `True`"
        )

    new_outputs = second.outputs
    remaining_outputs = [output for output in first.outputs if output not in aligned_first_outputs]
    if keep_remaining_first_model_outputs:
        new_outputs.extend(remaining_outputs)
    elif remaining_outputs:
        logger.info(
            "Some outputs of the first model were left uncovered and not included in the connected model: "
            + ", ".join(output.name for output in remaining_outputs)
            + ". To add them set `keep_unaligned_outputs` to `True`"
        )

    connected_model = Model(new_outputs, new_inputs, f"{first.get_name()}_with_{second.get_name()}")
    # TODO: Cleanup model1 and mode2 to avoid using them, they are ill-formed after the reconnection
    connected_model.validate_nodes_and_infer_types()
    return connected_model


def add_greedy_decoding(text_generation_model: Model, logits_output: str = LOGITS_OUTPUT_NAME) -> Model:
    return connect_models(
        first=text_generation_model,
        second=get_greedy_decoding_ov_model(),
        name_map={logits_output: LOGITS_OUTPUT_NAME},
        keep_second_model_unaligned_inputs=True,
        keep_remaining_first_model_outputs=True,
    )


class Generator:
    def __init__(self, generation_model: Model, decoding_type: DecodingType = DecodingType.greedy) -> None:
        pass