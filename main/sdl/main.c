#ifdef DREAMCAST
#include <kos.h>
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "shared.h"
#include "drawing.h"
#include "hack.h"

void exit_oswan();
extern void mixaudioCallback(void *userdata, unsigned char *stream, int len);
#ifdef SWITCHING_GRAPHICS
	extern void screen_prepback(SDL_Surface *s);
#else
	extern void screen_putskin(SDL_Surface *s, unsigned char *bmpBuf, unsigned int bmpSize);
#endif

#ifdef PSP
#include <pspkernel.h>
#include <pspdisplay.h>
#include <psppower.h>
PSP_MODULE_INFO("OSWAN", 0, 1, 1);
PSP_HEAP_SIZE_MAX();
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
int exit_callback(int arg1, int arg2, void *common);
int CallbackThread(SceSize args, void *argp);
int SetupCallbacks(void);
int exit_callback(int arg1, int arg2, void *common) 
{
	exit_oswan(); return 0;
}
int CallbackThread(SceSize args, void *argp) 
{
	int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);	sceKernelSleepThreadCB();
	return 0;
}
int SetupCallbacks(void) 
{
	int thid = sceKernelCreateThread("CallbackThread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if (thid >= 0) sceKernelStartThread(thid, 0, 0); return thid;
}
#endif

unsigned int m_Flag;
unsigned int interval;

gamecfg GameConf;
char gameName[512];
char current_conf_app[MAX__PATH];

unsigned long nextTick, lastTick = 0, newTick, currentTick, wait;
unsigned char FPS = 60; 
unsigned char pastFPS = 0; 

SDL_Surface *actualScreen, *screenshots;

unsigned long SDL_UXTimerRead(void) 
{
	struct timeval tval; /* timing	*/
  	gettimeofday(&tval, 0);
	return (((tval.tv_sec*1000000) + (tval.tv_usec )));
}

void graphics_paint(void) 
{
	SDL_LockSurface(actualScreen);
	screen_draw();
	SDL_UnlockSurface(actualScreen);

	pastFPS++;
	newTick = SDL_UXTimerRead();
	if ((newTick-lastTick)>1000000) {
		FPS = pastFPS;
		pastFPS = 0;
		lastTick = newTick;
	}

	flip_screen(actualScreen);
}

void initSDL(void) 
{
	SDL_Init(SDL_INIT_VIDEO);
	
	/* Get current resolution, does nothing on Windowed or bare metal platroms*/
	Get_resolution();
	
	SDL_ShowCursor(SDL_DISABLE);
	SetVideo(0);

#ifdef SOUND_ON
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec fmt, retFmt;
	
	/*	Set up SDL sound */
	fmt.freq = 44100;   
    fmt.format = AUDIO_S16SYS;
    fmt.channels = 2;
    fmt.samples = 2048;
    fmt.callback = mixaudioCallback;
    fmt.userdata = NULL;

    /* Open the audio device and start playing sound! */
    if ( SDL_OpenAudio(&fmt, &retFmt) < 0 )
	{
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        exit(1);
    }
#endif
}


int main(int argc, char *argv[]) 
{
	double period;
	
#ifdef _TINSPIRE
	enable_relative_paths(argv);
#endif

#ifdef PSP
	SetupCallbacks();
	scePowerSetClockFrequency(333, 333, 166);
#endif
	
	/* Init graphics & sound	*/
	initSDL();
	
#ifdef JOYSTICK
	SDL_Init(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_ENABLE);
#endif

	getcwd(current_conf_app, MAX__PATH);
	
#if defined(HOME_SUPPORT)
	char home_path[256];
	snprintf(home_path, sizeof(home_path), "%s/.oswan", PATH_DIRECTORY);
	mkdir(home_path, 0755);	
	snprintf(current_conf_app, sizeof(current_conf_app), "%s/.oswan/oswan.cfg", PATH_DIRECTORY);
#else
	snprintf(current_conf_app, sizeof(current_conf_app), "%soswan.cfg%s", PATH_DIRECTORY, EXTENSION);
#endif
	
	m_Flag = GF_MAINUI;
	system_loadcfg(current_conf_app);

	SDL_WM_SetCaption("Oswan", NULL);

    /*	load rom file via args if a rom path is supplied	*/
	strcpy(gameName,"");
	
	if(argc > 1) 
	{
#ifdef SWITCHING_GRAPHICS
		if (!GameConf.m_ScreenRatio) SetVideo(1);
#endif
		flip_screen(actualScreen);
		strcpy(gameName,argv[1]);
		m_Flag = GF_GAMEINIT;
	}

	while (m_Flag != GF_GAMEQUIT) 
	{
		switch (m_Flag) 
		{
			case GF_MAINUI:
				#ifdef SOUND_ON
				SDL_PauseAudio(1);
				#endif
				screen_showtopmenu();
				if (cartridge_IsLoaded()) 
				{
					#ifdef SOUND_ON
					SDL_PauseAudio(0);
					#endif
				}
				nextTick = SDL_UXTimerRead() + interval;
				break;

			case GF_GAMEINIT:
			
				Set_DrawRegion();
				
				if (WsCreate(gameName)) 
				{
					WsInit();
					m_Flag = GF_GAMERUNNING;
					#ifdef SOUND_ON
					SDL_PauseAudio(0);
					#endif
					/* Init timing */
					period = 1.0 / 60;
					period = period * 1000000;
					interval = (int) period;
					nextTick = SDL_UXTimerRead() + interval;
				}
				else 
				{
					fprintf(stderr,"Can't load %s : %s", gameName, SDL_GetError()); fflush(stderr);
					m_Flag = GF_GAMEQUIT;
				}
				break;
		
			case GF_GAMERUNNING:	
				currentTick = SDL_UXTimerRead(); 
				#ifndef NO_WAIT
				wait = (nextTick - currentTick);
				if (wait > 0) 
				{
					if (wait < 1000000) 
					{
						SDL_Delay(wait/1000);
					}
				}
				#endif
				WsRun();
				nextTick += interval;
				break;
		}
	}
	
	exit_oswan();
	return 0;
}

void exit_oswan()
{
	#ifdef SOUND_ON
		SDL_PauseAudio(1);
	#endif

	/* Free memory	*/
	#ifndef NOSCREENSHOTS
	if (screenshots != NULL) SDL_FreeSurface(screenshots);
	#endif
	if (actualScreen != NULL) SDL_FreeSurface(actualScreen);
	#if defined(SCALING)
	if (real_screen != NULL) SDL_FreeSurface(real_screen);
	#endif
		
	SDL_QuitSubSystem(SDL_INIT_VIDEO);	
	#ifdef SOUND_ON
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	#endif
	
	SDL_Quit();
	
#ifdef PSP
	sceDisplayWaitVblankStart();
	sceKernelExitGame(); 	
#endif
	
	exit(0);
}