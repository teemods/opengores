/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include "character.h"
#include "soundtrack.h"

CSoundtrack::CSoundtrack(CGameWorld *pGameWorld, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	CPlayer *pOwner = GameServer()->m_apPlayers[Owner];
	if(!pOwner || !pOwner->GetCharacter())
	{
		return;
	}

	m_Owner = Owner;
	m_Pos = pOwner->GetCharacter()->m_Pos + vec2(0, -25);

	GameWorld()->InsertEntity(this);

	int64_t TeamMask = pOwner->GetCharacter()->Teams()->TeamMask(pOwner->GetCharacter()->Team(), -1, Owner);
	GameServer()->CreateSound(m_Pos, SOUND_MENU, TeamMask);

	for(int i = 0; i < NUM_IDS; i++)
		m_IDs[i] = Server()->SnapNewID();
}

CSoundtrack::~CSoundtrack()
{
	for(int CurrentID : m_IDs)
		Server()->SnapFreeID(CurrentID);
}

void CSoundtrack::Reset()
{
	m_MarkedForDestroy = true;
}

void CSoundtrack::Tick()
{
	m_TicksActive++;

	if((m_TicksActive / Server()->TickSpeed()) > 25)
	{
		Reset();
		return;
	}
}

void CSoundtrack::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = 0;
	int64_t TeamMask = -1LL;

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	if(pOwnerChar && pOwnerChar->IsAlive())
		TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);

	if(!CmaskIsSet(TeamMask, SnappingClient))
		return;

	float AngleStart = (2.0f * pi * Server()->Tick() / static_cast<float>(Server()->TickSpeed())) / 10.0f;
	float AngleStep = (2.0f * pi / NUM_SIDE);
	float R = 30.0f;

	AngleStart = AngleStart * 3.0f;
	for(int i = 0; i < NUM_SIDE; ++i)
	{
		vec2 PosStart = m_Pos + vec2(R * cos((AngleStart + AngleStep * i)), R * sin((AngleStart + AngleStep * i)));
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(pObj)
		{
			pObj->m_X = (int)PosStart.x;
			pObj->m_Y = (int)PosStart.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick();
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
}