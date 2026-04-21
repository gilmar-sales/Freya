# Containers

Freya includes custom container data structures optimized for graphics engine use cases.

## SparseSet

A sparse set container providing O(1) insertion, removal, and access.

```cpp
#include "Freya/Containers/SparseSet.hpp"

// Create a sparse set with unsigned integers
fra::SparseSet<unsigned> sparseSet;

// Add elements (insert)
sparseSet.insert(10);
sparseSet.insert(25);
sparseSet.insert(100);

// Check existence
if (sparseSet.contains(25))
{
    // Element exists
}

// Access elements by index
auto& element = sparseSet[0];

// Remove elements
sparseSet.remove(25);

// Iterate (reverse iteration)
for (auto id : sparseSet)
{
    // Process id
}
```

### SparseSet Properties

- **O(1)** insertion, removal, and lookup
- **Dense storage** for values associated with IDs
- **Reverse iteration** over only active elements
- **Thread-safe** with internal mutex locking
- **Sort capability** for ordered access

## MeshSet

A specialized container for mesh management with mesh-to-index mapping.

```cpp
#include "Freya/Containers/MeshSet.hpp"

fra::MeshSet meshSet;

// Add mesh and get its index
meshSet.insert(meshData);

// Check existence
if (meshSet.contains(meshIndex))
{
    // Mesh exists
}

// Access mesh data by index
auto& meshData = meshSet[meshIndex];

// Remove mesh
meshSet.remove(meshIndex);
```

## Usage with Pools

These containers are used internally by the asset pools:

- `MeshPool` uses `MeshSet` internally to manage mesh indices
- `SparseSet` is used throughout the engine for efficient ID-to-object mapping
