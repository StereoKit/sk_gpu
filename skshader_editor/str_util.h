#pragma once

inline bool str_starts_with(const char *src, const char *with) {
	while (*with != '\0' && *src != '\0') {
		if (*with != *src)
			return false;
		src++;
		with++;
	}
	return *with=='\0';
}