// Copyright (c) Team CharLS.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "coding_parameters.h"
#include "span.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>


// During decoding, CharLS process one line at a time.
// Conversions include color transforms, line interleaved vs sample interleaved, masking out unused bits,
// accounting for line padding etc.

namespace charls {

struct process_decoded_line
{
    virtual ~process_decoded_line() = default;

    virtual void new_line_decoded(const void* source, size_t pixel_count, size_t source_stride) = 0;

protected:
    process_decoded_line() = default;
    process_decoded_line(const process_decoded_line&) = default;
    process_decoded_line(process_decoded_line&&) = default;
    process_decoded_line& operator=(const process_decoded_line&) = default;
    process_decoded_line& operator=(process_decoded_line&&) = default;
};


class process_decoded_single_component final : public process_decoded_line
{
public:
    process_decoded_single_component(std::byte* destination, const size_t stride, const size_t bytes_per_pixel) noexcept :
        destination_{destination}, bytes_per_pixel_{bytes_per_pixel}, stride_{stride}
    {
        ASSERT(bytes_per_pixel == sizeof(std::byte) || bytes_per_pixel == sizeof(uint16_t));
    }

    void new_line_decoded(const void* source, const size_t pixel_count, size_t /* source_stride */) noexcept(false) override
    {
        memcpy(destination_, source, pixel_count * bytes_per_pixel_);
        destination_ += stride_;
    }

private:
    std::byte* destination_;
    size_t bytes_per_pixel_;
    size_t stride_;
};


template<typename Transform, typename PixelType>
void transform_line(triplet<PixelType>* destination, const triplet<PixelType>* source, const size_t pixel_count,
                    const Transform& transform) noexcept
{
    for (size_t i{}; i < pixel_count; ++i)
    {
        destination[i] = transform(source[i].v1, source[i].v2, source[i].v3);
    }
}


template<typename PixelType>
void transform_line(quad<PixelType>* destination, const quad<PixelType>* source, const size_t pixel_count) noexcept
{
    for (size_t i{}; i < pixel_count; ++i)
    {
        destination[i] = source[i];
    }
}


template<typename PixelType>
void transform_line_to_quad(const PixelType* source, const size_t pixel_stride_in, quad<PixelType>* destination,
                            const size_t pixel_stride) noexcept
{
    const auto pixel_count{std::min(pixel_stride, pixel_stride_in)};

    for (size_t i{}; i < pixel_count; ++i)
    {
        destination[i] = {source[i], source[i + pixel_stride_in], source[i + 2 * pixel_stride_in],
                          source[i + 3 * pixel_stride_in]};
    }
}


template<typename Transform, typename PixelType>
void transform_line_to_triplet(const PixelType* source, const size_t pixel_stride_in, triplet<PixelType>* destination,
                               const size_t pixel_stride, const Transform& transform) noexcept
{
    const auto pixel_count{std::min(pixel_stride, pixel_stride_in)};

    for (size_t i{}; i < pixel_count; ++i)
    {
        destination[i] = transform(source[i], source[i + pixel_stride_in], source[i + 2 * pixel_stride_in]);
    }
}


template<typename TransformType>
class process_decoded_transformed final : public process_decoded_line
{
public:
    process_decoded_transformed(std::byte* destination, const size_t stride, const int32_t component_count,
                                const interleave_mode interleave_mode) noexcept :
        destination_{destination}, stride_{stride}, component_count_{component_count}, interleave_mode_{interleave_mode}
    {
    }

    void new_line_decoded(const void* source, const size_t pixel_count, const size_t source_stride) noexcept(false) override
    {
        decode_transform(source, destination_, pixel_count, source_stride);
        destination_ += stride_;
    }

    void decode_transform(const void* source, void* destination, const size_t pixel_count, const size_t byte_stride) noexcept
    {
        if (component_count_ == 3)
        {
            if (interleave_mode_ == interleave_mode::sample)
            {
                transform_line(static_cast<triplet<size_type>*>(destination), static_cast<const triplet<size_type>*>(source),
                               pixel_count, inverse_transform_);
            }
            else
            {
                transform_line_to_triplet(static_cast<const size_type*>(source), byte_stride,
                                          static_cast<triplet<size_type>*>(destination), pixel_count, inverse_transform_);
            }
        }
        else if (component_count_ == 4)
        {
            if (interleave_mode_ == interleave_mode::sample)
            {
                transform_line(static_cast<quad<size_type>*>(destination), static_cast<const quad<size_type>*>(source),
                               pixel_count);
            }
            else if (interleave_mode_ == interleave_mode::line)
            {
                transform_line_to_quad(static_cast<const size_type*>(source), byte_stride,
                                       static_cast<quad<size_type>*>(destination), pixel_count);
            }
        }
    }

private:
    using size_type = typename TransformType::size_type;

    std::byte* destination_;
    size_t stride_;
    int32_t component_count_;
    interleave_mode interleave_mode_;
    typename TransformType::inverse inverse_transform_{};
};

} // namespace charls
