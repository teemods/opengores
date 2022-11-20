/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_AURA_H
#define GAME_SERVER_ENTITIES_AURA_H

#include <game/server/entity.h>

class CAura : public CEntity
{
public:
	enum
	{
		NUM_SIDE = 64,
		NUM_IDS = NUM_SIDE,
	};

public:
	CAura(CGameWorld *pGameWorld, int Owner, int Num, int Type, bool Changing);
	~CAura();

	virtual void Snap(int SnappingClient) override;
	virtual void Tick() override;
	virtual void Reset() override;

private:
	int m_IDs[NUM_IDS];

public:
	int m_Owner;

	int m_LoadingTick;

	int m_Num;
	int m_Type;
	bool m_boolreback;
	bool m_Changing;
};

#endif