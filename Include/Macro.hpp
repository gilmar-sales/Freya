#pragma once

#include <memory>

#define FREYA_NAMESPACE fra

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T>
using UniqueRef = std::unique_ptr<T>;