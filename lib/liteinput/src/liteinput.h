#ifndef LIBLITEINPUT_H
#define LIBLITEINPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct _keyboard_modifiers{
	bool left_control : 1;
	bool right_control : 1;
	bool left_shift : 1;
	bool right_shift : 1;
	bool left_alt : 1;
	bool right_alt : 1;
	bool left_meta : 1; 
	bool right_meta : 1;
	bool left_super : 1;
	bool right_super : 1;
	bool left_hyper : 1;
	bool right_hyper : 1;
} keyboard_modifiers;

typedef struct _keyboard_event{
	bool pressed;
	bool released;
	char keychar; // The ASCII character of the key, 0 if unavailable
	uint16_t keycode;
	uint16_t keysym; // On X11, the KeySym, on windows, the Virtual Key code
	keyboard_modifiers modifiers;
	size_t timestamp; // Timestamp of event, in milliseconds
} keyboard_event;

typedef enum _mouse_button{
	mouse_button_left,
	mouse_button_right,
	mouse_button_middle,
} mouse_button;

typedef enum _mouse_button_event_kind{
	mouse_press_event,
	mouse_release_event,
} mouse_button_event_kind;

typedef struct _mouse_button_event{
	mouse_button button;
	mouse_button_event_kind kind;
} mouse_button_event;

typedef struct _mouse_move_event{
	unsigned int x, y;
	float velocity_x, velocity_y, velocity;
} mouse_move_event;

typedef struct _event_listener{
	bool listen_keyboard;
	bool initialized;
	void *data; // Internal data, do not use
	bool listen_mouse_button;
	bool listen_mouse_move;
} event_listener;

typedef enum _liteinput_error{
	LITEINPUT_OK = 0,
	LITEINPUT_UNINITIALIZED,
	LITEINPUT_MALLOC,
	LITEINPUT_XKB,
	LITEINPUT_DEV_INPUT_DIR,
	LITEINPUT_NO_PERM,
	LITEINPUT_IOCTL_FAIL,
	LITEINPUT_POLL_FAIL,
} liteinput_error;

typedef void (*keyboard_callback)(keyboard_event);
typedef void (*mouse_button_callback)(mouse_button_event);
typedef void (*mouse_move_callback)(mouse_move_event);

char const *liteinput_error_get_message(liteinput_error error);

liteinput_error event_listener_init( event_listener *listener );
liteinput_error event_listener_poll( event_listener *listener ,
									 keyboard_callback callback,
									 mouse_button_callback button_callback, 
									 mouse_move_callback move_callback );

// Free up internal data in the Listener.
liteinput_error event_listener_free(event_listener *listener);

#endif