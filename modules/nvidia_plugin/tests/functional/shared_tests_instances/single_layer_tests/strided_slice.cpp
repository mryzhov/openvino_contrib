// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "single_layer_tests/strided_slice.hpp"

#include "common_test_utils/test_constants.hpp"
#include "cuda_test_constants.hpp"
#include "transformations/convert_precision.hpp"

using namespace LayerTestsDefinitions;

namespace {

std::initializer_list<StridedSliceSpecificParams> ss_only_test_cases_fp32 = {
    {{2, 2, 2, 2}, {0, 0, 0, 0}, {2, 2, 2, 2}, {1, 1, 1, 1}, {}, {}, {}, {}, {}},
    {{2, 2, 4, 3},
     {0, 0, 0, 0},
     {2, 2, 4, 3},
     {1, 1, 2, 1},
     {1, 0, 0, 0},
     {1, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 1, 0, 0}},
    {{2, 2, 2, 2},
     {1, 1, 1, 1},
     {2, 2, 2, 2},
     {1, 1, 1, 1},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{1, 3, 4, 2},
     {0, 0, -2, 0},
     {1, -2, 4, -1},
     {1, 1, 1, 1},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{2, 2, 4, 5},
     {0, 0, 0, 2},
     {1, 2, 3, 4},
     {1, 1, 1, 1},
     {0, 0, 0, 0},
     {1, 1, 1, 1},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{1, 12, 100}, {0, -1, 0}, {0, 0, 0}, {1, 1, 1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 1, 0}, {0, -1, 0}, {1, 1, 1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 7, 0}, {0, 9, 0}, {-1, 1, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 4, 0}, {0, 10, 0}, {-1, 2, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 11, 0}, {0, 0, 0}, {-1, -2, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{10, 12}, {-1, 1}, {-9999, 0}, {-1, 1}, {0, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}},
    {{2, 2, 2, 2}, {1, 1, 1, 1}, {2, 2, 2, 2}, {1, 1, 1, 1}, {0, 0, 0, 0}, {1, 1, 1, 1}, {}, {}, {}},
    {{1, 12, 100, 10}, {0, 0, 0}, {1, 12, 100}, {1, 1, 1}, {}, {}, {}, {}, {}},
    {{2, 2, 4, 2}, {1, 0, 0, 1}, {2, 2, 4, 2}, {1, 1, 2, 1}, {0, 1, 1, 0}, {1, 1, 0, 0}, {}, {}, {}},
    {{1, 2}, {0, 0}, {1, 2}, {1, -1}, {1, 1}, {1, 1}, {}, {}, {}},
    {{2, 2, 4, 2}, {1, 0, 0, 0}, {1, 2, 4, 2}, {1, 1, -2, -1}, {0, 1, 1, 1}, {1, 1, 1, 1}, {}, {}, {}},
};

std::initializer_list<StridedSliceSpecificParams> ss_only_test_cases_fp16 = {
    {{2, 2, 2, 2},
     {0, 0, 0, 0},
     {2, 2, 2, 2},
     {1, 1, 1, 1},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{2, 2, 4, 2},
     {1, 0, 0, 1},
     {2, 2, 4, 2},
     {1, 2, 2, 2},
     {0, 0, 1, 0},
     {0, 1, 0, 0},
     {1, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{2, 2, 2, 2}, {0, 0, 0, 0}, {2, 2, 2, 2}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {}, {}, {}},
    {{2, 2, 4, 5},
     {0, 0, 0, 2},
     {1, 2, 3, 4},
     {1, 1, 1, 1},
     {0, 0, 0, 1},
     {1, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{1, 4, 1, 1}, {0, 3, 0, 0}, {1, 1, 1, 1}, {1, -1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {}, {0, 0, 0, 0}, {}},
    {{1, 12, 100}, {0, 9, 0}, {0, 11, 0}, {1, 1, 1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 9, 0}, {0, 7, 0}, {-1, -1, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 4, 0}, {0, 9, 0}, {-1, 2, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 9, 0}, {0, 4, 0}, {-1, -2, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, 10, 0}, {0, 4, 0}, {-1, -2, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{1, 12, 100}, {0, -6, 0}, {0, -8, 0}, {-1, -2, -1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{5, 5, 5, 5},
     {-1, 0, -1, 0},
     {-50, 0, -60, 0},
     {-1, 1, -1, 1},
     {0, 0, 0, 0},
     {0, 1, 0, 1},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{2, 2, 2, 2}, {1, 1, 1, 1}, {2, 2, 2, 2}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {}, {}, {}},
    {{2, 2, 4, 3}, {0, 0, 0, 0}, {2, 2, 4, 3}, {1, 1, 2, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {}, {}, {}},
    {{1, 12, 100, 10}, {0, -1, 0}, {0, 0, 0}, {1, 1, 1}, {1, 0, 1}, {1, 0, 1}, {}, {0, 1, 0}, {}},
    {{1, 2, 4, 2}, {1, 0, 0, 0}, {1, 2, 4, 2}, {1, 1, -2, -1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {}, {}, {}},
};

std::initializer_list<StridedSliceSpecificParams> ss_only_test_cases_i32 = {
    {{4}, {0}, {1}, {1}, {0}, {0}, {0}, {1}, {0}},
    {{4}, {1}, {2}, {1}, {0}, {0}, {0}, {1}, {0}},
    {{4}, {2}, {3}, {1}, {0}, {0}, {0}, {1}, {0}},
    {{4}, {2}, {4}, {1}, {0}, {1}, {0}, {0}, {0}},
};

std::initializer_list<StridedSliceSpecificParams> smoke_test_cases_fp32 = {
    {{2, 2, 2, 2}, {0, 0, 0, 0}, {2, 2, 2, 2}, {1, 1, 1, 1}, {}, {}, {}, {}, {}},
    {{1, 3, 4, 2},
     {0, 0, -2, 0},
     {1, -2, 4, -1},
     {1, 1, 1, 1},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0},
     {0, 0, 0, 0}},
    {{1, 2}, {0, 0}, {1, 2}, {1, -1}, {1, 1}, {1, 1}, {}, {}, {}},
    {{2, 2, 4, 2}, {1, 0, 0, 0}, {1, 2, 4, 2}, {1, 1, -2, -1}, {0, 1, 1, 1}, {1, 1, 1, 1}, {}, {}, {}},
};

INSTANTIATE_TEST_CASE_P(StridedSliceTest_FP32,
                        StridedSliceLayerTest,
                        ::testing::Combine(::testing::ValuesIn(ss_only_test_cases_fp32),
                                           ::testing::Values(InferenceEngine::Precision::FP32),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(StridedSliceTest_FP16,
                        StridedSliceLayerTest,
                        ::testing::Combine(::testing::ValuesIn(ss_only_test_cases_fp16),
                                           ::testing::Values(InferenceEngine::Precision::FP16),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(StridedSliceTest_I32,
                        StridedSliceLayerTest,
                        ::testing::Combine(::testing::ValuesIn(ss_only_test_cases_i32),
                                           ::testing::Values(InferenceEngine::Precision::I32),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(smoke_StridedSliceTest_FP32,
                        StridedSliceLayerTest,
                        ::testing::Combine(::testing::ValuesIn(smoke_test_cases_fp32),
                                           ::testing::Values(InferenceEngine::Precision::FP32),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerTest::getTestCaseName);

class StridedSliceLayerI32Test : public StridedSliceLayerTest {
public:
    void SetUp() override {
        StridedSliceLayerTest::SetUp();

        ov::pass::Manager manager;
        manager.register_pass<ov::pass::ConvertPrecision>(ov::element::i64, ov::element::i32);
        manager.run_passes(function);
    }
};

TEST_P(StridedSliceLayerI32Test, CompareWithRefs) { Run(); }

INSTANTIATE_TEST_CASE_P(StridedSliceTest_FP32_I32,
                        StridedSliceLayerI32Test,
                        ::testing::Combine(::testing::ValuesIn(ss_only_test_cases_fp32),
                                           ::testing::Values(InferenceEngine::Precision::FP32),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerI32Test::getTestCaseName);

INSTANTIATE_TEST_CASE_P(StridedSliceTest_FP16_I32,
                        StridedSliceLayerI32Test,
                        ::testing::Combine(::testing::ValuesIn(ss_only_test_cases_fp16),
                                           ::testing::Values(InferenceEngine::Precision::FP16),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerI32Test::getTestCaseName);

INSTANTIATE_TEST_CASE_P(StridedSliceTest_I32_I32,
                        StridedSliceLayerI32Test,
                        ::testing::Combine(::testing::ValuesIn(ss_only_test_cases_i32),
                                           ::testing::Values(InferenceEngine::Precision::I32),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerI32Test::getTestCaseName);

INSTANTIATE_TEST_CASE_P(smoke_StridedSliceTest_FP32_I32,
                        StridedSliceLayerI32Test,
                        ::testing::Combine(::testing::ValuesIn(smoke_test_cases_fp32),
                                           ::testing::Values(InferenceEngine::Precision::FP32),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(ov::test::utils::DEVICE_NVIDIA),
                                           ::testing::Values(std::map<std::string, std::string>())),
                        StridedSliceLayerI32Test::getTestCaseName);

}  // namespace
