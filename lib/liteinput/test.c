#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include "src/liteinput.h"

event_listener listener;

void keyboard_callback_func(keyboard_event evt){
	if(evt.pressed) printf("%i pressed\n",evt.keycode);
	if(evt.released) printf("%i released\n",evt.keycode);
}

int main(void){
	liteinput_error rs;

	rs = event_listener_init(&listener);
	if(rs != LITEINPUT_OK){
		fprintf(stderr, "ERROR: Failed to create keyboard listener! Error: %s\n",liteinput_error_get_message(rs));
		return 1;
	}

	for(int i=0; i<10000; i++){
		rs = event_listener_poll(&listener,keyboard_callback_func, NULL, NULL);
		if(rs != LITEINPUT_OK){
			fprintf(stderr, "ERROR: Failed to poll keyboard listener! Error: %s\n",liteinput_error_get_message(rs));
			return 1;
		}
	}

	rs = event_listener_free(&listener);
	if(rs != LITEINPUT_OK) {
		fprintf(stderr, "ERROR: Failed to free keyboard listener! Error: %s\n",liteinput_error_get_message(rs));
		return 1;
	}

	return 0;
}
