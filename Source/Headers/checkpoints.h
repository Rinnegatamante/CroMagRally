//
// checkpoints.h
//

#ifndef __CKPT__
#define __CKPT__

#define	MAX_CHECKPOINTS	100


typedef struct
{
	uint16_t	unused;
	uint16_t	infoBits;

	float	x[2],z[2];			// the two endpoints
}CheckpointDefType;

//================================================================


void UpdatePlayerCheckpoints(short p);
void PlayerCompletedRace(short playerNum);
void CalcPlayerPlaces(void);



#endif