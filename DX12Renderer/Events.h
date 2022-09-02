#pragma once
#include "KeyCodes.h"

// Base class for all event args
class EventArgs
{
public:
	EventArgs() = default;
};

class KeyEventArgs : public EventArgs
{
public:
	enum KeyState
	{
		Released = 0,
		Pressed = 1
	};

	using Base = EventArgs;

	KeyEventArgs(const KeyCode::Key key, const unsigned int c, const KeyState state, const bool control,
	             const bool shift, const bool alt)
		: Key(key)
		  , Char(c)
		  , State(state)
		  , Control(control)
		  , Shift(shift)
		  , Alt(alt)
	{
	}

	KeyCode::Key Key; // The Key Code that was pressed or released.
	unsigned int Char;
	// The 32-bit character code that was pressed. This value will be 0 if it is a non-printable character.
	KeyState State; // Was the key pressed or released?
	bool Control; // Is the Control modifier pressed
	bool Shift; // Is the Shift modifier pressed
	bool Alt; // Is the Alt modifier pressed
};

class MouseMotionEventArgs : public EventArgs
{
public:
	using Base = EventArgs;

	MouseMotionEventArgs(const bool leftButton, const bool middleButton, const bool rightButton, const bool control,
	                     const bool shift, const int x, const int y)
		: LeftButton(leftButton)
		  , MiddleButton(middleButton)
		  , RightButton(rightButton)
		  , Control(control)
		  , Shift(shift)
		  , X(x)
		  , Y(y)
		  , RelX(0)
		  , RelY(0)
	{
	}

	bool LeftButton; // Is the left mouse button down?
	bool MiddleButton; // Is the middle mouse button down?
	bool RightButton; // Is the right mouse button down?
	bool Control; // Is the CTRL key down?
	bool Shift; // Is the Shift key down?

	int X; // The X-position of the cursor relative to the upper-left corner of the client area.
	int Y; // The Y-position of the cursor relative to the upper-left corner of the client area.
	int RelX; // How far the mouse moved since the last event.
	int RelY; // How far the mouse moved since the last event.
};

class MouseButtonEventArgs : public EventArgs
{
public:
	enum MouseButton
	{
		None = 0,
		Left = 1,
		Right = 2,
		Middle = 3
	};

	enum ButtonState
	{
		Released = 0,
		Pressed = 1
	};

	using Base = EventArgs;

	MouseButtonEventArgs(const MouseButton buttonId, const ButtonState state, const bool leftButton,
	                     const bool middleButton,
	                     const bool rightButton,
	                     const bool control, const bool shift, const int x, const int y)
		: Button(buttonId)
		  , State(state)
		  , LeftButton(leftButton)
		  , MiddleButton(middleButton)
		  , RightButton(rightButton)
		  , Control(control)
		  , Shift(shift)
		  , X(x)
		  , Y(y)
	{
	}

	MouseButton Button; // The mouse button that was pressed or released.
	ButtonState State; // Was the button pressed or released?
	bool LeftButton; // Is the left mouse button down?
	bool MiddleButton; // Is the middle mouse button down?
	bool RightButton; // Is the right mouse button down?
	bool Control; // Is the CTRL key down?
	bool Shift; // Is the Shift key down?

	int X; // The X-position of the cursor relative to the upper-left corner of the client area.
	int Y; // The Y-position of the cursor relative to the upper-left corner of the client area.
};

class MouseWheelEventArgs : public EventArgs
{
public:
	using Base = EventArgs;

	MouseWheelEventArgs(const float wheelDelta, const bool leftButton, const bool middleButton, const bool rightButton,
	                    const bool control,
	                    const bool shift, const int x, const int y)
		: WheelDelta(wheelDelta)
		  , LeftButton(leftButton)
		  , MiddleButton(middleButton)
		  , RightButton(rightButton)
		  , Control(control)
		  , Shift(shift)
		  , X(x)
		  , Y(y)
	{
	}

	float WheelDelta;
	// How much the mouse wheel has moved. A positive value indicates that the wheel was moved to the right. A negative value indicates the wheel was moved to the left.
	bool LeftButton; // Is the left mouse button down?
	bool MiddleButton; // Is the middle mouse button down?
	bool RightButton; // Is the right mouse button down?
	bool Control; // Is the CTRL key down?
	bool Shift; // Is the Shift key down?

	int X; // The X-position of the cursor relative to the upper-left corner of the client area.
	int Y; // The Y-position of the cursor relative to the upper-left corner of the client area.
};

class ResizeEventArgs : public EventArgs
{
public:
	using Base = EventArgs;

	ResizeEventArgs(const int width, const int height)
		: Width(width)
		  , Height(height)
	{
	}

	// The new width of the window
	int Width;
	// The new height of the window.
	int Height;
};

class UpdateEventArgs : public EventArgs
{
public:
	using Base = EventArgs;

	UpdateEventArgs(const double deltaTime, const double totalTime)
		: ElapsedTime(deltaTime)
		  , TotalTime(totalTime)
	{
	}

	double ElapsedTime;
	double TotalTime;
};

class RenderEventArgs : public EventArgs
{
public:
	using Base = EventArgs;

	RenderEventArgs(const double deltaTime, const double totalTime)
		: ElapsedTime(deltaTime)
		  , TotalTime(totalTime)
	{
	}

	double ElapsedTime;
	double TotalTime;
};

class UserEventArgs : public EventArgs
{
public:
	using Base = EventArgs;

	UserEventArgs(const int code, void* data1, void* data2)
		: Code(code)
		  , Data1(data1)
		  , Data2(data2)
	{
	}

	int Code;
	void* Data1;
	void* Data2;
};
