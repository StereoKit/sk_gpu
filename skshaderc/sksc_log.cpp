#include "_sksc.h"

#include <stdarg.h>
#include <stdio.h>

///////////////////////////////////////////

array_t<sksc_log_item_t> sksc_log_list = {};

///////////////////////////////////////////

void sksc_log(log_level_ level, const char *text, ...) {
	sksc_log_item_t item = {};
	item.level  = level;
	item.line   = -1;
	item.column = -1;

	va_list args;
	va_start(args, text);
	va_list copy;
	va_copy(copy, args);
	size_t length = vsnprintf(nullptr, 0, text, args);
	item.text = (char*)malloc(length + 2);
	vsnprintf((char*)item.text, length + 2, text, copy);
	va_end(copy);
	va_end(args);

	sksc_log_list.add(item);
}

///////////////////////////////////////////

void sksc_log_at(log_level_ level, int32_t line, int32_t column, const char *text, ...) {
	sksc_log_item_t item = {};
	item.level  = level;
	item.line   = line;
	item.column = column;

	va_list args;
	va_start(args, text);
	va_list copy;
	va_copy(copy, args);
	size_t length = vsnprintf(nullptr, 0, text, args);
	item.text = (char*)malloc(length + 2);
	vsnprintf((char*)item.text, length + 2, text, copy);
	va_end(copy);
	va_end(args);

	sksc_log_list.add(item);
}

///////////////////////////////////////////

void sksc_log_print(const char *file, const sksc_settings_t *settings) {
	for (size_t i = 0; i < sksc_log_list.count; i++) {
		if (sksc_log_list[i].level == log_level_info && !settings->silent_info) {
			printf("%s\n", sksc_log_list[i].text);
		}
	}
	for (size_t i = 0; i < sksc_log_list.count; i++) {
		if ((sksc_log_list[i].level == log_level_err_pre && !settings->silent_err)) {
			printf("%s", sksc_log_list[i].text);
		}
	}
	for (size_t i = 0; i < sksc_log_list.count; i++) {
		if ((sksc_log_list[i].level == log_level_warn && !settings->silent_warn) ||
			(sksc_log_list[i].level == log_level_err  && !settings->silent_err )) {

			const char* level = sksc_log_list[i].level == log_level_warn
				? "warning"
				: "error";

			if (sksc_log_list[i].line < 0) {
				printf("%s: %s: %s\n", file, level, sksc_log_list[i].text);
			} else {
				printf("%s(%d,%d): %s: %s\n", file, sksc_log_list[i].line, sksc_log_list[i].column, level, sksc_log_list[i].text);
			}
		}
	}
}

///////////////////////////////////////////

void sksc_log_clear() {
	sksc_log_list.each([](sksc_log_item_t &i) {free((void*)i.text); });
	sksc_log_list.clear();
}

///////////////////////////////////////////

int32_t sksc_log_count() {
	return (int32_t)sksc_log_list.count;
}

///////////////////////////////////////////

sksc_log_item_t sksc_log_get(int32_t index) {
	if (index < 0 || index >= sksc_log_list.count)
		return {};
	return sksc_log_list[index];
}