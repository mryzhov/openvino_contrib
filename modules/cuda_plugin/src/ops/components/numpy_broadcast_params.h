// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ngraph/shape.hpp>
#include <vector>

#include "kernels/numpy_broadcast_mapper.cuh"
#include "workbuffer_desc.hpp"

namespace CUDAPlugin {

class NumpyBroadcastParams {
public:
    virtual ~NumpyBroadcastParams() {}
    static std::unique_ptr<NumpyBroadcastParams> create(const ngraph::Shape& in_shape, const ngraph::Shape& out_shape);

    virtual void addWorkbufferRequests(std::vector<WorkbufferRequest::size_in_bytes_t>& immutable_buffer_sizes) = 0;
    virtual void initWorkbuffers(const std::vector<CUDA::DevicePointer<void*>>& buffers) const = 0;

    virtual kernel::NumpyBroadcastMapper mapper(
        const std::vector<CUDA::DevicePointer<const void*>>& immutable_buffers) const = 0;
};

class NumpyBroadcastParamsIdentity : public NumpyBroadcastParams {
public:
    void addWorkbufferRequests(std::vector<WorkbufferRequest::size_in_bytes_t>& immutable_buffer_sizes) override {}
    void initWorkbuffers(const std::vector<CUDA::DevicePointer<void*>>& buffers) const override {}
    kernel::NumpyBroadcastMapper mapper(
        const std::vector<CUDA::DevicePointer<const void*>>& immutable_buffers) const override {
        return kernel::NumpyBroadcastMapper{};
    }
};

class NumpyBroadcastParamsImpl : public NumpyBroadcastParams {
public:
    NumpyBroadcastParamsImpl(const ngraph::Shape& in_shape, const ngraph::Shape& out_shape);

    void addWorkbufferRequests(std::vector<WorkbufferRequest::size_in_bytes_t>& immutable_buffer_sizes) override;
    void initWorkbuffers(const std::vector<CUDA::DevicePointer<void*>>& buffers) const override;

    kernel::NumpyBroadcastMapper mapper(
        const std::vector<CUDA::DevicePointer<const void*>>& immutable_buffers) const override;

private:
    size_t shape_rank_;
    std::vector<size_t> src_strides_;
    std::vector<size_t> dst_strides_;
    std::vector<size_t> broadcasted_dims_;

    WorkbufferDesc ib_src_strides_;
    WorkbufferDesc ib_dst_strides_;
    WorkbufferDesc ib_broadcasted_dims_;
};

}  // namespace CUDAPlugin