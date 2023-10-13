// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <openvino/op/op.hpp>

// Having a decomposed representation for a tensor, converts it to a single string tensor
// (packed u8 or natively supported element::string depending on whether or not USE_STRING_TENSORS defined).
class StringTensorPack : public ov::op::Op {
public:
    OPENVINO_OP("StringTensorPack");

    StringTensorPack () = default;

    StringTensorPack(ov::OutputVector inputs, const std::string& mode = "begins_ends")
        : ov::op::Op(inputs), m_mode(mode) {
        constructor_validate_and_infer_types();
    }

    void validate_and_infer_types() override;

    std::shared_ptr<ov::Node> clone_with_new_inputs(const ov::OutputVector& inputs) const override {
        auto result = std::make_shared<StringTensorPack>(inputs, m_mode);
        return result;
    }

    bool visit_attributes(ov::AttributeVisitor& visitor) override {
        visitor.on_attribute("mode", m_mode);
        return true;
    }

    bool has_evaluate() const {
        return true;
    }

    bool evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const;

private:

    std::string m_mode = "begins_ends";
};