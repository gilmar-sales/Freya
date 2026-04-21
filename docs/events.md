# Event System

Freya provides a flexible pub/sub event system for handling application events.

## EventManager

Central hub for subscribing to and publishing events.

```cpp
auto eventManager = serviceProvider->GetService<fra::EventManager>();

// Subscribe to an event
eventManager->Subscribe<WindowResizeEvent>([](WindowResizeEvent event) {
    std::cout << "Window resized to " << event.width << "x" << event.height << std::endl;
});

// Publish an event
eventManager->Send(MyEvent{ .data = 42 });
```

## Event Types

### Window Events

#### WindowCloseEvent

Fired when the window is closed.

```cpp
struct WindowCloseEvent {};
```

#### WindowResizeEvent

Fired when the window is resized.

```cpp
struct WindowResizeEvent
{
    std::uint32_t width;
    std::uint32_t height;
    bool handled = false;
};
```

#### WindowFocusEvent

Fired when the window gains or loses focus.

```cpp
struct WindowFocusEvent
{
    bool focused;
};
```

### Keyboard Events

#### KeyPressedEvent

Fired when a key is pressed.

```cpp
struct KeyPressedEvent
{
    KeyCode key;
    bool held;  // true if key is being held down
};
```

#### KeyReleasedEvent

Fired when a key is released.

```cpp
struct KeyReleasedEvent
{
    KeyCode key;
};
```

#### KeyTypedEvent

Fired when a key is typed (for text input).

```cpp
struct KeyTypedEvent
{
    char32_t codepoint;
};
```

### Mouse Events

#### MouseButtonPressedEvent

Fired when a mouse button is pressed.

```cpp
struct MouseButtonPressedEvent
{
    MouseButton button;
    glm::vec2 position;
};
```

#### MouseButtonReleasedEvent

Fired when a mouse button is released.

```cpp
struct MouseButtonReleasedEvent
{
    MouseButton button;
    glm::vec2 position;
};
```

#### MouseMovedEvent

Fired when the mouse moves.

```cpp
struct MouseMovedEvent
{
    glm::vec2 position;
    glm::vec2 delta;
};
```

#### MouseScrolledEvent

Fired when the mouse wheel is scrolled.

```cpp
struct MouseScrolledEvent
{
    glm::vec2 offset;
};
```

### Gamepad Events

#### GamepadConnectedEvent

Fired when a gamepad is connected.

```cpp
struct GamepadConnectedEvent
{
    std::uint32_t gamepadId;
};
```

#### GamepadDisconnectedEvent

Fired when a gamepad is disconnected.

```cpp
struct GamepadDisconnectedEvent
{
    std::uint32_t gamepadId;
};
```

#### GamepadButtonPressedEvent

Fired when a gamepad button is pressed.

```cpp
struct GamepadButtonPressedEvent
{
    std::uint32_t gamepadId;
    GamepadButton button;
};
```

#### GamepadButtonReleasedEvent

Fired when a gamepad button is released.

```cpp
struct GamepadButtonReleasedEvent
{
    std::uint32_t gamepadId;
    GamepadButton button;
};
```

#### GamepadAxisEvent

Fired when a gamepad axis changes.

```cpp
struct GamepadAxisEvent
{
    std::uint32_t gamepadId;
    GamepadAxis axis;
    float value;  // -1.0 to 1.0
};
```

## KeyCode

Key codes for keyboard input. See `KeyCode.hpp` for the full enumeration.

Common key codes:
- `KeyCode::A` through `KeyCode::Z`
- `KeyCode::Num0` through `KeyCode::Num9`
- `KeyCode::Space`
- `KeyCode::Enter`
- `KeyCode::Escape`
- `KeyCode::Left`, `KeyCode::Right`, `KeyCode::Up`, `KeyCode::Down`

## MouseButton

Mouse button identifiers.

```cpp
enum class MouseButton
{
    Left,
    Right,
    Middle,
    X1,
    X2
};
```

## GamepadButton

Gamepad button identifiers.

```cpp
enum class GamepadButton
{
    A, B, X, Y,
    LeftShoulder, RightShoulder,
    LeftTrigger, RightTrigger,
    DPadUp, DPadDown, DPadLeft, DPadRight,
    Start, Select,
    LeftThumb, RightThumb
};
```

## GamepadAxis

Gamepad axis identifiers.

```cpp
enum class GamepadAxis
{
    LeftX, LeftY,
    RightX, RightY,
    LeftTrigger,
    RightTrigger
};
```

## Event Subscriptions

Subscribe to events in `StartUp()`:

```cpp
void StartUp() override
{
    mEventManager->Subscribe<WindowCloseEvent>([this](WindowCloseEvent) {
        std::cout << "Window closed!" << std::endl;
    });

    mEventManager->Subscribe<KeyPressedEvent>([this](KeyPressedEvent event) {
        if (event.key == KeyCode::Escape)
        {
            // Handle escape key
        }
    });

    mEventManager->Subscribe<MouseMovedEvent>([this](MouseMovedEvent event) {
        std::cout << "Mouse at " << event.position.x << ", " << event.position.y << std::endl;
    });
}
```
