#pragma once
#include <stdint.h>

int kh1vita_platform_init(void);
void kh1vita_platform_shutdown(void);
void kh1vita_platform_delay_ms(unsigned ms);
uint32_t kh1vita_platform_buttons(void);
int kh1vita_platform_mkdirs(void);
void kh1vita_log(const char *fmt, ...);
