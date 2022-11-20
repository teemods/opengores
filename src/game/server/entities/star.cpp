/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Copyright � 2013 Neox.                                                                                                */
/* If you are missing that file, acquire a complete release at https://www.teeworlds.com/forum/viewtopic.php?pid=106934  */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include "character.h"
#include "star.h"

CStar::CStar(CGameWorld *pGameWorld, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	CPlayer *pOwner = GameServer()->m_apPlayers[Owner];
	if(!pOwner || !pOwner->GetCharacter())
	{
		return;
	}

	m_Owner = Owner;

	m_Pos = pOwner->GetCharacter()->m_Pos;
	pOwner->m_PowersData.m_HasStarSpawned = true;

	GameWorld()->InsertEntity(this);
}

void CStar::Reset()
{
	// verify if player exists
	// without this verification, if player disconnect, the server will crash
	if(GameServer()->m_apPlayers[m_Owner])
	{
		GameServer()->m_apPlayers[m_Owner]->m_PowersData.m_HasStarSpawned = false;
	}

	m_MarkedForDestroy = true;
}

void CStar::Tick()
{
	if(!GameServer()->GetPlayerChar(m_Owner))
	{
		Reset();
		return;
	}

	if(!GameServer()->m_apPlayers[m_Owner]->m_Powers.m_HasStar || !GameServer()->m_apPlayers[m_Owner]->m_Powers.m_HasStarEnabled)
	{
		Reset();
		return;
	}

	bool CheckCollisionLeft = GameServer()->Collision()->CheckPoint(GameServer()->m_apPlayers[m_Owner]->GetCharacter()->m_Pos + vec2(50, 0));
	bool CheckCollisionRight = GameServer()->Collision()->CheckPoint(GameServer()->m_apPlayers[m_Owner]->GetCharacter()->m_Pos + vec2(-50, 0));

	if(CheckCollisionLeft || (!CheckCollisionRight && GameServer()->m_apPlayers[m_Owner]->GetCharacter()->GetLastSightInput().x > 0))
		m_Pos = GameServer()->m_apPlayers[m_Owner]->GetCharacter()->m_Pos + vec2(-50, -25);
	else
		m_Pos = GameServer()->m_apPlayers[m_Owner]->GetCharacter()->m_Pos + vec2(50, -25);
}

void CStar::Snap(int SnappingClient)
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

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_Pos.x;
	pObj->m_FromY = (int)m_Pos.y;
	pObj->m_StartTick = Server()->Tick();
}