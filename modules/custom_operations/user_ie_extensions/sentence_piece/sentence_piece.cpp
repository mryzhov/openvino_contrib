// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "normalizer.h"
#include "sentence_piece.hpp"

#include "openvino/opsets/opset10.hpp"

//#define USE_STRING_TENSORS

#ifdef USE_STRING_TENSORS

// A plugin can support a string tensor on inputs and outputs via the hack which wraps such tensor to
// a u8 tensor holding a pointer to the original string tensor. The hack lets us avoid more deep
// plugin modifications by pre-transform a model where string tensor parameters and results are replaced
// by the described wrapping tensors. Such a hack requires some pre/post processing in operations
// that handle such wrapping tensors on the edge of a model.
#define USE_INPUT_OUTPUT_STRING_TENSOR_HACK

#endif

//#define SENTENCE_PIECE_EXTENSION_DECOMPOSED_STRINGS

using sentencepiece::SentencePieceProcessor;
using namespace TemplateExtension;
using namespace ov;
using namespace ov::frontend;
using namespace ov::opset10;

namespace {
    template<typename T>
    T extract_scalar_const_value(const std::shared_ptr<Node>& node, const std::string& const_name) {
        auto const_node = as_type_ptr<Constant>(node);
        FRONT_END_GENERAL_CHECK(const_node, "Conversion expects " + const_name + " to be constant.");
        std::vector<T> const_value = const_node->cast_vector<T>();
        FRONT_END_GENERAL_CHECK(const_value.size() == 1, "Conversion expects " + const_name + " to be a scalar.");
        return const_value[0];
    }
}  // namespace

SentencepieceTokenizer::SentencepieceTokenizer(const OutputVector& args, int32_t nbest_size, float alpha,
    bool add_bos, bool add_eos, bool reverse) : m_sp(std::make_shared<SentencePieceProcessor>()),
    m_nbest_size(nbest_size), m_alpha(alpha), m_add_bos(add_bos), m_add_eos(add_eos),
    m_reverse(reverse), Op(args) {
    auto sp_model_const = as_type_ptr<Constant>(args[0].get_node_shared_ptr());
    FRONT_END_GENERAL_CHECK(sp_model_const, "SentencepieceTokenizer expects SentencePiece model to be constant.");
    auto spm_model = static_cast<const char*>(sp_model_const->get_data_ptr());
    auto spm_model_size = sp_model_const->get_byte_size();

    // configure SentencePieceProcessor
    std::string model_proto(spm_model, spm_model_size);
    CHECK_OK(m_sp->LoadFromSerializedProto(model_proto));

    // form extra options to configure SentencePieceProcessor
    std::string extra_options = "";
    if (m_add_bos) {
        extra_options += "bos";
    }
    if (m_add_eos) {
        extra_options = extra_options.empty() ? extra_options : extra_options + ":";
        extra_options += "eos";
    }
    /* TODO: TF ignores this option, so we are ignoring it as well; need to understand what should we do
    if (m_reverse) {
        extra_options = extra_options.empty() ? extra_options : extra_options + ":";
        extra_options += "reverse";
    }
    */
    // example of extra_options, if "bos:eos:reverse"
    CHECK_OK(m_sp->SetEncodeExtraOptions(extra_options));
    constructor_validate_and_infer_types();
}

SentencepieceTokenizer::SentencepieceTokenizer(const OutputVector& args, const std::shared_ptr<sentencepiece::SentencePieceProcessor>& sp,
    int32_t nbest_size, float alpha, bool add_bos, bool add_eos, bool reverse) : m_sp(sp),
    m_nbest_size(nbest_size), m_alpha(alpha), m_add_bos(add_bos), m_add_eos(add_eos),
    m_reverse(reverse), Op(args) {
    constructor_validate_and_infer_types();
}

void SentencepieceTokenizer::validate_and_infer_types() {

    #ifdef SENTENCE_PIECE_EXTENSION_DECOMPOSED_STRINGS

    FRONT_END_GENERAL_CHECK(get_input_size() == 1 + 3, "SentencepieceTokenizer expects 4 inputs: sp model and input sentences represented as 3 decomposed tensors (begins, ends, sybols)");
    FRONT_END_GENERAL_CHECK(get_input_element_type(0) == element::u8, "SentencepieceTokenizer accepts sp model as the first input and it should be of type u8 tensor");
    FRONT_END_GENERAL_CHECK(get_input_element_type(1) == element::i32, "SentencepieceTokenizer accepts begins offsets as the second and it should be of type i32 tensor");
    FRONT_END_GENERAL_CHECK(get_input_element_type(2) == element::i32, "SentencepieceTokenizer accepts ends offsets as the third and it should be of type i32 tensor");
    FRONT_END_GENERAL_CHECK(get_input_element_type(3) == element::u8, "SentencepieceTokenizer accepts sentence symbols as the fourth input and it should be of type u8 tensor");

    #else

    FRONT_END_GENERAL_CHECK(get_input_size() == 2, "SentencepieceTokenizer expects two inputs: sp model and input sentences");
    FRONT_END_GENERAL_CHECK(get_input_element_type(0) == element::u8, "SentencepieceTokenizer accepts sp model as the first input and it should be of type u8 tensor");

    #ifdef USE_STRING_TENSORS

        #ifdef USE_INPUT_OUTPUT_STRING_TENSOR_HACK
        FRONT_END_GENERAL_CHECK(
            get_input_element_type(1) == element::string || get_input_element_type(1) == element::u8,
            "SentencepieceTokenizer accepts sentences as the second input and it should be of type u8 or string depending on the current stage of model preparation");
        #else
        FRONT_END_GENERAL_CHECK(
            get_input_element_type(1) == element::string,
            "SentencepieceTokenizer accepts sentences as the second input and it should be of type string tensor");
        #endif

    #else

#if 0   // change to 0 when compiled with master and the bug with data propagation from within inline context is not solved
    FRONT_END_GENERAL_CHECK(
        get_input_element_type(1) == element::u8,
        "SentencepieceTokenizer accepts sentences as the second input and it should be of type u8 tensor, but got " +
            get_input_element_type(1).get_type_name());
#endif

    #endif

    #endif

    // The operation SentencepieceTokenizerExtensionOp has three outputs: sparse indices, sparse values
    // and dense shape
    set_output_type(0, element::i64, PartialShape{ Dimension(), Dimension(2) });
    set_output_type(1, element::i32, PartialShape{ Dimension() });
    set_output_type(2, element::i64, PartialShape{ Dimension(2) });
}

bool SentencepieceTokenizer::visit_attributes(AttributeVisitor& visitor) {
    visitor.on_attribute("nbest_size", m_nbest_size);
    visitor.on_attribute("alpha", m_alpha);
    visitor.on_attribute("add_bos", m_add_bos);
    visitor.on_attribute("add_eos", m_add_eos);
    visitor.on_attribute("reverse", m_reverse);
    return true;
}

void parse_packed_strings (const Tensor& packed, int32_t& batch_size, const int32_t*& begin_ids, const int32_t*& end_ids, const uint8_t*& symbols) {
    auto strings = packed.data<const uint8_t>();
    auto bitstream_size = packed.get_byte_size();
    // check the format of the input bitstream representing the string tensor
    FRONT_END_GENERAL_CHECK(bitstream_size >= 4, "Incorrect packed string tensor format: no batch size in the packed string tensor");
    batch_size = *reinterpret_cast<const int32_t*>(strings + 0);
    FRONT_END_GENERAL_CHECK(bitstream_size >= 4 + 4 + 4 * batch_size,
        "Incorrect packed string tensor format: the packed string tensor must contain first string offset and end indices");
    begin_ids = reinterpret_cast<const int32_t*>(strings + 4);
    end_ids = begin_ids + 1;
    symbols = strings + 4 + 4 + 4 * batch_size;
}

bool SentencepieceTokenizer::evaluate(TensorVector& outputs, const TensorVector& inputs) const {
    std::vector<int64_t> sparse_indices;
    std::vector<int32_t> sparse_values;
    std::vector<int64_t> sparse_dense_shape;

#ifdef SENTENCE_PIECE_EXTENSION_DECOMPOSED_STRINGS

    auto begin_ids = inputs[1].data<const int32_t>();
    auto end_ids = inputs[2].data<const int32_t>();
    auto data = inputs[3].data<const uint8_t>();

    auto batch_size = shape_size(inputs[1].get_shape());

#else

#ifdef USE_STRING_TENSORS

    #ifdef USE_INPUT_OUTPUT_STRING_TENSOR_HACK
    const ov::Tensor& strings_tensor = **reinterpret_cast<ov::Tensor**>(inputs[1].data<uint8_t>());
    #else
    const ov::Tensor& strings_tensor = inputs[1];
    #endif

    const std::string* strings = strings_tensor.data<std::string>();
    size_t batch_size = ov::shape_size(strings_tensor.get_shape());

#else

    // const uint8_t* strings = inputs[1].data<const uint8_t>();
    // auto bitstream_size = inputs[1].get_byte_size();

    // // check the format of the input bitstream representing the string tensor
    // FRONT_END_GENERAL_CHECK(bitstream_size >= 4, "Incorrect packed string tensor format: no batch size in the packed string tensor");
    // auto batch_size = *reinterpret_cast<const int32_t*>(strings + 0);
    // FRONT_END_GENERAL_CHECK(bitstream_size >= 4 + 4 + 4 * batch_size,
    //     "Incorrect packed string tensor format: the packed string tensor must contain first string offset and end indices");
    // auto begin_ids = reinterpret_cast<const int32_t*>(strings + 4);
    // auto end_ids = begin_ids + 1;
    // auto data = strings + 4 + 4 + 4 * batch_size;
    int32_t batch_size;
    const int32_t* begin_ids;
    const int32_t* end_ids;
    const uint8_t* data;
    parse_packed_strings(inputs[1], batch_size, begin_ids, end_ids, data);

#endif

#endif
    //std::cerr << "    Batch size: " << batch_size << "\n";

    size_t max_token_id = 0;
    for (size_t batch_ind = 0; batch_ind < batch_size; ++batch_ind) {
#if defined(USE_STRING_TENSORS) && !defined(SENTENCE_PIECE_EXTENSION_DECOMPOSED_STRINGS)
        const std::string& sentence = strings[batch_ind];
        //std::cerr << "    sentence: " << sentence << "\n";
#else
        auto begin_ind = begin_ids[batch_ind];
        auto end_ind = end_ids[batch_ind];
        //std::string sentence(data + begin_ind, data + end_ind);
        absl::string_view sentence((const char*)data + begin_ind, end_ind - begin_ind);
        //std::cerr << "string: " << sentence << "\n";
#endif
        std::vector<int32_t> ids;
        CHECK_OK(m_sp->SampleEncode(sentence, m_nbest_size, m_alpha, &ids));
        // put into resulted vectors
        for (size_t token_id = 0; token_id < ids.size(); ++token_id) {
            sparse_indices.push_back(static_cast<int64_t>(batch_ind));
            sparse_indices.push_back(static_cast<int64_t>(token_id));
            sparse_values.push_back(static_cast<int32_t>(ids[token_id]));
        }
        max_token_id = max_token_id < ids.size() ? ids.size() : max_token_id;
    }
    sparse_dense_shape.push_back(static_cast<int64_t>(batch_size));
    sparse_dense_shape.push_back(static_cast<int64_t>(max_token_id));

    outputs[0].set_shape({ sparse_indices.size() / 2, 2 });
    memcpy(outputs[0].data(), sparse_indices.data(), sizeof(int64_t) * sparse_indices.size());
    outputs[1].set_shape({ sparse_values.size() });
    memcpy(outputs[1].data(), sparse_values.data(), sizeof(int32_t) * sparse_values.size());
    outputs[2].set_shape({ 2 });
    memcpy(outputs[2].data(), sparse_dense_shape.data(), sizeof(int64_t) * sparse_dense_shape.size());
    return true;
}

bool SentencepieceTokenizer::has_evaluate() const {
    return true;
}

std::shared_ptr<Node> SentencepieceTokenizer::clone_with_new_inputs(const OutputVector& new_args) const {
    return std::make_shared<SentencepieceTokenizer>(new_args, m_sp, m_nbest_size, m_alpha, m_add_bos, m_add_eos, m_reverse);
}

OutputVector translate_sentencepiece_op(const NodeContext& node) {
    // extract model to configure SentencePieceTokenizer
    auto sp_model_ov_any = node.get_attribute_as_any("model");
    FRONT_END_GENERAL_CHECK(sp_model_ov_any.is<std::string>(),
        "SentencePieceOp configuration model is in incorrect format");
    auto str_spm_model = sp_model_ov_any.as<std::string>();
    auto sp_model_const = std::make_shared<Constant>(element::u8, Shape{ str_spm_model.size() }, str_spm_model.data());
    return { sp_model_const };
}




void check_string_input(const Node* node, size_t input_index) {
    FRONT_END_GENERAL_CHECK(node->get_input_element_type(input_index+0) == element::i32, "Expected an i32 tensor as the first part of the decomposed string representation");
    FRONT_END_GENERAL_CHECK(node->get_input_element_type(input_index+1) == element::i32, "Expected an i32 tensor as the second part of the decomposed string representation");
    FRONT_END_GENERAL_CHECK(node->get_input_element_type(input_index+2) == element::u8,  "Expected a u8 tensor as the third part of the decomposed string representation");
}

void check_string_scalar_input(const Node* node, size_t input_index) {
    auto shape = node->get_input_partial_shape(input_index);
    auto element_type = node->get_input_element_type(input_index);

    #ifdef USE_STRING_TENSORS

    OPENVINO_ASSERT(
        (element_type == element::dynamic || element_type == element::string) &&
        (shape.rank().is_dynamic() || shape.rank().get_length() == 0),
        "string/0D tensor is expected");

    #else

    OPENVINO_ASSERT(
        (element_type == element::dynamic || element_type == element::u8) &&
        (shape.rank().is_dynamic() || shape.rank().get_length() == 1),
        "u8/1D tensor is expected");

    #endif
}

void set_string_output(Node* node, size_t output_index, const PartialShape& shape) {
    node->set_output_type(output_index+0, element::i32, shape);
    node->set_output_type(output_index+1, element::i32, shape);
    node->set_output_type(output_index+2, element::u8,  PartialShape{Dimension()});
}


void StringTensorPack::validate_and_infer_types() {
    OPENVINO_ASSERT(m_mode == "begins_ends", "StringTensorPack supporst only 'begins_ends' mode, but get " + m_mode);
    check_string_input(this, 0);
    #ifdef USE_STRING_TENSORS
    set_output_type(0, element::string, get_input_partial_shape(0));
    #else
    set_output_type(0, element::u8, PartialShape{Dimension()});
    #endif
}


bool StringTensorPack::evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const {
#ifdef USE_STRING_TENSORS
    // TODO
    return false;
#else
    auto rank = inputs[0].get_shape().size();
    if (rank != 1) {
        std::cerr << "[ WARNING ] StringTensorPack ignores the rank " << rank << " of input tensor and set rank=1 in the output\n";
    }

    auto num_elements = shape_size(inputs[0].get_shape());
    auto num_chars = shape_size(inputs[2].get_shape());
    auto num_output_elements = 4*(1 + 1 + num_elements) + num_chars;
    outputs[0].set_shape(Shape{num_output_elements});

    //auto begins = inputs[0].data<const int32_t>();    // this is not needed as no repacking happens in this version of code
    auto ends   = inputs[1].data<const int32_t>();
    auto chars  = inputs[2].data<const uint8_t>();

    auto output = outputs[0].data<uint8_t>();
    auto output_int32 = reinterpret_cast<int32_t*>(output);

    *output_int32++ = num_elements;
    *output_int32++ = 0;
    output_int32 = std::copy(ends, ends + num_elements, output_int32);
    output = reinterpret_cast<uint8_t*>(output_int32);
    output = std::copy(chars, chars + num_chars, output);

    OPENVINO_ASSERT(num_output_elements == output - outputs[0].data<uint8_t>(), "[ INTERNAL ERROR ] StringTensorPack output tensor is corrupted");

    // WARNING! Chars are not repacked. If there are gaps between strings, they will remain.

    return true;
#endif
}


void StringTensorUnpack::validate_and_infer_types() {
    OPENVINO_ASSERT(
        get_input_size() == 1,
        "Number of inputs for StringTensorUnpack is not equal to 1");

#if 0 // Uncomment it when the bug is fixed with type substitution in TF partition call inlining
    OPENVINO_ASSERT(
        #ifdef USE_STRING_TENSORS
        get_input_element_type(0) == element::string ||
        #endif
        get_input_element_type(0) == element::dynamic ||
        get_input_element_type(0) == element::u8,
        "Unsupported input element type for StringTensorUnpack: " + get_input_element_type(0).get_type_name());
#endif

    OPENVINO_ASSERT(
        get_input_partial_shape(0).rank().is_static(),
        "StringTensorUnpack supports only static input rank");

#if 0
    // Obtain shape from rt_info.
    auto& rt_info = get_input_node_shared_ptr(0)->get_rt_info();
    auto ops = rt_info.find("original_partial_shape");
    if(ops != rt_info.end()) {
        input_shape = ops->second.as<PartialShape>();
        std::cerr << "StringTensorUnpack: orig_partial_shape: " << input_shape << "\n";
    } else {
        std::cerr << "Impossible\n";
        std::cerr << get_input_node_shared_ptr(0) << "\n";
    }
#endif

    auto output_shape = PartialShape::dynamic();

#ifdef USE_STRING_TENSORS

    // In case of explicit string tensors the shape is carried by input tensor itself
    // OPENVINO_ASSERT(
    //     input_shape == PartialShape::dynamic(),
    //     "Excplicitly set shape for a string tensor in the unpacking is not supported");

    #ifdef USE_INPUT_OUTPUT_STRING_TENSOR_HACK

    // There are two cases that affect expected element type of the input tensor:
    // before the hack is applied (element::string) and after it (element::u8).

    OPENVINO_ASSERT(
        get_input_element_type(0) == element::string
        || get_input_element_type(0) == element::u8,
        "Type of StringTensorUnpack input is expected to be element::string before a model compilation or element::u8 after the compilation");

    if(get_input_element_type(0) == element::string) {
        output_shape = get_input_partial_shape(0);
    }

    if(get_input_element_type(0) == element::u8)
    {
        // After the plugin hack, a tensor is represented as a wrapping u8 tensor that will hold a pointer to a string tensor.
        // The original shape of a string tensor is stored in RT attribute of a tensor descriptor.
        const auto& rt_info = get_input_tensor(0).get_rt_info();
        auto it = rt_info.find("__original_partial_shape");

        // StringTensorUnpack expects __original_partial_shape attribute of type PartialShape in the input tensor.
        // If it is not found that means that model compilation wasn't pass the expected transformation where a string tensor
        // is wrapped to a u8 tensor holding a pointer, or because evaluation of this node is in progress and tensor attributes aren't preserved.
        if(it != rt_info.end() && it->second.is<PartialShape>()) {
            output_shape = it->second.as<PartialShape>();
        }
    }

    #else

    OPENVINO_ASSERT(
        get_input_element_type(0) == element::string,
        "StringTensorUnpack expects element::string in an input tensor, but it is " + std::string(get_input_element_type(0)));

    output_shape = get_input_partial_shape(0);

    #endif

#else
    // Expect packed string tensor represenation which can carry only a string tensors of shape [?]
    // Shape is not known in advance and only rank of the output can be set

    OPENVINO_ASSERT(
#if 0 // Uncomment it when the bug is fixed with type substitution in TF partition call inlining
        get_input_element_type(0) == element::u8 &&
#endif
        get_input_partial_shape(0).rank().is_static() && get_input_partial_shape(0).rank().get_length() == 1,
        "StringTensorUnpack expects a u8 tensor with rank 1 that holds packed batched string tensor as an input, but observes type " +
            get_input_element_type(0).get_type_name() + " and shape " + get_input_partial_shape(0).to_string());

    output_shape = PartialShape({Dimension()});  // [?]

    #if 0

    if(get_input_element_type(0) == element::u8) {
        if(all_inputs_are_constants(this)) {
            std::cerr << "StringTensorUnpack: u8/const\n";
            // HACK: Tensor of strings is passed by a raw pointer to a tensor
            auto constant = std::dynamic_pointer_cast<ov::opset1::Constant>(get_input_node_shared_ptr(0));
            size_t raw_size = constant->get_shape()[0];
            if(raw_size == 0) {
                // means empty input
                std::cerr << "StringTensorUnpack: empty\n";
                data = nullptr;
                input_shape = PartialShape({0});
            } else if(raw_size == sizeof(void*)) {
                std::cerr << "StringTensorUnpack: not empty, tensor HACK\n";
                auto tensor = *reinterpret_cast<const ov::Tensor* const *>(constant->get_data_ptr<uint8_t>());
                std::cerr << "Pointer to tensor from op: " << tensor << "\n";
                input_shape = tensor->get_shape();
                data = tensor->data<std::string>();
            } else {

                OPENVINO_ASSERT(
                    false,
                    "Unexpected size for hacked Tensor<string> input. Something went wrong.");
            }
        } else {
            std::cerr << "StringTensorUnpack: u8/not constant\n";
        }
    } else {
        std::cerr << "StringTensorUnpack: string\n";
        input_shape = get_input_partial_shape(0);
        if(all_inputs_are_constants(this)) {
            auto constant = std::dynamic_pointer_cast<ov::opset1::Constant>(get_input_node_shared_ptr(0));
            data = constant->get_data_ptr<std::string>();
        } else {
            input_shape = get_input_partial_shape(0);
        }
    }

    #endif

#endif

    OPENVINO_ASSERT(m_mode == "begins_ends", "StringTensorUnpack supporst only 'begins_ends' mode, but get " + m_mode);

    if (m_mode == "begins_ends") {
        set_string_output(this, 0, output_shape);
    }
}

bool StringTensorUnpack::evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const {

#ifdef USE_STRING_TENSORS

    #ifdef USE_INPUT_OUTPUT_STRING_TENSOR_HACK
    auto tensor = *reinterpret_cast<const ov::Tensor* const *>(inputs[0].data<uint8_t>());
    #else
    auto tensor = inputs[0];
    #endif

    //std::cerr << "Pointer to tensor from op evaluate: " << tensor << "\n";
    Shape input_shape = tensor->get_shape();
    const std::string* input_strings = tensor->data<std::string>();
    std::cerr << "input_shape = " << input_shape << "\n";
    //std::cerr << data << "\n";

    auto nelements = shape_size(input_shape);
    size_t total = 0;
    for(size_t i = 0; i < nelements; ++i)
        total += input_strings[i].length();

    outputs[0].set_shape(input_shape);
    outputs[1].set_shape(input_shape);
    outputs[2].set_shape(Shape{total});

    auto begins = outputs[0].data<int32_t>();
    auto ends = outputs[1].data<int32_t>();
    auto output_symbols = reinterpret_cast<char*>(outputs[2].data<uint8_t>());
    size_t offset = 0;

    for(size_t i = 0; i < nelements; ++i)
    {
        begins[i] = offset;
        output_symbols = std::copy(input_strings[i].begin(), input_strings[i].end(), output_symbols);
        offset += input_strings[i].length();
        ends[i] = offset;
    }

    return true;

#else

    int32_t batch_size;
    const int32_t* begin_ids;
    const int32_t* end_ids;
    const uint8_t* data;
    parse_packed_strings(inputs[0], batch_size, begin_ids, end_ids, data);

    auto num_chars = end_ids[batch_size - 1];

    outputs[0].set_shape(Shape{static_cast<unsigned long>(batch_size)});
    outputs[1].set_shape(Shape{static_cast<unsigned long>(batch_size)});
    outputs[2].set_shape(Shape{static_cast<unsigned long>(num_chars)});

    auto begins = outputs[0].data<int32_t>();
    auto ends = outputs[1].data<int32_t>();
    auto chars = outputs[2].data<uint8_t>();

    std::copy(begin_ids, begin_ids + batch_size, begins);
    std::copy(end_ids, end_ids + batch_size, ends);
    std::copy(data, data + num_chars, chars);

    return true;

#endif
}


OutputVector pre_translate_string_tensor_input(const NodeContext& node, size_t input_index) {
    auto input = node.get_input(input_index);
    auto input_node = input.get_node_shared_ptr();

#ifndef USE_STRING_TENSORS
    // Override type of input tensor if this is a Parameter
    if (auto parameter = std::dynamic_pointer_cast<Parameter>(input_node)) {
        // TODO: Apply this change conditionally based on real Parameter value
        std::cerr << "Overriding Parameter element_type to U8 to be ready to accept a packed batch of strings\n";
        parameter->set_partial_shape(PartialShape{ Dimension() });
        parameter->set_element_type(element::u8);
        parameter->validate_and_infer_types();
    }
#endif

    if (auto struct_pack = std::dynamic_pointer_cast<StringTensorPack>(input_node)) {
        FRONT_END_GENERAL_CHECK(struct_pack->get_input_size() == 3, "Expected 3 inputs to StringTensorPack which represents a string tensor");
        return struct_pack->input_values();
    } else {
        #if defined(USE_STRING_TENSORS) || true     // always
        return std::make_shared<StringTensorUnpack>(OutputVector{input}, "begins_ends")->outputs();
        #else
        // Suppose this is u8 packed string tensor with a single batch dimension
        // Unpack this tensor using standard operations

        // Cannot do that because there is not ReinterprectCast operation in OV
        // TODO: Find a way to make it without reinterpretation operation
        #endif
    }
}

ov::Output<ov::Node> post_translate_string_tensor_output(const OutputVector& outputs) {
    FRONT_END_GENERAL_CHECK(outputs.size() == 3, "Expected 3 tensors in decomposed string tensor representation");
    return std::make_shared<StringTensorPack>(outputs, "begins_ends");
}

NamedOutputVector translate_sentencepiece_tokenizer(const NodeContext& node) {
    // this is custom translator that converts a sub-graph with SentencePieceOp, SentencePieceTokenizer,
    // and RaggedTensorToSparse operation- into a custom operation SentencepieceTokenizerExtensionOp
    FRONT_END_GENERAL_CHECK(node.get_input_size() > 0, "RaggedTensorToSparse expects at least one input.");
    auto node_name = node.get_name();

    // check that producers of RaggedTensorToSparse is SentencePieceTokenizer
    auto sp_tokenize_op = node.get_input(0).get_node_shared_ptr();
    FRONT_END_GENERAL_CHECK(sp_tokenize_op->get_input_size() > 6,
        "SentencepieceTokenizeOp expects at least six inputs");

    // prepare inputs that go to custom operation
    // prepare input 0 - SentencePieceTokenizer configuration model
    auto sp_model_const = as_type_ptr<Constant>(sp_tokenize_op->input_value(0).get_node_shared_ptr());
    FRONT_END_GENERAL_CHECK(sp_model_const, "Conversion expects SentencePiece model to be constant.");

    // prepare input six inputs
    auto inputs = sp_tokenize_op->input_value(1);

    // extract values for nbest_size, alpha, add_bos, add_eos, reverse attributes
    auto nbest_size = extract_scalar_const_value<int32_t>(sp_tokenize_op->input_value(2).get_node_shared_ptr(), "nbest_size");
    auto alpha = extract_scalar_const_value<float>(sp_tokenize_op->input_value(3).get_node_shared_ptr(), "alpha");
    auto add_bos = extract_scalar_const_value<bool>(sp_tokenize_op->input_value(4).get_node_shared_ptr(), "add_bos");
    auto add_eos = extract_scalar_const_value<bool>(sp_tokenize_op->input_value(5).get_node_shared_ptr(), "add_eos");
    auto reverse = extract_scalar_const_value<bool>(sp_tokenize_op->input_value(6).get_node_shared_ptr(), "reverse");

#ifndef USE_STRING_TENSORS
    // Override type of input tensor if this is a Parameter
    if (auto parameter = std::dynamic_pointer_cast<Parameter>(inputs.get_node_shared_ptr())) {
        std::cerr << "HERE\n";
        parameter->set_partial_shape(PartialShape{ Dimension() });
        parameter->set_element_type(element::u8);
        parameter->validate_and_infer_types();
    }
#endif

#ifdef SENTENCE_PIECE_EXTENSION_DECOMPOSED_STRINGS

    OutputVector inputs_vector = OutputVector{ sp_model_const };
    auto unpacked_outputs = std::make_shared<StringTensorUnpack>(OutputVector{inputs}, "begins_ends")->outputs();
    inputs_vector.insert(inputs_vector.end(), unpacked_outputs.begin(), unpacked_outputs.end());

#else

    OutputVector inputs_vector = OutputVector{ sp_model_const, inputs };

#endif

    // create a node with custom operation
    auto sp_tokenizer_ext = std::make_shared<SentencepieceTokenizer>(inputs_vector, nbest_size, alpha, add_bos, add_eos, reverse);
    FRONT_END_GENERAL_CHECK(sp_tokenizer_ext->get_output_size() == 3,
        "Internal error: SentencepieceTokenizer operation extension must have three outputs.");

    // set tensor names
    sp_tokenizer_ext->output(0).add_names({ node_name + ":0" });
    sp_tokenizer_ext->output(1).add_names({ node_name + ":1" });
    sp_tokenizer_ext->output(2).add_names({ node_name + ":2" });

    // create named outputs for the conversion extension
    NamedOutputVector named_results;
    named_results.push_back({ "sparse_indices", sp_tokenizer_ext->output(0) });
    named_results.push_back({ "sparse_values", sp_tokenizer_ext->output(1) });
    named_results.push_back({ "sparse_dense_shape", sp_tokenizer_ext->output(2) });

    return named_results;
}


void CaseFold::validate_and_infer_types() {
    check_string_input(this, 0);
    set_string_output(this, 0, get_input_partial_shape(0));
}

bool CaseFold::evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const {
    auto begins = inputs[0].data<const int32_t>();
    auto ends   = inputs[1].data<const int32_t>();
    auto chars  = inputs[2].data<const uint8_t>();

    // Stub implementation that transforms each input string "X" to "CaseFold(X)" for debugging purposes
    {
        // Set output shapes
        outputs[0].set_shape(inputs[0].get_shape());
        outputs[1].set_shape(inputs[1].get_shape());
        const std::string left_side = "CaseFold(", right_side = ")";
        const size_t num_elements = inputs[0].get_size();
        const size_t new_len = inputs[2].get_size() + (left_side.length() + right_side.length())*num_elements;
        outputs[2].set_shape(Shape{new_len});

        // For the whole implementation below the input shapes can be ignored, we are working with the flatten representaions
        // and only number of elements in the original tensors matter

        // Get pointers in the output tensors
        auto new_begins = outputs[0].data<int32_t>();
        auto new_ends   = outputs[1].data<int32_t>();
        auto new_chars  = outputs[2].data<uint8_t>();
        int32_t char_offset = 0;

        for(size_t i = 0; i < num_elements; ++i) {
            new_begins[i] = char_offset;
            std::string new_str = left_side + std::string(chars + begins[i], chars + ends[i]) + right_side;
            std::copy(new_str.data(), new_str.data() + new_str.length(), new_chars + char_offset);
            char_offset += new_str.length();
            new_ends[i] = char_offset;
        }
        return true;
    }
    // End of stub implementation
}


ov::OutputVector translate_case_fold_utf8(const ov::frontend::NodeContext& node) {
    std::cerr << "translate_case_fold_utf8\n";
    FRONT_END_GENERAL_CHECK(node.get_input_size() == 1, "CaseFold expects only 1 input");
    return { post_translate_string_tensor_output(std::make_shared<CaseFold>(
        pre_translate_string_tensor_input(node, 0))->outputs()) };
}



void NormalizeUnicode::validate_and_infer_types() {
    check_string_input(this, 0);
    set_string_output(this, 0, get_input_partial_shape(0));
}

bool NormalizeUnicode::evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const {
    auto begins = inputs[0].data<const int32_t>();
    auto ends   = inputs[1].data<const int32_t>();
    auto chars  = inputs[2].data<const uint8_t>();

#if 0
    // TODO: Complete implementation
#else
    // Stub implementation that transforms each input string "X" to "NormalizeUnicode(X, normalization_form)" for debugging purposes
    {
        // Set output shapes
        outputs[0].set_shape(inputs[0].get_shape());
        outputs[1].set_shape(inputs[1].get_shape());
        const std::string left_side = "NormalizeUnicode(", right_side = ")", delimeter = ", ";
        const size_t num_elements = inputs[0].get_size();
        const size_t new_len = inputs[2].get_size() + (left_side.length() + right_side.length() + delimeter.length() + m_normalization_form.length())*num_elements;
        outputs[2].set_shape(Shape{new_len});

        // For the whole implementation below the input shapes can be ignored, we are working with the flatten representaions
        // and only number of elements in the original tensors matter

        // Get pointers in the output tensors
        auto new_begins = outputs[0].data<int32_t>();
        auto new_ends   = outputs[1].data<int32_t>();
        auto new_chars  = outputs[2].data<uint8_t>();
        int32_t char_offset = 0;

        for(size_t i = 0; i < num_elements; ++i) {
            new_begins[i] = char_offset;
            std::string new_str = left_side + std::string(chars + begins[i], chars + ends[i]) + delimeter + m_normalization_form + right_side;
            std::copy(new_str.data(), new_str.data() + new_str.length(), new_chars + char_offset);
            char_offset += new_str.length();
            new_ends[i] = char_offset;
        }
        return true;
    }
    // End of stub implementation
#endif
}


ov::OutputVector translate_normalize_utf8(const ov::frontend::NodeContext& node) {
    FRONT_END_GENERAL_CHECK(node.get_input_size() == 1, "NormalizeUTF8 expects only 1 input");
    return { post_translate_string_tensor_output(std::make_shared<NormalizeUnicode>(
        pre_translate_string_tensor_input(node, 0),
        node.get_attribute<std::string>("normalization_form"))->outputs()) };
}




void RegexNormalization::validate_and_infer_types() {
    check_string_input(this, 0);
    check_string_scalar_input(this, 3);
    check_string_scalar_input(this, 4);
    set_string_output(this, 0, get_input_partial_shape(0));
}

bool RegexNormalization::evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const {
    auto begins = inputs[0].data<const int32_t>();
    auto ends   = inputs[1].data<const int32_t>();
    auto chars  = inputs[2].data<const uint8_t>();

#ifdef USE_STRING_TENSORS
    auto search_pattern  = *inputs[3].data<const std::string>();
    auto replace_pattern  = *inputs[4].data<const std::string>();
#else
    auto search_pattern_buf  = inputs[3].data<const uint8_t>();
    auto replace_pattern_buf  = inputs[4].data<const uint8_t>();
    auto search_pattern = absl::string_view((const char*)search_pattern_buf, shape_size(inputs[3].get_shape()) - 1);   // FIXME: -1 is a complementary change to a WA applied in string_attribute_to_constant
    auto replace_pattern = absl::string_view((const char*)replace_pattern_buf, shape_size(inputs[4].get_shape()) - 1);   // FIXME: -1 is a complementary change to a WA applied in string_attribute_to_constant
#endif

#if 0
    // TODO: Complete implementation
#else
    // Stub implementation that transforms each input string "X" to "RegexNormalization(X, search_pattern, replace_pattern)" for debugging purposes
    {
        // Set output shapes
        outputs[0].set_shape(inputs[0].get_shape());
        outputs[1].set_shape(inputs[1].get_shape());
        const std::string left_side = "RegexNormalization(", right_side = ")", delimeter = ", ";
        const size_t num_elements = inputs[0].get_size();
        const size_t new_len = inputs[2].get_size() + (left_side.length() + right_side.length() + 2*delimeter.length() + search_pattern.length() + replace_pattern.length())*num_elements;
        outputs[2].set_shape(Shape{new_len});

        // For the whole implementation below the input shapes can be ignored, we are working with the flatten representaions
        // and only number of elements in the original tensors matter

        // Get pointers in the output tensors
        auto new_begins = outputs[0].data<int32_t>();
        auto new_ends   = outputs[1].data<int32_t>();
        auto new_chars  = outputs[2].data<uint8_t>();
        int32_t char_offset = 0;

        for(size_t i = 0; i < num_elements; ++i) {
            new_begins[i] = char_offset;

            std::string new_str =
                left_side + std::string(chars + begins[i], chars + ends[i]) + delimeter +
                std::string(search_pattern) + delimeter +
                std::string(replace_pattern) + right_side;

            std::copy(new_str.data(), new_str.data() + new_str.length(), new_chars + char_offset);
            char_offset += new_str.length();
            new_ends[i] = char_offset;
        }
        return true;
    }
    // End of stub implementation
#endif
}


std::shared_ptr<Node> string_attribute_to_constant (const ov::frontend::NodeContext& node, const std::string& name) {
    // FIXME: using space to pad the value to work-around CPU issue with empty constants
    auto value = node.get_attribute<std::string>(name) + " ";

    #ifdef USE_STRING_TENSORS
    return std::make_shared<Constant>(element::string, {}, value);
    #else
    return std::make_shared<Constant>(element::u8, Shape{value.length()}, (const void*)value.data());
    #endif
}


ov::OutputVector translate_static_regex_replace(const ov::frontend::NodeContext& node) {
    FRONT_END_GENERAL_CHECK(node.get_input_size() == 1, "StaticRegexReplace expects only 1 input");
    ov::OutputVector inputs = pre_translate_string_tensor_input(node, 0);
    inputs.push_back(string_attribute_to_constant(node, "pattern"));
    inputs.push_back(string_attribute_to_constant(node, "rewrite"));
    return { post_translate_string_tensor_output(std::make_shared<RegexNormalization>(inputs)->outputs()) };
}


ov::OutputVector translate_reshape(const ov::frontend::NodeContext& node) {
    // This is a copied-and-pasted and adopted fragment of TF reshape translator from OV.
    // It checks if the input tensor has string type, and then perform custom tranlation.
    // Otherwise it should operate identically to the stock version of Reshape translator in TF FE.
    // TODO: Introduce an API to call original translators from an extension without copying the code to an extension.

    FRONT_END_GENERAL_CHECK(node.get_input_size() == 2, "Tensorflow Reshape op should have two inputs");
    auto tensor = node.get_input(0);
    auto shape = node.get_input(1);
    if(auto pack = dynamic_cast<StringTensorPack*>(tensor.get_node())) {
        // TODO: If it is a beginning of the graph, how to detect strings? It falls in 'else' branch in this case.
        // FIXME: Needs extension for a Parameter to prepare it first
        auto begins = std::make_shared<Reshape>(pack->input_value(0), shape, false);
        auto ends = std::make_shared<Reshape>(pack->input_value(1), shape, false);
        auto chars = pack->input_value(2);

        auto reshape = post_translate_string_tensor_output({begins, ends, chars});

        return {reshape};
    } else {
        auto reshape = std::make_shared<Reshape>(tensor, shape, false);
        return {reshape};
    }
    // set_node_name(node.get_name(), reshape); // TODO: requires dependencies from TF FE internals
}

