/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_SOUNDTRACK_H
#define GAME_SERVER_ENTITIES_SOUNDTRACK_H

#include <game/server/entity.h>

class CSoundtrack : public CEntity
{
public:
	enum
	{
		NUM_SIDE = 3,
		NUM_IDS,
	};

public:
	CSoundtrack(CGameWorld *pGameWorld, int Owner);
	virtual ~CSoundtrack();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

private:
	int m_Owner;

	int m_IDs[NUM_IDS];
	int m_TicksActive;
};

#endif