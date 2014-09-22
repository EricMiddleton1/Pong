#include <Windows.h>
#include <math.h>
#include <stdbool.h>

/*
 *Pong.c (C) 2014 Eric Middleton
 *This program is free to use and distribute
 *for non-commercial use. The author is not liable
 *for any damages resulting from the use of this program.
 *
 *This program was created for an EE285 Programming assignment.
 *This is my version of the classic 1972 video game written in C using the Windows API.
 *This version of Pong requires Windows XP or greater and has been tested on Windows 7 and 8
 *and was compiled using Visual Studio 2013 Ultimate.
 *
 *To play, compile and run this code. Use the arrow up and down keys to select one or two player mode.
 *In single player mode, use the W and S keys to control the paddle on the left side of the screen. 
 *	The AI will control the right side paddle.
 *In two player mode, player 1 will use the W and S keys to control the left paddle and
 *	player 2 will use the O and L keys to control the right paddle.
 *To serve the ball, press the space bar. The ball will be served from the center to
 *	one of the players at random.
 *The game will continue until one of the players (or the AI) reaches 10 points.
 *The game can be closed at any time by pressing the escape key.
 *
 *--How the game works--
 *On the highest level, this game is a very simple Finite State Machine.
 *On a lower level, this program uses the Windows API to create a borderless window
 *and uses the Windows GDI (Graphics Device Interface) to display the game on the screen.
 *The game is rendered on a 128x128 back buffer and is stretched onto the screen
 *whenever the window is redrawn.
 *The game is updated approximately 100 times per second using a standard Windows timer event
 *
 *There is a precompiled version of this program located at: https://drive.google.com/file/d/0B33_0mkAWVhcTWFEOVAwQV96eHc/edit?usp=sharing
*/


//Defines for game properties
#define BALLSPEED		100
#define ENGLISHSCALE	20
#define BALLSIZE		2
#define	PADDLEHEIGHT	12
#define PADDLEWIDTH		2
#define NETWIDTH		2
#define	MESHSIZE		4
#define	MAXSCORE		10
#define	AI_SPEED		1.f
#define	PLAYER_SPEED	1.5f
#define RESOLUTION		128
#define MARGIN			32

//State definitions
#define STATE_HOME		0
#define STATE_READY		1
#define STATE_SERVE		2
#define STATE_PLAY		3
#define	STATE_END		4

//Game mode definitions
#define MODE_ONE		1
#define	MODE_TWO		2

//Macros
#define PLAYER_SCORE(s)		((s) >> 4)
#define	PLAYER2_SCORE(s)	((s) & 0x0F)
#define MAKE_SCORE(p, a)	( ((p) << 4) | (a))
#define CLAMP(n, a, b)		(min(max( (n), (a) ), (b) ))

//Typedef for a byte
typedef unsigned char byte;

//Ball struct
typedef struct Ball {
	float x, y, vx, vy;
} Ball;

//Paddle struct
typedef struct Paddle {
	float height;
} Paddle;

//Global variables
//These are needed because all of the processing
//is done inside the Windows event loop system
Paddle player2, player;
Ball ball;
byte state, score, mode;
unsigned short width, height;
bool stateChange;

//Windows event loop function
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//Game functions
void UpdateAI(Paddle *AI, Ball b);
void ScoreToStrs(byte score, char *pStr, char*AIStr);
void ServeBall(Ball *b);
void ApplyEnglish(Ball *b, Paddle p);
void UpdateBall(Ball *b, Paddle player, Paddle player2);
void DrawTableText(HDC hdc, HFONT font, unsigned short x, unsigned short y, unsigned short width, unsigned short height, byte format, char *str);
void DrawTable(HDC hdc, Ball b, Paddle player, Paddle player2, byte score, byte state, byte mode);


/*
 *int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
 *This is the entry point for a Windows application
 *It creates a borderless window and enters the Windows event loop
 *until the process is terminated or closed.
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX wc;
	HWND hWnd;
	MSG msg;
	HDC hdc;

	//Seed the random number generator with the time
	srand(time());

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "PongGame";
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	//Attempt to register the windows with the above information
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, "Error: Window Registration has Failed!", "Pong", MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	//Get the screen resolution
	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);

	//Create the window
	hWnd = CreateWindow("PongGame", "Pong", WS_POPUP, 0, 0, width, height, NULL, NULL, hInstance, NULL);

	//Alert the user if the window was not created and then close
	if (!hWnd) {
		MessageBox(NULL, "Error: Window Creation Failed!", "Pong", MB_ICONEXCLAMATION | MB_OK);
		return -1;
	}

	//Show the window
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	//initialize variables
	score = 0;
	state = STATE_HOME;
	mode = MODE_ONE;
	stateChange = true;

	player2.height = (RESOLUTION + MARGIN) / 2.f;
	player.height = (RESOLUTION + MARGIN) / 2.f;

	//Set the update timer
	SetTimer(hWnd, 1, 10, NULL);

	//Enter the event loop
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	//We only get to this point if the process is terminated or closed
	//in some way
	return msg.wParam;
}

/*
 *LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM)
 *This is the function that is automatically called by windows to
 *process messages in the event loop.
 *The entire game is run through this function
*/
int error;
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	//There are hundreds (or maybe thousands) of different window messages.
	//We will switch through the only ones we need and have Windows automatically
	//deal with the rest.
	switch (msg) {
		//The window was just created
		case WM_CREATE:
			//Seed the random number generator with the time
			srand(time());
			//Hide the cursor
			ShowCursor(false);
		break;

		//The window has been asked to close
		case WM_CLOSE:
			DestroyWindow(hWnd);
		break;

		//The program is closing
		case WM_DESTROY:
			PostQuitMessage(0);
		break;

		//If a key has been pressed down
		case WM_KEYDOWN:
			//If escape is pressed at any time, exit the program
			if (wParam == VK_ESCAPE) {
				PostQuitMessage(0);
				return 0;
			} //If we are on the home screen
			else if (state == STATE_HOME) {
				switch (wParam) {
					//Move the cursor up
					case VK_UP:
						mode = MODE_ONE;
						stateChange = true;
						break;
					//Move the cursor down
					case VK_DOWN:
						mode = MODE_TWO;
						stateChange = true;
						break;
					//Use current selection and change state to ready
					case VK_SPACE:
						state = STATE_READY;
						stateChange = true;
						score = 0;
						break;
					default:
						break;
				}
			} //If the game is over and the space bar is pressed
			else if (state == STATE_END && wParam == VK_SPACE) {
				state = STATE_HOME; //change state to home
				stateChange = true;
			} //If the game is ready to serve the ball and the space bar is pressed
			else if (state == STATE_READY && wParam == VK_SPACE) {
				state = STATE_SERVE; //change state to serve
				stateChange = true;
			}
		break;

		//The timer has expired
		case WM_TIMER:
			//If we are not on the home screen
			//We will udpate the player paddles
			//When the appropriate keys are pressed
			if (state != STATE_HOME) {
				player.height += PLAYER_SPEED * (!!(GetAsyncKeyState('S') & 0x8000) - !!(GetAsyncKeyState('W') & 0x8000));
				player.height = CLAMP(player.height, MARGIN + PADDLEHEIGHT / 2.f, RESOLUTION - PADDLEHEIGHT / 2.f);

				if(mode == MODE_TWO) {
					player2.height += PLAYER_SPEED * (!!(GetAsyncKeyState('L') & 0x8000) - !!(GetAsyncKeyState('O') & 0x8000));
					player2.height = CLAMP(player2.height, MARGIN + PADDLEHEIGHT / 2.f, RESOLUTION - PADDLEHEIGHT / 2.f);
				}
			}

			//Update based on state
			if (state == STATE_SERVE) {
				ServeBall(&ball);
				UpdateBall(&ball, player, player2);
			}
			else if (state == STATE_PLAY) {
				UpdateBall(&ball, player, player2);
				if (mode == MODE_ONE) //Update the AI if it's a single player game
					UpdateAI(&player2, ball);
			}

			//Tell windows the screen needs to be redrawn
			//if we are playing the ball, ready for a serve, or if the state has changed
			if (stateChange || state == STATE_PLAY || state == STATE_READY) {
				InvalidateRect(hWnd, NULL, 0);
				stateChange = false;
			}

			//Reset the timer
			SetTimer(hWnd, 1, 10, NULL);
		break;

		//The window needs to be redrawn
		case WM_PAINT:
		{
			//Create the back buffer for flicker-free drawing
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps), bdc = CreateCompatibleDC(hdc);
			HBITMAP bBitmap = CreateCompatibleBitmap(hdc, RESOLUTION, RESOLUTION), oldBitmap;
			oldBitmap = SelectObject(bdc, bBitmap);

			//Render the game on the back buffer and stretch it onto the screen
			DrawTable(bdc, ball, player, player2, score, state, mode);
			StretchBlt(hdc, (width - height) / 2, 0, height, height, bdc, 0, 0, RESOLUTION, RESOLUTION, SRCCOPY);

			//Clean up the drawing objects to avoid a memory leak
			SelectObject(bdc, oldBitmap);
			DeleteObject(bBitmap);
			DeleteDC(bdc);
			EndPaint(hWnd, &ps);
		}
		break;

		//Let Windows handle any other messages
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		break;
	
	}
}

/*
 *void UpdateAI(Paddle*, Ball)
 *This function updates the AI paddle position based on the height of the ball
 *It will attempt to exactly match the height of the ball at all times
 *but is limited in maximum speed by the value of AI_SPEED
*/
void UpdateAI(Paddle *AI, Ball b) {
	float speed = b.y - AI->height, height;

	if (state < STATE_SERVE)
		return;

	speed = CLAMP(speed, -AI_SPEED, AI_SPEED); //Limit the speed

	height = AI->height + speed;
	
	AI->height = CLAMP(height, MARGIN + PADDLEHEIGHT/2.f + 1, RESOLUTION - PADDLEHEIGHT/2.f); //Keep the paddle on the table
}

/*
 *void ServeBall(Ball*)
 *This function serves the ball towards one side at random
 *with a random angle and a set speed of BALLSPEED
*/
void ServeBall(Ball *b) {
	float theta;

	b->x = RESOLUTION / 2.f;
	b->y = (RESOLUTION - MARGIN) / 2.f + MARGIN;

	theta = (rand() % 90 - 45 + 180*(rand() % 2)) * 3.14f / 180.f;

	b->vx = (BALLSPEED / 100.f) * cos(theta);
	b->vy = (BALLSPEED / 100.f) * sin(theta);

	state = STATE_PLAY;
	stateChange = true;
}

/*
 *void ApplyEnglish(Ball *b, Paddle p)
 *This function simulates the application of english (curve)
 *to the ball based on where the ball strikes the paddle
*/
void ApplyEnglish(Ball *b, Paddle p) {
	float english;

	english = -(p.height - b->y)*ENGLISHSCALE / 100;

	b->vy += english;
}

/*
 *void UpdateBall(Ball*, Paddle, Paddle)
 *This function updates the position of the ball based its velocity
 *and handles collisions with the sides and paddles. It also detects when a
 *player has scored and updates the score accordingly. Additionally, it will
 *detect when the game is over and switches the state accordingly.
*/
void UpdateBall(Ball *b, Paddle player, Paddle player2) {
	b->x += b->vx;
	b->y += b->vy;

	//Detect vertical collisions
	if (b->y < (BALLSIZE + MARGIN)) {
		b->y += 2*((BALLSIZE + MARGIN) - b->y);
		b->vy = -b->vy;
	}
	else if (b->y > (RESOLUTION - BALLSIZE + 1)) {
		b->y += 2 * ((RESOLUTION - BALLSIZE + 1) - b->y);
		b->vy = -b->vy;
	}

	//Detect paddle collisions and out of bounds conditions (scores)
	if (b->x < PADDLEWIDTH) {
		//If the ball hit the paddle
		if (b->y >= (player.height - PADDLEHEIGHT / 2.f - BALLSIZE) && b->y <= (player.height + PADDLEHEIGHT / 2.f + BALLSIZE)) {
			b->x += 2 * (PADDLEWIDTH - b->x); //Make it bounce
			b->vx = -b->vx;
			ApplyEnglish(b, player); //Apply english
		}
		else { //The opposing player scored!
			score = MAKE_SCORE(PLAYER_SCORE(score), PLAYER2_SCORE(score) + 1); //Update the score
			if (PLAYER2_SCORE(score) >= MAXSCORE) { //If the game is over
				state = STATE_END; //Switch to end of game state
				stateChange = true;
			}
			else { //Otherwise
				state = STATE_READY; //Switch to ready state
				stateChange = true;
			}
		}
	}
	else if (b->x > (RESOLUTION - PADDLEWIDTH)) {
		if (b->y >= (player2.height - PADDLEHEIGHT / 2.f - BALLSIZE) && b->y <= (player2.height + PADDLEHEIGHT / 2.f + BALLSIZE)) {
			b->x += 2 * ((RESOLUTION - PADDLEWIDTH) - b->x);
			b->vx = -b->vx;
			ApplyEnglish(b, player2);
		}
		else {
			score = MAKE_SCORE(PLAYER_SCORE(score) + 1, PLAYER2_SCORE(score));
			if (PLAYER_SCORE(score) >= MAXSCORE) {
				state = STATE_END;
				stateChange = true;
			}
			else {
				state = STATE_READY;
				stateChange = true;
			}
		}
	}
}

/*
 *void ScoreToStrs(byte, char*, char*)
 *This function converts the byte containing both scores into two
 *strings representing the scores. The strings will always be two digits
 *long (e.g. 0 -> '00' and 7 -> '07').
*/
void ScoreToStrs(byte score, char *pStr, char *player2Str) {
	byte pScore = PLAYER_SCORE(score), player2Score = PLAYER2_SCORE(score); //unpack the scores

	pStr[0] = (pScore / 10) + '0';
	pStr[1] = (pScore % 10) + '0';
	pStr[2] = NULL;

	player2Str[0] = (player2Score / 10) + '0';
	player2Str[1] = (player2Score % 10) + '0';
	player2Str[2] = NULL;
}


/*
 *void DrawTableText(HDC, HFONT, unsigned short, unsigned short, unsigned short, unsigned short, byte, char*)
 *This function will draw text at a specified location
*/
void DrawTableText(HDC hdc, HFONT font, unsigned short x, unsigned short y, unsigned short width, unsigned short height, byte format, char *str) {
	RECT r;
	HFONT oldFont;

	r.bottom = y + height;
	r.left = x;
	r.right = x + width;
	r.top = y;

	oldFont = SelectObject(hdc, font);

	DrawText(hdc, str, -1, &r, format | DT_TOP);

	SelectObject(hdc, oldFont);
}

/*
 *void DrawTable(HDC, Ball, Paddle, Paddle, byte, byte, byte)
 *This function handles draws the game for every state
*/
void DrawTable(HDC hdc, Ball b, Paddle player, Paddle player2, byte score, byte state, byte mode) {
	byte i;
	char pStr[3], player2Str[3];
	HBRUSH brush = GetStockObject(WHITE_BRUSH), oldBrush;
	HPEN pen = CreatePen(PS_NULL, 1, 0x00000000), oldPen, whitePen = GetStockObject(WHITE_PEN);
	HFONT bigFont = CreateFontA(20, 0, 0, 0, 0, false, false, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Courier New");
	HFONT smallFont = CreateFontA(15, 0, 0, 0, 0, false, false, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Courier New");

	//Sets text appearance
	SetBkColor(hdc, 0x00000000);
	SetTextColor(hdc, 0x00FFFFFF);

	//Fill the screen with black
	BitBlt(hdc, 0, 0, RESOLUTION, RESOLUTION, NULL, 0, 0, BLACKNESS);

	//Use a white brush
	oldBrush = SelectObject(hdc, brush);
	//Use a white pen
	oldPen = SelectObject(hdc, whitePen);

	//Draw the top and bottom table borders
	MoveToEx(hdc, 0, MARGIN, NULL);
	LineTo(hdc, RESOLUTION, MARGIN);
	MoveToEx(hdc, 0, RESOLUTION - 1, NULL);
	LineTo(hdc, RESOLUTION, RESOLUTION - 1);

	//Use an invisible pen
	SelectObject(hdc, pen);
	
	//Always draw both paddles
	Rectangle(hdc, 0, player.height - PADDLEHEIGHT / 2, PADDLEWIDTH, player.height + PADDLEHEIGHT / 2);
	Rectangle(hdc, RESOLUTION - PADDLEWIDTH, player2.height - PADDLEHEIGHT / 2, RESOLUTION, player2.height + PADDLEHEIGHT / 2);

	//Always draw the screen
	for (i = MARGIN + MESHSIZE/2; i < (RESOLUTION - MESHSIZE); i += 2 * MESHSIZE) {
		Rectangle(hdc, RESOLUTION / 2 - NETWIDTH / 2, i, RESOLUTION / 2 + NETWIDTH / 2, i + MESHSIZE);
	}

	ScoreToStrs(score, pStr, player2Str);

	DrawTableText(hdc, bigFont, 0, 0, MARGIN, MARGIN, DT_LEFT, pStr);
	DrawTableText(hdc, bigFont, RESOLUTION - MARGIN, 0, MARGIN, MARGIN, DT_RIGHT, player2Str);
	DrawTableText(hdc, bigFont, RESOLUTION / 2 - MARGIN, 0, 2 * MARGIN, MARGIN, DT_CENTER, "Pong");

	//Draw different things based on state
	if (state == STATE_PLAY) //If the state is play
		Rectangle(hdc, b.x - BALLSIZE, b.y - BALLSIZE, b.x + BALLSIZE, b.y + BALLSIZE); //Draw the ball
	else if(state == STATE_HOME) { //If the state is home
		DrawTableText(hdc, smallFont, RESOLUTION / 2 - 2*MARGIN, 1.5*MARGIN, 4 * MARGIN, MARGIN/2, DT_CENTER, "One Player"); //Draw the menu
		DrawTableText(hdc, smallFont, RESOLUTION / 2 - 2*MARGIN, 2*MARGIN, 4 * MARGIN, MARGIN/2, DT_CENTER, "Two Player");
		DrawTableText(hdc, smallFont, RESOLUTION / 2 - 2 * MARGIN, (1.5 + 0.5*(mode == MODE_TWO) )*MARGIN, 16, MARGIN / 2, DT_RIGHT, ">");
	}
	else if (state == STATE_END) { //If the state is end
		//Draw the appropriate end message based on mode and score
		if (mode == MODE_ONE)
			DrawTableText(hdc, smallFont, RESOLUTION / 2 - 2 * MARGIN, 1.5*MARGIN, 4 * MARGIN, MARGIN / 2, DT_CENTER, PLAYER_SCORE(score) > PLAYER2_SCORE(score) ? "You won!" : "You lost!");
		else {
			DrawTableText(hdc, smallFont, RESOLUTION / 2 - 2 * MARGIN, 1.5*MARGIN, 4 * MARGIN, MARGIN / 2, DT_CENTER, PLAYER_SCORE(score) > PLAYER2_SCORE(score) ? "Player 1" : "Player 2");
			DrawTableText(hdc, smallFont, RESOLUTION / 2 - 2 * MARGIN, 2 * MARGIN, 4 * MARGIN, MARGIN / 2, DT_CENTER, "Wins!");
		}
	}

	//Delete drawing objects to avoid a memory leak
	SelectObject(hdc, oldPen);
	SelectObject(hdc, oldBrush);
	DeleteObject(brush);
	DeleteObject(pen);
	DeleteObject(bigFont);
	DeleteObject(smallFont);
}
