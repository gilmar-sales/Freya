#pragma once

#include <memory>
#include <cassert>
#include <utility>

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

namespace FREYA_NAMESPACE
{
  template <typename T, typename... Args>
  Ref<T> MakeRef(Args&&... args)
  {
    return std::make_shared<T>(args...);
  }

  template <typename T, typename... Args>
  UniqueRef<T> MakeUniqueRef(Args&&... args)
  {
    return std::make_unique<T>(args...);
  }
} // namespace FREYA_NAMESPACE

#include "Builders/ApplicationBuilder.hpp"
#include "Events/Events.hpp"
