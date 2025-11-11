#include <math.h>

#include "a1wl.h"

int dotY = 100;
int dotX = 100;
int sampleRate = 44100;

void process(short* buffer, int size);

//Called once at startup
void start() {
	window(L"title", 600, 400, 1000, 500); //Sets the title, width, height, x & y positions of the window
	//initAudio(sampleRate, process);
	winFPS(120);
	win.clearCol = col(10, 20, 20);
}

//Called every frame
void update() {
	cls(); //Clears the screen
	
	//Moves the pixel left/right
	if (keyDown('d')) {
		dotX += 5;
	}
	if (keyPress('a')) { //Note 'keyPress' will only trigger one time even if you hold down the key
		dotX -= 10;
	}

	//Moves the pixel up/down using the scroll wheel
	if (mouseDown(M_WHEELDOWN)) {
		dotY += 5;
	}
	if (mouseDown(M_WHEELUP)) {
		dotY -= 5;
	}
	pixel(dotX, dotY, col(255, 0, 0)); //Draws the pixel

	//Exits the program
	if (keyPress(VK_ESCAPE)) { win.quit = 1; }

	//Draws sqaure near the mouse cursor
	box(mouse.x + 50, mouse.y, 10, 10, col(255, 255, 255));


	//If left-click is down, draws another sqaure near the cursor
	if (mouseDown(M_LEFT)) { box(mouse.x + 100, mouse.y, 10, 10, col(255, 255, 255)); }

	//Text rendering
	win.textCol = col(255, 255, 255);
	text("text is white!", 2, 2);
	win.textCol = col(0, 0, 255);
	text("text is blue!", 30, 5);
	
}

//Called every audio block (1024 samples)
void process(short* buffer, int size) {

	//Goes through the entire audio buffer
	for (int i = 0; i < size; i++) {

		//Generates a sine wave for each sample
		short sinVal = (short)(sinf(win.sample * 6.283185 / sampleRate * 440.0f) * 1000.0f);

		int index = win.sample % size;
		buffer[index * 2] = sinVal;
		buffer[index * 2 + 1] = sinVal;
		win.sample++;
	}
}