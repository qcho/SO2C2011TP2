#include "../include/tty.h"

void initTTY(int index);

static TTY tty[MAX_TTYs];
static int currentTTY;

void tty_init() {
	int i;
	currentTTY = 0;
	for (i = 0; i < MAX_TTYs; i++) {
		initTTY(i);
	}
	char format = video_getFormattedColor(tty[currentTTY].fgColor, tty[currentTTY].bgColor);
	video_clearScreen(format);
}

void initTTY(int index) {
	tty[index].terminal = (char*) calloc(TOTAL_VIDEO_SIZE);
	tty[index].offset = 0;
	tty[index].buffer.head = 0;
	tty[index].buffer.tail = 0;
	tty[index].bgColor = BLACK;
	tty[index].fgColor = WHITE;
}

void tty_setCurrent(int tty) {
	currentTTY = tty;
	TTY* currTTY = tty_getCurrentTTY();
	video_clearScreen(video_getFormattedColor(currTTY->fgColor, currTTY->bgColor));
	video_setOffset(0);
	video_write(currTTY->terminal, currTTY->offset, -1);
}

int tty_getCurrent() {
	return currentTTY;
}

TTY* tty_getTTY(int index) {
	return &tty[index];
}

TTY* tty_getCurrentTTY() {
	return &tty[currentTTY];
}

void tty_write(TTY* tty, char* buffer, size_t size) {
	int j;
	char format = video_getFormattedColor(tty->fgColor, tty->bgColor);
	for (j = 0; j < size; ++j) {
		if (tty->offset >= TOTAL_VIDEO_SIZE) {
			terminal_scroll(tty->terminal);
			tty->offset = terminal_getOffset(ROWS - 1, 0);
			video_clearRow(ROWS - 1, -1);
		}
		int newOffset = terminal_prtSpecialCharater(tty->terminal, tty->offset, buffer[j], format);
		tty->offset += newOffset;
		if (newOffset != 0) { // It was an special character, nothing to print
			continue;
		}
		tty->terminal[tty->offset++] = buffer[j];
		tty->terminal[tty->offset++] = format;
	}
}

void tty_clean(TTY* tty) {
	char format = video_getFormattedColor(tty->fgColor, tty->bgColor);
	int i;
	for(i = 0; i < TOTAL_VIDEO_SIZE; i+=2) {
		tty->terminal[i] = ' ';
		tty->terminal[i + 1] = format;
	};
	tty->offset = 0;
	video_clearScreen(video_getFormattedColor(tty->fgColor, tty->bgColor));
	video_setOffset(0);
	video_write(tty->terminal, tty->offset, -1);
}
char tty_getCurrTTYFormat() {
	TTY* currTTY = tty_getCurrentTTY();
	return video_getFormattedColor(currTTY->fgColor, currTTY->bgColor);
}

void tty_setFormatToCurrTTY(char format) {
	TTY* currTTY = tty_getCurrentTTY();
	currTTY->fgColor = video_getFGcolor(format);
	currTTY->bgColor = video_getBGcolor(format);
}

