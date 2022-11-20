/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LOOT_H
#define GAME_SERVER_ENTITIES_LOOT_H

#include <game/server/entity.h>

class CLoot : public CEntity
{
public:
	enum
	{
		NUM_SIDE = 1,
		NUM_IDS,
	};

public:
	CLoot(CGameWorld *pGameWorld, int Owner, const char *OwnerName, int ResponsibleTeam, vec2 Pos, int LootType, bool DotsEffect, bool Guided);
	virtual ~CLoot();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

	virtual void FindGuided();
	virtual void MoveGuided();

private:
	int m_Owner;
	char m_OwnerName[16];

	int m_ResponsibleTeam;
	int m_LootType;
	int m_DotsEffect;
	int m_Guided;
	CCharacter *m_GuidedTarget;
	vec2 m_GuidedLefpos;

	int m_IDs[NUM_IDS];
	int m_TicksActive;
};

#endif