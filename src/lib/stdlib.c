#include <lib/stdlib.h>
#include <driver/video.h>

void* malloc(u32int size) {;
	return (void*)kmalloc(size);
}

void free(void* p) {
	kfree(p);
}

void* calloc(u32int size) {
	char* temp = (char*) kmalloc(size);
	if (temp == NULL)
		return NULL;
	for(int i = 0; i < size; i++) temp[i] = 0;
	return (void*) temp;
}

void memcpy(void* to, void* from, u32int count) {
	u32int i;
	char* cfrom = (char *) from;
	char* cto = (char *) to;
	for (i = 0; i < count; i++) {
		cto[i] = cfrom[i];
	}
}

void* realloc(void* ptr, int newsize) {
	// TODO: do realloc!!!
	return NULL;
}

int atoi(const char *str) {
    int i = 0;
    while (*str) {
        i = (i << 3) + (i << 1) + (*str - '0');
        str++;
    }
    return i;
}

int htoi(const char *str) {
    int i = 0;
    while (*str) {
        i = (i << 4) + (*str - '0');
        str++;
    }
    return i;
}

void panic(char* str, int line ,int lock) {
    _cli();
//  char format = video_getFormattedColor(WHITE, BLACK);
    char *video = (char*) VIDEO_ADDRESS + 160 * (line - 1);
//  video_clearScreen(format);
    int i;
    int msgLength = strlen(str);
    for (i = 0; i < msgLength * 2; i+=2) {
        *(video + i) = str[i/2];
    }
    if (lock) {
        __asm volatile("hlt");
    }
    _sti();
}
