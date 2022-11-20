/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include "character.h"
#include "trail.h"

CTrail::CTrail(CGameWorld *pGameWorld, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	CPlayer *pOwner = GameServer()->m_apPlayers[Owner];
	if(!pOwner || !pOwner->GetCharacter())
	{
		return;
	}

	m_Owner = Owner;
	m_Pos = pOwner->GetCharacter()->m_Pos;

	GameWorld()->InsertEntity(this);
	m_IDs[0] = Server()->SnapNewID();

	pOwner->m_PowersData.m_HasTrailSpawned = true;
}

CTrail::~CTrail()
{
	Server()->SnapFreeID(m_IDs[0]);
}

void CTrail::Reset()
{
	// verify if player exists
	// without this verification, if player disconnect, the server will crash
	if(GameServer()->m_apPlayers[m_Owner])
	{
		GameServer()->m_apPlayers[m_Owner]->m_PowersData.m_HasTrailSpawned = false;
	}

	m_MarkedForDestroy = true;
}

void CTrail::Tick()
{
	CPlayer *pOwner = GameServer()->m_apPlayers[m_Owner];
	if(!pOwner || !pOwner->GetCharacter())
	{
		Reset();
		return;
	}

	bool HasTheTrail = pOwner->m_Powers.m_HasTrail && pOwner->m_Powers.m_HasTrailEnabled;

	// check if player has power and power enabled
	if(!HasTheTrail)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetCharacter()->m_Pos;
}

void CTrail::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = 0;
	int64_t TeamMask = -1LL;

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	// need to verify also here if char exists (or the server will crash)
	if(pOwnerChar && pOwnerChar->IsPaused())
		return;

	if(pOwnerChar && pOwnerChar->IsAlive())
		TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);

	if(!CmaskIsSet(TeamMask, SnappingClient))
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[0], sizeof(CNetObj_Projectile)));
	if(!pObj)
		return;
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_VelX = 0;
	pObj->m_VelY = 0;
	pObj->m_StartTick = Server()->Tick() - 1;
	pObj->m_Type = WEAPON_HAMMER;
}