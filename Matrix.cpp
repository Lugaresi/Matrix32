/* xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Matrix -- simulate the text scrolls from the movie "The Matrix".
 *
 * The movie people distribute their own Windows/Mac screensaver that does
 * a similar thing, so I wrote one for Unix.  However, that version (the
 * Windows/Mac version at http://www.whatisthematrix.com/) doesn't match my
 * memory of what the screens in the movie looked like, so my `xmatrix'
 * does things differently.
 *
 * ----------------------------------------------------------------------------
 *
 * Windows port by Craig Boston.  I liked the X screensaver better than the
 * "official" one so I decided to steal the code and convert it to a Win32
 * screensaver to use on my NT (actually Win2k) box at work.  Plus I wanted
 * multimonitor support so it would REALLY be like in the movie :-)
 *
 * The original code is mostly intact; only the X drawing/window code has been
 * removed and replaced with Win32 GDI calls.  Anything else that I changed I
 * tried to include an option to turn it on or off in the configuration dialog.
 *
 * I think jwz's comments are all C-style (/ * * /), whereas I used C++
 * style ( // ) wherever possible so the difference would be noticible.
 */

#include "stdafx.h"				// funky MS predefined header crap.  whatever...

#define VendorName "Pointless Stuff Unlimited"		// predefined stuff
#define AppName "Matrix"							// for registry access
#define RegKeyName "Software\\" VendorName "\\" AppName

extern HINSTANCE hMainInstance;	// screen saver instance handle

#define CHAR_ROWS 27			// Defaults straight from XMatrix
#define CHAR_COLS 3				// I converted the font XPMs directly to
#define FADE_COL  0				// paletted BMPs (check out the .rc file)
#define PLAIN_COL 1				// with ImageMagik and Corel PhotoPaint 9
#define GLOW_COL  2				// (I wanted fine control over the palette)

typedef struct {
  unsigned int glyph   : 8;
           int glow    : 8;
  unsigned int changed : 1;
  unsigned int spinner : 1;
} m_cell;

typedef struct {
  int remaining;
  int throttle;
  int y;
} m_feeder;

typedef struct {
	int size;				// bool actually but I hate bools
	int style;				// scroll style: 0 = sliding (top), 1 = growing (bottom), 2 = both
	int density;			// hmm, i wonder what this could be?
	int spinners;
	int randglow, glowrate;
	int phosphor;			// enable phosphor trails
	int norules;			// break all the rules! :-)
	long delay;				// milliseconds since windows timers aren't quite as high-resoltuion :-)
} m_config;					// new structure for holding config data to/from the registry

typedef struct {
	HWND hwnd;
	RECT screen;
	HDC hDC;
	HDC TempBitmap;
	UINT timer;
	HBRUSH black;

	m_config config;

	int grid_width, grid_height;
	int char_width, char_height;
	m_cell *cells;
	m_feeder *feeders;
	bool insert_top_p, insert_bottom_p;
	int density;

	HBITMAP images;
	int image_width, image_height;
	int nglyphs;
} m_state;

BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
	INITCOMMONCONTROLSEX icx;
	icx.dwSize = sizeof(INITCOMMONCONTROLSEX);	// couldn't they have picked a shorter struct name...?
	icx.dwICC = ICC_WIN95_CLASSES;				// just want classes for the slider
	InitCommonControlsEx(&icx);					// the hoops we have to jump through...
    return TRUE;								// all done here...
}

BOOL WINAPI ReadConfigFromRegistry(m_config *config)
{
	HKEY hMatrix;
	DWORD dummy;			// don't really care if the key exists or not, just open it
	if (RegCreateKeyEx(HKEY_CURRENT_USER, RegKeyName, 0, "", 0, KEY_READ | KEY_WRITE,
		NULL, &hMatrix, &dummy))
	{
		// hmm, couldn't open key...  User might be able to do something about this
		// so we'll go ahead and report it
		MessageBox(NULL, "Error opening/creating registry key.  Check that you "
			"are not out of hard disk space and have permission to write to the "
			"HKEY_CURRENT_USER key (if running Windows NT).  Other than that, good "
			"luck :-/", "Matrix", MB_OK | MB_ICONSTOP);

		return FALSE;		// still can't recover...
	}

	LONG err;
	DWORD type, size;
	size = sizeof(DWORD);	// should be 4, but you never know (Win64? hehe)
	err = RegQueryValueEx(hMatrix, "Size", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->size = dummy;
	else config->size = 1;	// default = large

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "Style", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->style = dummy;
	else config->style = 2;	// default = both

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "Spinners", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->spinners = dummy;
	else config->spinners = 5;

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "Density", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->density = dummy;
	else config->density = 40;	// changed slightly from the X version

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "Delay", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->delay = dummy;
	else config->delay = 35;

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "RandGlow", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->randglow = dummy;
	else config->randglow = 1;	// default = yes

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "GlowRate", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->glowrate = dummy;
	else config->glowrate = 10;		// arbitrary units

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "Phosphor", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->phosphor = dummy;
	else config->phosphor = 1;		// of course we want the cool stuff!

	size = sizeof(DWORD);
	err = RegQueryValueEx(hMatrix, "NoRules", 0, &type, (UCHAR*)&dummy, &size);
	if (!err && (type == REG_DWORD)) config->norules = dummy;
	else config->norules = 0;	// yeaaaah... breakin' the law! breakin' the laaaaw!

	RegCloseKey(hMatrix);	// done with the registry
	return TRUE;			// and we're outta here
}

BOOL WINAPI WriteConfigToRegistry(m_config *config)
{
	HKEY hMatrix;
	DWORD dummy;			// don't really care if the key exists or not, just open it
	if (RegCreateKeyEx(HKEY_CURRENT_USER, RegKeyName, 0, "", 0, KEY_READ | KEY_WRITE,
		NULL, &hMatrix, &dummy))
	{
		// hmm, couldn't open key...  User might be able to do something about this
		// so we'll go ahead and report it
		MessageBox(NULL, "Error opening/creating registry key.  Check that you "
			"are not out of hard disk space and have permission to write to the "
			"HKEY_CURRENT_USER key (if running Windows NT).  Other than that, good "
			"luck :-/", "Matrix", MB_OK | MB_ICONSTOP);

		return FALSE;		// still can't recover...
	}

	dummy = config->size;
	RegSetValueEx(hMatrix, "Size", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->style;
	RegSetValueEx(hMatrix, "Style", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->spinners;
	RegSetValueEx(hMatrix, "Spinners", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->density;
	RegSetValueEx(hMatrix, "Density", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->delay;
	RegSetValueEx(hMatrix, "Delay", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->randglow;
	RegSetValueEx(hMatrix, "RandGlow", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->glowrate;
	RegSetValueEx(hMatrix, "GlowRate", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->phosphor;
	RegSetValueEx(hMatrix, "Phosphor", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	dummy = config->norules;
	RegSetValueEx(hMatrix, "NoRules", 0, REG_DWORD, (UCHAR*)&dummy, sizeof(DWORD));

	RegCloseKey(hMatrix);	// done with the registry
	return TRUE;			// and we're outta here
}

// code I blatently stole from the Microsoft sample, but who cares? :-)
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{
	static HWND hLarge, hSmall;				// static handles to all our controls for
	static HWND hSliding, hGrowing, hBoth;	// easy reference later...
	static HWND hDensity;
	static HWND hSpinners;
	static HWND hSpeed;
	static HWND hDelay;
	static HWND hRandGlow;
	static HWND hGlowRateText;
	static HWND hGlowRate;
	static HWND hPhosphor;
	static HWND hNoRules;

	long dummy;

	switch(message) {
	case WM_INITDIALOG:
		hLarge = GetDlgItem(hDlg, IDC_LARGE);	// get handles to all our controls...
		hSmall = GetDlgItem(hDlg, IDC_SMALL);
		hSliding = GetDlgItem(hDlg, IDC_SLIDING);
		hGrowing = GetDlgItem(hDlg, IDC_GROWING);
		hBoth = GetDlgItem(hDlg, IDC_BOTH);
		hDensity = GetDlgItem(hDlg, IDC_DENSITY);
		hSpinners = GetDlgItem(hDlg, IDC_SPINNERS);
		hSpeed = GetDlgItem(hDlg, IDC_SPEED);
		hDelay = GetDlgItem(hDlg, IDC_DELAY);
		hRandGlow = GetDlgItem(hDlg, IDC_RANDGLOW);
		hGlowRateText = GetDlgItem(hDlg, IDC_GLOWRATETEXT);
		hGlowRate = GetDlgItem(hDlg, IDC_GLOWRATE);
		hPhosphor = GetDlgItem(hDlg, IDC_PHOSPHOR);
		hNoRules = GetDlgItem(hDlg, IDC_NORULES);

		// Quick sanity check...
		if (!(hLarge && hSmall && hSliding && hGrowing && hBoth && hSpeed && hDelay
			&& hRandGlow && hGlowRateText && hGlowRate && hPhosphor && hNoRules))
		{
			// well if the controls don't exist it's probably some freaky low memory condition
			// and we (or the user) won't be able to do much anyway...

			// screen savers have no standard output or error, so we'll just have to hope somebody
			// has a debugger running...
			OutputDebugString("Matrix: (panic) Unable to get handles to dialog controls.\r\n");
			// yeah, CR/LF reminds me of the good ol' DOS days :-)

			return FALSE;		// hopefully will be able to shut down without crashing
		}

		// set up the slider
		SendMessage(hSpeed, TBM_SETRANGE, FALSE, MAKELONG(0, 250));
		SendMessage(hSpeed, TBM_SETPAGESIZE, 0, 5);

		m_config config;
		if (!ReadConfigFromRegistry(&config))				// try to read configuration
			return FALSE;									// only proceed if we succeed

		char buffer[64];	// hopefully we shouldn't have more than 64 digit numbers...

		SendMessage(config.size ? hLarge : hSmall, BM_SETCHECK, BST_CHECKED, NULL);
		SendMessage(config.style == 0 ? hSliding : (config.style == 1 ? hGrowing : hBoth),
			BM_SETCHECK, BST_CHECKED, NULL);		// hehe, this one's fun :-)

		_itoa(config.density, buffer, 10);			// possible buffer overflow, but what can I do?
		SendMessage(hDensity, WM_SETTEXT, 0, (LPARAM)buffer);

		_itoa(config.spinners, buffer, 10);			// well shouldn't be possible.  unless int changes...
		SendMessage(hSpinners, WM_SETTEXT, 0, (LPARAM)buffer);

		long pos;
		pos = 250 - config.delay;
		if (pos > 250) pos = 250;	// huh...?  negative delay?
		if (pos < 0) pos = 0;		// now this one's possible
		SendMessage(hSpeed, TBM_SETPOS, TRUE, pos);

		_itoa(config.delay, buffer, 10);			// yeah, yeah, damn unchecked buffer again
		SendMessage(hDelay, WM_SETTEXT, 0, (LPARAM)buffer);

		if (config.randglow)
		{
			SendMessage(hRandGlow, BM_SETCHECK, BST_CHECKED, NULL);
			EnableWindow(hGlowRateText, TRUE);
			EnableWindow(hGlowRate, TRUE);
		}

		_itoa(config.glowrate, buffer, 10);
		SendMessage(hGlowRate, WM_SETTEXT, 0, (LPARAM)buffer);

		if (config.phosphor)
			SendMessage(hPhosphor, BM_SETCHECK, BST_CHECKED, NULL);

		if (config.norules)
			SendMessage(hNoRules, BM_SETCHECK, BST_CHECKED, NULL);

		return TRUE;			// everything's fine here

	case WM_HSCROLL:			// slider tracking
		pos = SendMessage(hSpeed, TBM_GETPOS, 0, 0);
		pos = 250 - pos;		// what is this, scheme?
		_itoa(pos, buffer, 10);
		SendMessage(hDelay, WM_SETTEXT, 0, (LPARAM)buffer);

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_RANDGLOW:		// set state of other boxes...
			dummy = (SendMessage(hRandGlow, BM_GETCHECK, 0, 0) == BST_CHECKED);
			EnableWindow(hGlowRateText, dummy ? TRUE : FALSE);
			EnableWindow(hGlowRate, dummy ? TRUE : FALSE);
			break;
		case IDOK:				// save settings
			dummy = SendMessage(hLarge, BM_GETCHECK, 0, 0);
			if (dummy == BST_CHECKED)
				config.size = 1;
			else
				config.size = 0;

			dummy = SendMessage(hBoth, BM_GETCHECK, 0, 0);
			if (dummy == BST_CHECKED)
				config.style = 2;
			else
			{
				dummy = SendMessage(hGrowing, BM_GETCHECK, 0, 0);
				if (dummy == BST_CHECKED)
					config.style = 1;
				else
					config.style = 0;
			}

			SendMessage(hDensity, WM_GETTEXT, 64, (LPARAM)buffer);	// HA!  No overflow possibility this time! :-)
			config.density = atoi(buffer);

			SendMessage(hSpinners, WM_GETTEXT, 64, (LPARAM)buffer);
			config.spinners = atoi(buffer);

			SendMessage(hDelay, WM_GETTEXT, 64, (LPARAM)buffer);
			config.delay = atoi(buffer);

			dummy = SendMessage(hRandGlow, BM_GETCHECK, 0, 0);
			if (dummy == BST_CHECKED)
				config.randglow = 1;
			else
				config.randglow = 0;

			SendMessage(hGlowRate, WM_GETTEXT, 64, (LPARAM)buffer);
			config.glowrate = atoi(buffer);

			dummy = SendMessage(hPhosphor, BM_GETCHECK, 0, 0);		// a kludge, i know
			if (dummy == BST_CHECKED)
				config.phosphor = 1;
			else
				config.phosphor = 0;

			dummy = SendMessage(hNoRules, BM_GETCHECK, 0, 0);
			if (dummy == BST_CHECKED)
				config.norules = 1;
			else
				config.norules = 0;

			WriteConfigToRegistry(&config);
		case IDCANCEL:			// ending the dialog either way so just fall through
			EndDialog(hDlg, LOWORD(wParam) == IDOK);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

/* Finally -- the fun stuff :-) */

void load_images(m_state *state)
{
	state->images = LoadBitmap(hMainInstance, MAKEINTRESOURCE(state->config.size ? IDB_MATRIX : IDB_MATRIX2));
	char *bmp;
	int size = GetObject(state->images, 0, NULL);
	bmp = (char*)malloc(size);			// risky, but it works
	GetObject(state->images, size, bmp);
	
	state->image_width = ((BITMAP*)bmp)->bmWidth;
	state->image_height = ((BITMAP*)bmp)->bmHeight;
	state->nglyphs = CHAR_ROWS;

	free (bmp);
}

void init_spinners(m_state *state)
{
	int i = state->config.spinners;
	int x, y;
	m_cell *cell;

	for (y = 0; y < state->grid_height; y++)
		for (x = 0; x < state->grid_width; x++)
		{
			cell = &state->cells[state->grid_width * y + x];
			cell->spinner = 0;
		}

	while (--i > 0)
	{
		x = rand() % state->grid_width;
		y = rand() % state->grid_height;
		cell = &state->cells[state->grid_width * y + x];
		cell->spinner = 1;
	}
}

m_state *init_matrix(HWND hwnd)
{
	m_state *state = (m_state *) calloc (sizeof(*state), 1);	// why not malloc?
	state->hwnd = hwnd;
	GetClientRect(hwnd, &state->screen);
	ReadConfigFromRegistry(&state->config);

	load_images(state);
	state->char_width = state->image_width / CHAR_COLS;
	state->char_height = state->image_height / CHAR_ROWS;

	state->grid_width = (state->screen.right - state->screen.left) / state->char_width;
	state->grid_height = (state->screen.bottom - state->screen.top) / state->char_height;
	state->grid_width++;
	state->grid_height++;

	state->cells = (m_cell *)
		calloc (sizeof(m_cell), state->grid_width * state->grid_height);
	state->feeders = (m_feeder *) calloc (sizeof(m_feeder), state->grid_width);

	if (state->config.style == 0)
	{
		state->insert_top_p = true;
		state->insert_bottom_p = false;
	}
	else if (state->config.style == 1)
	{
		state->insert_top_p = false;
		state->insert_bottom_p = true;
	}
	else
	{
		state->insert_top_p = true;
		state->insert_bottom_p = true;
	}
	state->density = state->config.density;			// kind of redundant but I'm too lazy to change the code

	init_spinners(state);
	return state;
}

void insert_glyph (m_state *state, int glyph, int x, int y)
{
	bool bottom_feeder_p = (y >= 0);
	m_cell *from, *to;

	if (y >= state->grid_height)
		return;

	if (bottom_feeder_p)
	{
		to = &state->cells[state->grid_width * y + x];
	}
	else
	{
		for (y = state->grid_height-1; y > 0; y--)
		{
			from = &state->cells[state->grid_width * (y-1) + x];
			to   = &state->cells[state->grid_width * y     + x];
			if (state->config.phosphor && to->glyph && !from->glyph)	// check for trailing glyph
			{															// (phosphor trail)
				to->glow = -1;						// don't remove the glyph, but instead fade it
			}
			else
			{
				to->glyph   = from->glyph;
				to->glow    = from->glow;
			}
			to->changed = 1;
		}
		to = &state->cells[x];
	}

	to->glyph = glyph;
	to->changed = 1;

	if (!to->glyph)
		;
	else if (bottom_feeder_p)
		to->glow = 1 + (rand() % 2);
	else
		to->glow = 0;
}

void feed_matrix (m_state *state)
{
	int x;

	/* Update according to current feeders. */
	for (x = 0; x < state->grid_width; x++)
	{
		m_feeder *f = &state->feeders[x];

		if (f->throttle)		/* this is a delay tick, synced to frame. */
		{
			f->throttle--;
		}
		else if (f->remaining > 0)	/* how many items are in the pipe */
		{
			int g = (rand() % state->nglyphs) + 1;
			insert_glyph (state, g, x, f->y);
			f->remaining--;
			if (f->y >= 0)  /* bottom_feeder_p */
				f->y++;
		}
		else				/* if pipe is empty, insert spaces */
		{
			insert_glyph (state, 0, x, f->y);
			if (f->y >= 0)  /* bottom_feeder_p */
			f->y++;
		}

		if ((rand() % 10) == 0)		/* randomly change throttle speed */
		{
			f->throttle = ((rand() % 5) + (rand() % 5));
		}
	}
}

int densitizer (m_state *state)
{
  /* Horrid kludge that converts percentages (density of screen coverage)
     to the parameter that actually controls this.  I got this mapping
     empirically, on a 1024x768 screen.  Sue me. */
  if      (state->density < 10) return 85;
  else if (state->density < 15) return 60;
  else if (state->density < 20) return 45;
  else if (state->density < 25) return 25;
  else if (state->density < 30) return 20;
  else if (state->density < 35) return 15;
  else if (state->density < 45) return 10;
  else if (state->density < 50) return 8;
  else if (state->density < 55) return 7;
  else if (state->density < 65) return 5;
  else if (state->density < 80) return 3;
  else if (state->density < 90) return 2;
  else return 1;
}

void hack_matrix (m_state *state)
{
	int x;

	/* Glow some characters. */
//	if (!state->insert_bottom_p)					// not sure why this was here -- i like the
//	{												// random glow, so out it goes! :-)
	if (state->config.randglow)
	{
		int i = rand() % ((state->grid_width / 2) * state->config.glowrate) / 10;
		// implemented glow rate here -- just an arbitary value to multiply by
		while (--i > 0)
		{
			int x = rand() % state->grid_width;
			int y = rand() % state->grid_height;
			m_cell *cell = &state->cells[state->grid_width * y + x];
			if (cell->glyph && cell->glow == 0)
			{
				cell->glow = rand() % 20;			// increased 2* from XMatrix
				cell->changed = 1;
			}
		}
	}

	/* Change some of the feeders. */
	for (x = 0; x < state->grid_width; x++)
	{
		m_feeder *f = &state->feeders[x];
		bool bottom_feeder_p;

		if (!state->config.norules && f->remaining > 0)	/* never change if pipe isn't empty */
			continue;			// well why the hell not?  it's much more interesting this way :-)

		if ((rand() % densitizer(state)) != 0) /* then change N% of the time */
			continue;

		f->remaining = 3 + (rand() % state->grid_height);
		f->throttle = ((rand() % 5) + (rand() % 5));

		if ((rand() % 4) != 0)
			f->remaining = 0;

		if (state->insert_top_p && state->insert_bottom_p)
			bottom_feeder_p = (rand() & 1);
		else
			bottom_feeder_p = state->insert_bottom_p;

		if (bottom_feeder_p)
			f->y = rand() % (state->grid_height / 2);
		else
		f->y = -1;
	}

	if (! (rand() % 500))
		init_spinners (state);
}

void draw_matrix (m_state *state)
{
	int x, y;
	int count = 0;

	feed_matrix (state);
	hack_matrix (state);

	for (y = 0; y < state->grid_height; y++)
		for (x = 0; x < state->grid_width; x++)
		{
			m_cell *cell = &state->cells[state->grid_width * y + x];

			if (cell->glyph)
				count++;

			if (!cell->changed)
				continue;

			if (cell->glyph == 0)
			{
				RECT rc;
				rc.left = x * state->char_width;
				rc.top = y * state->char_height;
				rc.right = rc.left + state->char_width;
				rc.bottom = rc.top + state->char_height;
				FillRect(state->hDC, &rc, state->black);
			}
			else
			{
				BitBlt(state->hDC,
					x * state->char_width,
					y * state->char_height,
					state->char_width,
					state->char_height,
					state->TempBitmap,
					((cell->glow > 0 || cell->spinner)
						? (state->char_width * GLOW_COL)
						: (cell->glow == 0
							? (state->char_width * PLAIN_COL)
							: (state->char_width * FADE_COL))),
					(cell->glyph - 1) * state->char_height,
					SRCCOPY);
			}

			cell->changed = 0;

			if (cell->glow > 0)
			{
				cell->glow--;
				cell->changed = 1;
			}
			else if (cell->glow < 0)
			{
				cell->glow++;
				if (cell->glow == 0)
					cell->glyph = 0;
				cell->changed = 1;
			}

			if (cell->spinner)
			{
				cell->glyph = rand() % CHAR_ROWS;
				cell->changed = 1;
			}
		}

}

LONG WINAPI ScreenSaverProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
	static m_state *state = NULL;
	RECT rc;
	HDC hdc;

	switch(message) {
	case WM_CREATE:
		srand(time(NULL));
//		MoveWindow(hwnd, 1152, 144, 1024, 768, true);
		state = init_matrix(hwnd);			// config gets read here too
		state->timer = SetTimer(hwnd, 1, state->config.delay, NULL);
		state->black = (HBRUSH)GetStockObject(BLACK_BRUSH);		// tiny speedup
		break;

	case WM_ERASEBKGND:
		hdc = GetDC(hwnd);
		GetClientRect(hwnd, &rc);
		FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
		ReleaseDC(hwnd, hdc);
		break;

	case WM_TIMER:
		hdc = GetDC(hwnd);		// do this in the timer just in case the user somehow
		state->hDC = hdc;		// changes pixel depths while the screen saver is running
								// (tell me how!)
								// still a big benefit doing it here instead of the inner loop
		state->TempBitmap = CreateCompatibleDC(state->hDC);		// ahh, the fun of Windows GDI
		SelectObject(state->TempBitmap, state->images);			// sometimes better than X, sometimes worse

		draw_matrix(state);				// <------- FUN HAPPENS HERE!

		DeleteDC(state->TempBitmap);	// now how do you know which one to use?
		ReleaseDC(hwnd, state->hDC);	// :-)
		state->hDC = NULL;
		state->TempBitmap = NULL;
		break;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		DeleteObject(state->images);	// nuke the font so we don't leak resources on Win95/Win32S

		free(state->cells);				// not really necessary,
		free(state->feeders);			// but it's good form
		free(state);
		state = NULL;
		break;
	}
    return DefScreenSaverProc(hwnd, message, wParam, lParam); 
}
