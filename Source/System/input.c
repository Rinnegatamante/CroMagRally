/****************************/
/*   	  INPUT.C	   	    */
/* (c)2000 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include <stdlib.h>
#include <DrawSprocket.h>
#include <InputSprocket.h>
#include <CursorDevices.h>
#include <Traps.h>
#include <TextUtils.h>
#include <FixMath.h>
#include "globals.h"
#include "misc.h"
#include "input.h"
#include "windows.h"
#include "ogl_support.h"
#include "miscscreens.h"
#include "main.h"
#include "player.h"
#include "file.h"

extern	short				gMainAppRezFile,gMyNetworkPlayerNum,gNumLocalPlayers;
extern	DSpContextReference gDisplayContext;
extern	Byte				gActiveSplitScreenMode;
extern	PlayerInfoType		gPlayerInfo[];
extern	Boolean				gOSX,gQuitting;
extern	AGLContext		gAGLContext;
extern	AGLDrawable		gAGLWin;
extern	float			gFramesPerSecondFrac;
extern	PrefsType			gGamePrefs;

/**********************/
/*     PROTOTYPES     */
/**********************/

static void MyGetKeys(KeyMap *keyMap);
static void GetLocalKeyStateForPlayer(short playerNum, Boolean secondaryControls);

static void DoMyKeyboardEdit(void);
static pascal Boolean DialogCallback (DialogRef dp,EventRecord *event, short *item);
static void SetKeyControl(short item, u_short keyCode);
static void SetMyKeyEditDefaults(void);
static short GetFirstRealKeyPressed(void);
static void KeyCodeToChar(UInt16 code, Str32 s);


/****************************/
/*    CONSTANTS             */
/****************************/



/**********************/
/*     VARIABLES      */
/**********************/


KeyMap gKeyMap,gNewKeys,gOldKeys,gKeyMap_Real,gNewKeys_Real,gOldKeys_Real;

KeyMap gKeyMapP,gNewKeysP,gOldKeysP;			// for Push/Pop functions below


Boolean	gISpActive 				= false;
Boolean	gISPInitialized			= false;

float	gAnalogSteeringTimer[MAX_PLAYERS] = {0,0,0,0,0,0};


		/* CONTORL NEEDS */



static ISpNeed	gControlNeeds[NUM_CONTROL_NEEDS] =
{
/*----------------- PLAYER 1 -----------------------*/

	{										// 0
		"Player 1: Steering Wheel",
		128,
		1,
		0,
		kISpElementKind_Axis,
		kISpElementLabel_Axis_XAxis,
		0,
		0,
		0,
		0,
	},


	{										// 1
		"Player 1: Forward",
		131,
		1,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_MoveForward,
		0,
		0,
		0,
		0
	},

	{										// 2
		"Player 1: Reverse",
		136,
		1,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_MoveBackward,
		0,
		0,
		0,
		0
	},

	{
		"Player 1: Brakes",				// 3
		138,
		1,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_Select,
		0,
		0,
		0,
		0
	},


	{
		"Player 1: Weapon Forward",			// 4
		140,
		1,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_Fire,
		0,
		0,
		0,
		0
	},

	{
		"Player 1: Weapon Backward",		// 5
		141,
		1,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_SecondaryFire,
		0,
		0,
		0,
		0
	},



	{
		"Player 1:  Camera Mode",			// 6
		144,
		1,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_LookDown,
		0,
		0,
		0,
		0
	},


/*----------------- PLAYER 2 -----------------------*/


	{										// 7
		"Player 2: Steering Wheel",
		128,
		2,
		0,
		kISpElementKind_Axis,
		kISpElementLabel_Axis_XAxis,
		0,
		0,
		0,
		0,
	},


	{										// 8
		"Player 2: Forward",
		131,
		2,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_MoveForward,
		0,
		0,
		0,
		0
	},

	{										// 9
		"Player 2: Reverse",
		136,
		2,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_MoveBackward,
		0,
		0,
		0,
		0
	},

	{
		"Player 2: Brakes",				// 10
		138,
		2,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_Select,
		0,
		0,
		0,
		0
	},

	{
		"Player 2: Weapon Forward",		// 11
		140,
		2,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_Fire,
		0,
		0,
		0,
		0
	},

	{										// 12
		"Player 2: Weapon Backward",
		141,
		2,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_SecondaryFire,
		0,
		0,
		0,
		0
	},



	{										// 13
		"Player 2:  Camera Mode",
		144,
		2,
		0,
		kISpElementKind_Button,
		kISpElementLabel_Btn_LookDown,
		0,
		0,
		0,
		0
	},




};


ISpElementReference	gVirtualElements[NUM_CONTROL_NEEDS];


static const u_short	gNeedToKey[NUM_CONTROL_NEEDS] =				// table to convert need # into key equate value
{
	(kKey_Left_P1<<8) | kKey_Right_P1,	// steer p1
	kKey_Forward_P1,
	kKey_Backward_P1,
	kKey_Brakes_P1,
	kKey_ThrowForward_P1,
	kKey_ThrowBackward_P1,
	kKey_CameraMode_P1,

	(kKey_Left_P2<<8) | kKey_Right_P2,	// steer p2
	kKey_Forward_P2,
	kKey_Backward_P2,
	kKey_Brakes_P2,
	kKey_ThrowForward_P2,
	kKey_ThrowBackward_P2,
	kKey_CameraMode_P2
};

static u_short	gUserKeySettings[NUM_CONTROL_NEEDS];

const u_short	gUserKeySettings_Defaults[NUM_CONTROL_NEEDS] =
{
	(kKey_Left_P1<<8) | kKey_Right_P1,	// steer p1
	kKey_Forward_P1,
	kKey_Backward_P1,
	kKey_Brakes_P1,
	kKey_ThrowForward_P1,
	kKey_ThrowBackward_P1,
	kKey_CameraMode_P1,

	(kKey_Left_P2<<8) | kKey_Right_P2,	// steer p2
	kKey_Forward_P2,
	kKey_Backward_P2,
	kKey_Brakes_P2,
	kKey_ThrowForward_P2,
	kKey_ThrowBackward_P2,
	kKey_CameraMode_P2
};



/**********************************************************************/


static const gControlBitToKey[NUM_CONTROL_BITS][2] = {						// remember to update kControlBit_XXX list!!!
														kKey_ThrowForward_P1, 	kKey_ThrowForward_P2,
														kKey_ThrowBackward_P1, 	kKey_ThrowBackward_P2,
														kKey_Brakes_P1, 		kKey_Brakes_P2,
														kKey_CameraMode_P1, 	kKey_CameraMode_P2,
														kKey_Forward_P1,		kKey_Forward_P2,
														kKey_Backward_P1,		kKey_Backward_P2
													};




/************************* INIT INPUT *********************************/

void InitInput(void)
{
OSErr				iErr;
ISpDeviceReference	dev[10];
UInt32				count = 0;
int			i;

			/********************************************/
			/* SEE IF USE INPUT SPROCKET OR HID MANAGER */
			/********************************************/

				/* OS X */

	if (gOSX)
	{
		for (i = 0; i < NUM_CONTROL_NEEDS; i++)			// copy key settings from prefs
			gUserKeySettings[i] = gGamePrefs.keySettings[i];

	}
				/* OS 9 */
	else
	{
		ISpStartup();
		gISPInitialized = true;

					/* CREATE NEW NEEDS */

		iErr = ISpElement_NewVirtualFromNeeds(NUM_CONTROL_NEEDS, gControlNeeds, gVirtualElements, 0);
		if (iErr)
		{
			DoAlert("InitInput: ISpElement_NewVirtualFromNeeds failed!");
			ShowSystemErr(iErr);
		}

		iErr = ISpInit(NUM_CONTROL_NEEDS, gControlNeeds, gVirtualElements, 'CavM','Inp8', 0, 1000, 0);
		if (iErr)
		{
			DoAlert("InitInput: ISpInit failed!");
			ShowSystemErr(iErr);
		}


				/* ACTIVATE ALL DEVICES */

		if (ISpDevices_Extract(10,&count,dev) != noErr)
			DoFatalAlert("InitInput: ISpDevices_Extract failed!");

		if (ISpDevices_Activate(count, dev) != noErr)
			DoFatalAlert("InitInput: ISpDevices_Activate failed!");

		gISpActive = true;

				/* DEACTIVATE JUST THE MOUSE SINCE WE DONT NEED THAT */

		ISpDevices_ExtractByClass(kISpDeviceClass_Mouse,10,&count,dev);
		ISpDevices_Deactivate(count, dev);
	}
}


/**************** READ KEYBOARD *************/
//
//
//

void ReadKeyboard(void)
{
			/* READ REAL KEYBOARD & INPUT SPROCKET KEYMAP */
			//
			// Note:  	This will convert ISp data into a KeyMap, so the rest
			// 			of the code looks like its just reading the real keyboard.
			//

	MyGetKeys(&gKeyMap);


			/* CALC WHICH KEYS ARE NEW THIS TIME */

	gNewKeys[0] = (gOldKeys[0] ^ gKeyMap[0]) & gKeyMap[0];
	gNewKeys[1] = (gOldKeys[1] ^ gKeyMap[1]) & gKeyMap[1];
	gNewKeys[2] = (gOldKeys[2] ^ gKeyMap[2]) & gKeyMap[2];
	gNewKeys[3] = (gOldKeys[3] ^ gKeyMap[3]) & gKeyMap[3];



				/* SEE IF QUIT GAME */

	if (!gQuitting)
	{
		if (GetKeyState_Real(KEY_Q) && GetKeyState_Real(KEY_APPLE))			// see if real key quit
			CleanQuit();
	}



			/* SEE IF DO SAFE PAUSE FOR SCREEN SHOTS */

	if (GetNewKeyState_Real(KEY_F12))
	{
		Boolean o = gISpActive;
		TurnOffISp();

		if (gAGLContext)
			aglSetDrawable(gAGLContext, nil);			// diable gl

		do
		{
			EventRecord	e;
			WaitNextEvent(everyEvent,&e, 0, 0);
			ReadKeyboard_Real();
		}while(!GetNewKeyState_Real(KEY_F12));
		if (o)
			TurnOnISp();

		if (gAGLContext)
			aglSetDrawable(gAGLContext, gAGLWin);		// reenable gl

		CalcFramesPerSecond();
		CalcFramesPerSecond();
	}


			/* REMEMBER AS OLD MAP */

	gOldKeys[0] = gKeyMap[0];
	gOldKeys[1] = gKeyMap[1];
	gOldKeys[2] = gKeyMap[2];
	gOldKeys[3] = gKeyMap[3];
}


/**************** READ KEYBOARD_REAL *************/
//
// This just does a simple read of the REAL keyboard (regardless of Input Sprockets)
//

void ReadKeyboard_Real(void)
{
	GetKeys(gKeyMap_Real);

			/* CALC WHICH KEYS ARE NEW THIS TIME */

	gNewKeys_Real[0] = (gOldKeys_Real[0] ^ gKeyMap_Real[0]) & gKeyMap_Real[0];
	gNewKeys_Real[1] = (gOldKeys_Real[1] ^ gKeyMap_Real[1]) & gKeyMap_Real[1];
	gNewKeys_Real[2] = (gOldKeys_Real[2] ^ gKeyMap_Real[2]) & gKeyMap_Real[2];
	gNewKeys_Real[3] = (gOldKeys_Real[3] ^ gKeyMap_Real[3]) & gKeyMap_Real[3];


			/* REMEMBER AS OLD MAP */

	gOldKeys_Real[0] = gKeyMap_Real[0];
	gOldKeys_Real[1] = gKeyMap_Real[1];
	gOldKeys_Real[2] = gKeyMap_Real[2];
	gOldKeys_Real[3] = gKeyMap_Real[3];
}


/****************** GET KEY STATE: REAL ***********/
//
// for data from ReadKeyboard_Real
//

Boolean GetKeyState_Real(unsigned short key)
{
unsigned char *keyMap;

	keyMap = (unsigned char *)&gKeyMap_Real;
	return ( ( keyMap[key>>3] >> (key & 7) ) & 1);
}

/****************** GET NEW KEY STATE: REAL ***********/
//
// for data from ReadKeyboard_Real
//

Boolean GetNewKeyState_Real(unsigned short key)
{
unsigned char *keyMap;

	keyMap = (unsigned char *)&gNewKeys_Real;
	return ( ( keyMap[key>>3] >> (key & 7) ) & 1);
}


/****************** GET KEY STATE ***********/
//
// NOTE: Assumes that ReadKeyboard has already been called!!
//

Boolean GetKeyState(unsigned short key)
{
unsigned char *keyMap;

	keyMap = (unsigned char *)&gKeyMap;
	return ( ( keyMap[key>>3] >> (key & 7) ) & 1);
}


/****************** GET NEW KEY STATE ***********/
//
// NOTE: Assumes that ReadKeyboard has already been called!!
//

Boolean GetNewKeyState(unsigned short key)
{
unsigned char *keyMap;

	keyMap = (unsigned char *)&gNewKeys;
	return ( ( keyMap[key>>3] >> (key & 7) ) & 1);
}

/********************** MY GET KEYS ******************************/
//
// Depending on mode, will either read key map from GetKeys or
// will "fake" a keymap using Input Sprockets.
//

static void MyGetKeys(KeyMap *keyMap)
{
short	i,key,j,q,p;
UInt32	keyState,axisState;
unsigned char *keyBytes;

	ReadKeyboard_Real();												// always read real keyboard anyway

	keyBytes = (unsigned char *)keyMap;
	(*keyMap)[0] = (*keyMap)[1] = (*keyMap)[2] = (*keyMap)[3] = 0;		// clear out keymap

				/********************/
				/* DO HACK FOR OS X */
				/********************/

	if (!gISpActive)
	{
		for (i = 0; i < NUM_CONTROL_NEEDS; i++)
		{
			switch(i)
			{
								/* EMULATE THE ANALOG INPUT FOR STEERING */
				case	0:

						if (GetKeyState_Real((gUserKeySettings[0] & 0xff00) >> 8))
							gPlayerInfo[0].analogSteering = -1.0f;
						else
						if (GetKeyState_Real(gUserKeySettings[0] & 0xff))
							gPlayerInfo[0].analogSteering = 1.0f;
						else
							gPlayerInfo[0].analogSteering = 0.0f;
						break;

				case	7:
						if (GetKeyState_Real((gUserKeySettings[7] & 0xff00) >> 8))
							gPlayerInfo[1].analogSteering = -1.0f;
						else
						if (GetKeyState_Real(gUserKeySettings[7] & 0xff))
							gPlayerInfo[1].analogSteering = 1.0f;
						else
							gPlayerInfo[1].analogSteering = 0.0f;
						break;


							/* HANDLE OTHER KEYS */

				default:
						key = gUserKeySettings[i];
						if (GetKeyState_Real(key))
						{
							j = key>>3;
							q = (1<<(key&7));
							keyBytes[j] |= q;											// set correct bit in keymap
						}
			}
		}
	}
	else
	{

			/***********************************/
			/* POLL NEEDS FROM INPUT SPROCKETS */
			/***********************************/
			//
			// Convert input sprocket info into keymap bits to simulate a real key being pressed.
			//

		for (i = 0; i < NUM_CONTROL_NEEDS; i++)
		{
			switch(gControlNeeds[i].theKind)
			{
					/* AXIS  */

				case	kISpElementKind_Axis:
						ISpElement_GetSimpleState(gVirtualElements[i],&axisState);		// get state of this one

						if (gNumLocalPlayers > 1)										// if 2-player split screen then extract player # from control
							p = gControlNeeds[i].playerNum - 1;							// get p1 or p2
						else															// otherwise single or net play
						{
							if (gControlNeeds[i].playerNum > 1)							// ignore any p2 input data form the device since there is only 1 player on this machine
								break;
							else
								p = gMyNetworkPlayerNum;
						}

						switch (axisState)												// check for specific states
						{
							case	kISpAxisMiddle:
									gPlayerInfo[p].analogSteering = 0.0f;
									gAnalogSteeringTimer[p] -= gFramesPerSecondFrac;
									break;

							case	kISpAxisMinimum:
									gPlayerInfo[p].analogSteering = -1.0f;
									gAnalogSteeringTimer[p] -= gFramesPerSecondFrac;
									break;

							case	kISpAxisMaximum:
									gPlayerInfo[p].analogSteering = 1.0f;
									gAnalogSteeringTimer[p] -= gFramesPerSecondFrac;
									break;

							default:
									gPlayerInfo[p].analogSteering = ((double)axisState - kISpAxisMiddle) / (double)kISpAxisMiddle;
									gAnalogSteeringTimer[p] = 5.0f;

						}
						break;


					/* SIMPLE KEY BUTTON */

				case	kISpElementKind_Button:
						ISpElement_GetSimpleState(gVirtualElements[i],&keyState);		// get state of this one
						if (keyState == kISpButtonDown)
						{
							key = gNeedToKey[i];										// get keymap value for this "need"
							j = key>>3;
							q = (1<<(key&7));
							keyBytes[j] |= q;											// set correct bit in keymap
						}
						break;

			}

		}
	}
}



#pragma mark -

/******************** TURN ON ISP *********************/

void TurnOnISp(void)
{
ISpDeviceReference	dev[10];
UInt32		count = 0;
OSErr		iErr;

	if (!gISPInitialized)
		return;

	if (!gISpActive)
	{
		ISpResume();
		gISpActive = true;

				/* ACTIVATE ALL DEVICES */

		iErr = ISpDevices_Extract(10,&count,dev);
		if (iErr)
			DoFatalAlert("TurnOnISp: ISpDevices_Extract failed!");
		iErr = ISpDevices_Activate(count, dev);
		if (iErr)
			DoFatalAlert("TurnOnISp: ISpDevices_Activate failed!");

			/* DEACTIVATE JUST THE MOUSE SINCE WE DONT NEED THAT */

		ISpDevices_ExtractByClass(kISpDeviceClass_Mouse,10,&count,dev);
		ISpDevices_Deactivate(count, dev);
	}
}

/******************** TURN OFF ISP *********************/

void TurnOffISp(void)
{
ISpDeviceReference	dev[10];
UInt32		count = 0;

	if (!gISPInitialized)
		return;

	if (gISpActive)
	{
				/* DEACTIVATE ALL DEVICES */

		ISpDevices_Extract(10,&count,dev);
		ISpDevices_Deactivate(count, dev);
		ISpSuspend();

		gISpActive = false;
	}
}



/************************ DO KEY CONFIG DIALOG ***************************/
//
// NOTE:  ISp must be turned ON when this is called!
//

void DoKeyConfigDialog(void)
{
	Enter2D(true);

	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());

	if (gOSX)
	{
		DoMyKeyboardEdit();
	}

				/* DO ISP CONFIG DIALOG */
	else
	{
		Boolean	ispWasOn = gISpActive;

		TurnOnISp();
		ISpConfigure(nil);
		if (!ispWasOn)
			TurnOffISp();

	}

	Exit2D();
}


/***************** ARE ANY NEW KEYS PRESSED ****************/

Boolean AreAnyNewKeysPressed(void)
{
	if (gNewKeys_Real[0] || gNewKeys_Real[1] ||  gNewKeys_Real[2] || gNewKeys_Real[3])
		return(true);
	else
		return(false);
}


#pragma mark -

/******************* INIT CONTROL BITS *********************/
//
// Called when each level is initialized.
//

void InitControlBits(void)
{
int	i;

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		gPlayerInfo[i].controlBits = gPlayerInfo[i].controlBits_New = 0;
	}
}



/********************* GET LOCAL KEY STATE *************************/
//
// Called at the end of the Main Loop, after rendering.  Gets the key state of *this* computer
// for the next frame.
//

void GetLocalKeyState(void)
{
		/* SEE IF JUST 1 PLAYER ON THIS COMPUTER */

	if (gActiveSplitScreenMode == SPLITSCREEN_MODE_NONE)
	{
		GetLocalKeyStateForPlayer(gMyNetworkPlayerNum, 0);
	}

		/* GET KEY STATES FOR BOTH PLAYERS */

	else
	{
		GetLocalKeyStateForPlayer(0, 0);
		GetLocalKeyStateForPlayer(1, 1);
	}
}



/*********************** GET LOCAL KEY STATE FOR PLAYER ****************************/
//
// Creates a bitfield of information for the player's key state.
// This bitfield is use throughout the game and over the network for all player controls.
//
// INPUT:  	playerNum 			=	player #0..n
// 			secondaryControsl	=	if doing split-screen player, true to check secondary local controls
//


static void GetLocalKeyStateForPlayer(short playerNum, Boolean secondaryControls)
{
u_long	mask,old;
short	i;

	old = gPlayerInfo[playerNum].controlBits;						// remember old bits
	gPlayerInfo[playerNum].controlBits = 0;							// initialize new bits
	mask = 0x1;


			/* BUILD CURRENT BITFIELD */

	for (i = 0; i < NUM_CONTROL_BITS; i++)
	{
		if (GetKeyState(gControlBitToKey[i][secondaryControls]))	// see if key is down
			gPlayerInfo[playerNum].controlBits |= mask;				// set bit in bitfield

		mask <<= 1;													// shift bit to next position
	}


			/* BUILD "NEW" BITFIELD */

	gPlayerInfo[playerNum].controlBits_New = (gPlayerInfo[playerNum].controlBits ^ old) & gPlayerInfo[playerNum].controlBits;
}


/********************** GET CONTROL STATE ***********************/
//
// INPUT: control = one of kControlBit_XXXX
//

Boolean GetControlState(short player, u_long control)
{
	if (gPlayerInfo[player].controlBits & (1L << control))
		return(true);
	else
		return(false);
}


/********************** GET CONTROL STATE NEW ***********************/
//
// INPUT: control = one of kControlBit_XXXX
//

Boolean GetControlStateNew(short player, u_long control)
{
	if (gPlayerInfo[player].controlBits_New & (1L << control))
		return(true);
	else
		return(false);
}


#pragma mark -

/************************ PUSH KEYS ******************************/

void PushKeys(void)
{
int	i;

	for (i = 0; i < 4; i++)
	{
		gKeyMapP[i] = gKeyMap[i];
		gNewKeysP[i] = gNewKeys[i];
		gOldKeysP[i] = gOldKeys[i];
	}
}

/************************ POP KEYS ******************************/

void PopKeys(void)
{
int	i;

	for (i = 0; i < 4; i++)
	{
		gKeyMap[i] = gKeyMapP[i];
		gNewKeys[i] = gNewKeysP[i];
		gOldKeys[i] = gOldKeysP[i];
	}
}


#pragma mark -


/********************* SET MY KEY EDIT DEFAULTS ****************************/

static void SetMyKeyEditDefaults(void)
{
int		i;

	for (i = 0; i < NUM_CONTROL_NEEDS; i++)
	{
		gUserKeySettings[i] = gGamePrefs.keySettings[i] = gUserKeySettings_Defaults[i];		// copy from defaults
	}



}



/*********************** DO MY KEYBOARD EDIT ****************************/

static void DoMyKeyboardEdit(void)
{
DialogRef 		myDialog;
DialogItemType	itemType,itemHit;
ControlHandle	itemHandle;
Rect			itemRect;
Boolean			dialogDone;
ModalFilterUPP	myProc;
short			i,j;
Str32			s;

	myDialog = GetNewDialog(4000,nil,MOVE_TO_FRONT);


				/* SET CONTROL VALUES */
reset_it:
	for (j = 2; j <= 17; j++)
	{
		i = j-2;
		switch(j)
		{
			case	2:							// turn left p1
					KeyCodeToChar((gUserKeySettings[0] & 0xff00) >> 8, s);
					break;

			case	3:							// turn right p1
					KeyCodeToChar(gUserKeySettings[0] & 0xff, s);
					break;

			case	10:							// turn left p2
					KeyCodeToChar((gUserKeySettings[7] & 0xff00) >> 8, s);
					break;

			case	11:							// turn right p2
					KeyCodeToChar(gUserKeySettings[7] & 0xff, s);
					break;

					/* P1 */
			case	4:
			case	5:
			case	6:
			case	7:
			case	8:
			case	9:
					KeyCodeToChar(gUserKeySettings[j-3], s);
					break;

					/* P2 */
			case	12:
			case	13:
			case	14:
			case	15:
			case	16:
			case	17:
					KeyCodeToChar(gUserKeySettings[j-11+7], s);
					break;

		}

		GetDialogItem(myDialog,j,&itemType,(Handle *)&itemHandle,&itemRect);
		SetDialogItemText((Handle)itemHandle,s);
	}



			/* SET OUTLINE FOR USERITEM */

	GetDialogItem(myDialog,1,&itemType,(Handle *)&itemHandle,&itemRect);
	SetDialogItem(myDialog,37, userItem, (Handle)NewUserItemUPP(DoOutline), &itemRect);


				/* DO IT */

	myProc = NewModalFilterUPP(DialogCallback);

	dialogDone = false;
	while(!dialogDone)
	{
		ModalDialog(myProc, &itemHit);
		switch (itemHit)
		{
			case 	1:									// hit ok
					dialogDone = true;
					break;

			case	38:									// reset defaults
					SetMyKeyEditDefaults();
					goto reset_it;
					break;

		}
	}
	DisposeDialog(myDialog);


		/* UPDATE IN PREFS */

	for (i = 0; i < NUM_CONTROL_NEEDS; i++)			// copy key settings from prefs
		gGamePrefs.keySettings[i] = gUserKeySettings[i];
	SavePrefs();
}


/*********************** DIALOG CALLBACK ***************************/

static pascal Boolean DialogCallback (DialogRef dp,EventRecord *event, short *item)
{
char 			c;
DialogItemType	itemType;
ControlHandle	itemHandle;
Rect			itemRect;
Str15			s;
short			selectedItem = GetDialogKeyboardFocusItem(dp);	// which text edit item is selected?

#pragma unused(item)

	ReadKeyboard_Real();

	switch(event->what)
	{
				/* CHECK SOLID KEYS */

		case	keyDown:								// we have a key press
				c = event->message & 0x00FF;			// what character is it?

				switch(c)
				{
					case	CHAR_SPACE:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Space");
							break;

					case	CHAR_UP:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Up Arrow");
							break;

					case	CHAR_DOWN:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Down Arrow");
							break;

					case	CHAR_LEFT:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Left Arrow");
							break;

					case	CHAR_RIGHT:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Right Arrow");
							break;

					case	CHAR_ESC:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"ESC");
							break;

					case	CHAR_DELETE:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Delete");
							break;

					case	CHAR_RETURN:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Return");
							break;

					case	CHAR_TAB:
							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,"Tab");
							break;

					default:
							if (c >= 'a')							// convert lower to upper case
								c = c - 'a' + 'A';

							s[0] = 1;
							s[1] = c;

							GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
							SetDialogItemText((Handle)itemHandle,s);
							break;
				}

				SetKeyControl(selectedItem, GetFirstRealKeyPressed());

				return(true);							// dont want anyone else to handle this key press


				/* CHECK MODIFIERS */

		default:
				if(GetNewKeyState_Real(KEY_APPLE))
				{
					GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
					SetDialogItemText((Handle)itemHandle,"Apple");
				}
				else
				if(GetNewKeyState_Real(KEY_OPTION))
				{
					GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
					SetDialogItemText((Handle)itemHandle,"Option");
				}
				else
				if(GetNewKeyState_Real(KEY_SHIFT))
				{
					GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
					SetDialogItemText((Handle)itemHandle,"Shift");
				}
				else
				if(GetNewKeyState_Real(KEY_CTRL))
				{
					GetDialogItem(dp, selectedItem,&itemType,(Handle *)&itemHandle,&itemRect);
					SetDialogItemText((Handle)itemHandle,"Control");
				}

	}


	return(false);
}

/********************** GET FIRST REAL KEY PRESSED ************/
//
// returns the char code of the first key found in the keymap
//

static short GetFirstRealKeyPressed(void)
{
short	i;

	for (i = 0; i < 256; i++)
	{
		if(GetNewKeyState_Real(i))
			return(i);
	}
	return(0);
}


/*********************** SET KEY CONTROL ********************************/

static void SetKeyControl(short item, u_short keyCode)
{
	switch(item)
	{
		case	2:							// turn left p1
				gUserKeySettings[0] = (gUserKeySettings[0] & 0x00ff) | (keyCode << 8);
				break;

		case	3:							// turn right p1
				gUserKeySettings[0] = (gUserKeySettings[0] & 0xff00) | keyCode;
				break;

		case	10:							// turn left p2
				gUserKeySettings[7] = (gUserKeySettings[7] & 0x00ff) | (keyCode << 8);
				break;

		case	11:							// turn right p2
				gUserKeySettings[7] = (gUserKeySettings[7] & 0xff00) | keyCode;
				break;

				/* P1 */
		case	4:
		case	5:
		case	6:
		case	7:
		case	8:
		case	9:
				gUserKeySettings[item-3] = keyCode;
				break;

				/* P2 */
		case	12:
		case	13:
		case	14:
		case	15:
		case	16:
		case	17:
				gUserKeySettings[item-11+7] = keyCode;
				break;
	}
}


/****************** KEY CODE TO CHAR ******************/

static void KeyCodeToChar(UInt16 code, Str32 s)
{
long    keyScript, KCHRID;
Handle  KCHRHdl;
UInt32 state;

	switch(code)
	{
				/****************/
				/* SPECIAL KEYS */
				/****************/

		case	KEY_OPTION:
				CopyPString("Option", s);
				break;

		case	KEY_SPACE:
				CopyPString("Space", s);
				break;

		case	KEY_APPLE:
				CopyPString("Apple", s);
				break;

		case	KEY_SHIFT:
				CopyPString("Shift", s);
				break;

		case	KEY_CTRL:
				CopyPString("Control", s);
				break;

		case	KEY_RETURN:
				CopyPString("Return", s);
				break;

		case	KEY_DELETE:
				CopyPString("Delete", s);
				break;

		case	KEY_TAB:
				CopyPString("Tab", s);
				break;

		case	KEY_UP:
				CopyPString("Up Arrow", s);
				break;
		case	KEY_DOWN:
				CopyPString("Down Arrow", s);
				break;
		case	KEY_LEFT:
				CopyPString("Left Arrow", s);
				break;
		case	KEY_RIGHT:
				CopyPString("Right Arrow", s);
				break;


				/**************/
				/* OTHER KEYS */
				/**************/

		default:

			 			/* GET KCHR RESOURCE */

				UseResFile(0);			// use system rez

				keyScript = GetScriptManagerVariable(smKeyScript);	//  First get the current keyboard script.
				KCHRID = GetScriptVariable(keyScript, smScriptKeys);	//  Now get the KCHR resource ID for that script.
				KCHRHdl = GetResource('KCHR',KCHRID);					// Finally, get your own copy of this KCHR. Now you can pass a proper KCHR pointer to KeyTrans. */
				HLock(KCHRHdl);
				UseResFile(gMainAppRezFile);

				if (KCHRHdl == nil)
				{
					SysBeep(0);
					DoFatalAlert("KeyCodeToChar: GetResource(KCHR) failed");
				}


			  		/* TRANSLATE CODE */

			  state = 0;
			  state = KeyTranslate (*KCHRHdl, code, &state);


			  			/* CLEANUP */

			  ReleaseResource(KCHRHdl);

			  s[0] = 1;								// set string to single char
			  s[1] = state & 0xff;
	}
}















