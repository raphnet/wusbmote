#ifndef _gamepad_h__
#define _gamepad_h__

typedef struct {
	// size of reports built by buildReport
	int report_size;

	int reportDescriptorSize;
	const void *reportDescriptor; // must be in flash

	int deviceDescriptorSize;
	const void *deviceDescriptor; // must be in flash

	void (*init)(void);
	void (*update)(void);
	char (*changed)(void);
	void (*buildReport)(unsigned char *buf);
} Gamepad;

#endif // _gamepad_h__
