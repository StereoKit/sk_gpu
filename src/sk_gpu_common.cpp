#include "sk_gpu_dev.h"
///////////////////////////////////////////
// Common Code                           //
///////////////////////////////////////////

void (*_skr_log)(const char *text);
void skr_log_callback(void (*callback)(const char *text)) {
	_skr_log = callback;
}
void skr_log(const char *text) {
	if (_skr_log) _skr_log(text);
}

///////////////////////////////////////////
