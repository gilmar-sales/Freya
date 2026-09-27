# Event System

Freya provides a flexible pub/sub event system for handling application events.

All event structs derive from the base `Event` (see `include/Freya/Events/Event.hpp`),
which carries the propagation flag:

```cpp
struct Event
{
    bool handled; ///< Whether the event was handled (stop propagation)
};
```

## EventManager

Central hub for subscribing to and publishing events.

```cpp
auto eventManager = serviceProvider->GetService<fra::EventManager>();

// Subscribe to an event (listener takes const T&)
fra::EventSubscription sub =
    eventManager->Subscribe<fra::WindowResizeEvent>(
        [](const fra::WindowResizeEvent& event) {
            std::cout << "Window resized to " << event.width << "x"
                      << event.height << std::endl;
        });

// Unsubscribe when no longer interested
eventManager->Unsubscribe<fra::WindowResizeEvent>(sub);

// Publish an event
eventManager->Send(fra::WindowResizeEvent{ .width = 1280, .height = 720 });
```

`Subscribe` returns an `EventSubscription` token (`std::uint64_t`,
see `Publisher.hpp`). Keep it if you later need to call
`Unsubscribe<T>(subscription)`.

## Event Types

### Window Events

#### WindowCloseEvent

Fired when the window is closed.

```cpp
struct WindowCloseEvent : Event
{
};
```

#### WindowResizeEvent

Fired when the window is resized.

```cpp
struct WindowResizeEvent : Event
{
    std::int32_t width;
    std::int32_t height;
};
```

### Keyboard Events

#### KeyPressedEvent

Fired when a key is pressed.

```cpp
struct KeyPressedEvent : Event
{
    KeyCode key;
};
```

#### KeyReleasedEvent

Fired when a key is released.

```cpp
struct KeyReleasedEvent : Event
{
    KeyCode key;
};
```

#### TextInputEvent

UTF-8 text committed by the OS (IME / keyboard). Use this for text entry
instead of interpreting key presses.

```cpp
struct TextInputEvent : Event
{
    std::string text;
};
```

#### TextEditingEvent

IME composition in progress (optional).

```cpp
struct TextEditingEvent : Event
{
    std::string  text;
    std::int32_t start  = 0;
    std::int32_t length = 0;
};
```

### Mouse Events

#### MouseButtonPressedEvent

Fired when a mouse button is pressed.

```cpp
struct MouseButtonPressedEvent : Event
{
    MouseButton button;
};
```

#### MouseButtonReleasedEvent

Fired when a mouse button is released.

```cpp
struct MouseButtonReleasedEvent : Event
{
    MouseButton button;
};
```

#### MouseMoveEvent

Fired when the mouse moves.

```cpp
struct MouseMoveEvent : Event
{
    float x;      ///< Absolute X position
    float y;      ///< Absolute Y position
    float deltaX; ///< Relative X movement
    float deltaY; ///< Relative Y movement
};
```

#### MouseWheelEvent

Fired when the mouse wheel is scrolled. Vertical delta is positive when
scrolled away from the user.

```cpp
struct MouseWheelEvent : Event
{
    float x = 0.f;
    float y = 0.f;
};
```

### Gamepad Events

#### GamepadButtonPressedEvent

Fired when a gamepad button is pressed.

```cpp
struct GamepadButtonPressedEvent : Event
{
    GamepadButton button;
};
```

#### GamepadButtonReleasedEvent

Fired when a gamepad button is released.

```cpp
struct GamepadButtonReleasedEvent : Event
{
    GamepadButton button;
};
```

#### GamepadAxisMotionEvent

Fired when a gamepad axis changes.

```cpp
struct GamepadAxisMotionEvent : Event
{
    GamepadAxis axis;
    double      value; ///< -1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers
};
```

## KeyCode

Key codes for keyboard input (SDL scancodes). See `KeyCode.hpp` for the full
enumeration.

Common key codes:

- `KeyCode::A` through `KeyCode::Z`
- `KeyCode::Num0` through `KeyCode::Num9`
- `KeyCode::Space`
- `KeyCode::Return`
- `KeyCode::Escape`
- `KeyCode::Left`, `KeyCode::Right`, `KeyCode::Up`, `KeyCode::Down`

## MouseButton

Mouse button identifiers.

```cpp
enum class MouseButton
{
    Left = 1,
    Middle,
    Right,
    Button4,
    Button5
};
```

## GamepadButton

Gamepad button identifiers (SDL gamepad style).

```cpp
enum class GamepadButton
{
    GamepadButtonInvalid = -1,
    GamepadButtonSouth,         ///< A/Cross button (bottom)
    GamepadButtonEast,          ///< B/Circle button (right)
    GamepadButtonWest,          ///< X/Square button (left)
    GamepadButtonNorth,         ///< Y/Triangle button (top)
    GamepadButtonBack,          ///< Back/Share button
    GamepadButtonGuide,         ///< Guide/Home button
    GamepadButtonStart,         ///< Start/Options button
    GamepadButtonLeftStick,     ///< Left stick button (press)
    GamepadButtonRightStick,    ///< Right stick button (press)
    GamepadButtonLeftShoulder,  ///< Left bumper
    GamepadButtonRightShoulder, ///< Right bumper
    GamepadButtonDpadUp,        ///< D-pad up
    GamepadButtonDpadDown,      ///< D-pad down
    GamepadButtonDpadLeft,      ///< D-pad left
    GamepadButtonDpadRight,     ///< D-pad right
    GamepadButtonMisc1,         ///< Miscellaneous button 1
    GamepadButtonRightPaddle1,  ///< Right paddle 1
    GamepadButtonLeftPaddle1,   ///< Left paddle 1
    GamepadButtonRightPaddle2,  ///< Right paddle 2
    GamepadButtonLeftPaddle2,   ///< Left paddle 2
    GamepadButtonTouchpad,      ///< Touchpad button
    GamepadButtonMisc2,         ///< Miscellaneous button 2
    GamepadButtonMisc3,         ///< Miscellaneous button 3
    GamepadButtonMisc4,         ///< Miscellaneous button 4
    GamepadButtonMisc5,         ///< Miscellaneous button 5
    GamepadButtonMisc6,         ///< Miscellaneous button 6
    GamepadButtonCount          ///< Number of defined buttons
};
```

## GamepadAxis

Gamepad axis identifiers (sticks and triggers).

```cpp
enum class GamepadAxis
{
    GamepadAxisInvalid = -1,
    GamepadAxisLeftX,        ///< Left stick X axis
    GamepadAxisLeftY,        ///< Left stick Y axis
    GamepadAxisRightX,       ///< Right stick X axis
    GamepadAxisRightY,       ///< Right stick Y axis
    GamepadAxisLeftTrigger,  ///< Left trigger
    GamepadAxisRightTrigger, ///< Right trigger
    GamepadAxisCount         ///< Number of defined axes
};
```

## Event Subscriptions

Subscribe to events in `StartUp()`. Listeners take `const T&`:

```cpp
void StartUp() override
{
    mEventManager->Subscribe<WindowCloseEvent>(
        [this](const WindowCloseEvent&) {
            std::cout << "Window closed!" << std::endl;
        });

    mEventManager->Subscribe<KeyPressedEvent>(
        [this](const KeyPressedEvent& event) {
            if (event.key == KeyCode::Escape)
            {
                // Handle escape key
            }
        });

    mEventManager->Subscribe<MouseMoveEvent>(
        [this](const MouseMoveEvent& event) {
            std::cout << "Mouse at " << event.x << ", " << event.y
                      << std::endl;
        });

    // Text entry uses TextInputEvent, not key presses
    mEventManager->Subscribe<TextInputEvent>(
        [this](const TextInputEvent& event) {
            std::cout << "Typed: " << event.text << std::endl;
        });
}
```

To stop receiving an event, keep the `EventSubscription` token returned by
`Subscribe` and pass it to `Unsubscribe`:

```cpp
mResizeSub = mEventManager->Subscribe<WindowResizeEvent>(
    [this](const WindowResizeEvent& event) { /* ... */ });

// Later (e.g. on shutdown):
mEventManager->Unsubscribe<WindowResizeEvent>(mResizeSub);
```
