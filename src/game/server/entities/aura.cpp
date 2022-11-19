/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include "aura.h"
#include "character.h"

CAura::CAura(CGameWorld *pGameWorld, int Owner, int Num, int Type, bool Changing)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
    CPlayer *pOwner = GameServer()->m_apPlayers[Owner];
    if(!pOwner || !pOwner->GetCharacter())
	{
		return;
	}

	m_Owner = Owner;

	m_Pos = pOwner->GetCharacter()->m_Pos;
	m_LoadingTick = Server()->TickSpeed();

	m_Num = Num;
	m_Type = Type;
	m_Changing = Changing;

	GameWorld()->InsertEntity(this);
	for(int i=0; i<m_Num; i++)
		m_IDs[i] = Server()->SnapNewID();

    if(m_Type == WEAPON_HAMMER) {
        pOwner->m_PowersData.m_HasAuraDotSpawned = true;
    }
    if(m_Type == WEAPON_GUN) {
        pOwner->m_PowersData.m_HasAuraGunSpawned = true;
    }
    if(m_Type == WEAPON_SHOTGUN) {
        pOwner->m_PowersData.m_HasAuraShotgunSpawned = true;
    }
}

CAura::~CAura()
{
	for(int i=0; i<m_Num; i++)
		Server()->SnapFreeID(m_IDs[i]);
}

void CAura::Reset()
{
    // verify if player exists
    // without this verification, if player disconnect, the server will crash
    if(GameServer()->m_apPlayers[m_Owner])
    {
        if(m_Type == WEAPON_HAMMER) {
            GameServer()->m_apPlayers[m_Owner]->m_PowersData.m_HasAuraDotSpawned = false;
        }
        if(m_Type == WEAPON_GUN) {
            GameServer()->m_apPlayers[m_Owner]->m_PowersData.m_HasAuraGunSpawned = false;
        }
        if(m_Type == WEAPON_SHOTGUN) {
            GameServer()->m_apPlayers[m_Owner]->m_PowersData.m_HasAuraShotgunSpawned = false;
        }
    }

	m_MarkedForDestroy = true;
}

void CAura::Tick()
{
	CPlayer *pOwner = GameServer()->m_apPlayers[m_Owner];
	if(!pOwner || !pOwner->GetCharacter())
	{
		Reset();
		return;
	}

    bool IsFreezed = pOwner->GetCharacter()->m_FreezeTime > Server()->Tick() - (g_Config.m_SvFreezeDelay * Server()->TickSpeed());
    bool HasTheAura = false;

    if(m_Type == WEAPON_HAMMER) {
        HasTheAura = pOwner->m_Powers.m_HasAuraDot && pOwner->m_Powers.m_HasAuraDotEnabled;
    }
    if(m_Type == WEAPON_GUN) {
        HasTheAura = pOwner->m_Powers.m_HasAuraGun && pOwner->m_Powers.m_HasAuraGunEnabled;
    }
    if(m_Type == WEAPON_SHOTGUN) {
        HasTheAura = pOwner->m_Powers.m_HasAuraShotgun && pOwner->m_Powers.m_HasAuraShotgunEnabled;
    }

    // check if player has power and power enabled and is freezed
    if(! HasTheAura || ! IsFreezed)
    {
        Reset();
		return;
    }

	if(m_Changing && !m_boolreback)
	{
		m_LoadingTick--;
		if(m_LoadingTick <= 1)
			m_boolreback = true;
	}
	if(m_Changing && m_boolreback)
	{
		m_LoadingTick++;
		if(m_LoadingTick >= 20)
			m_boolreback = false;
	}
	m_Pos = pOwner->GetCharacter()->m_Pos;
}


void CAura::Snap(int SnappingClient)
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

	// Check if char is moving
	if(! pOwnerChar->IsGrounded()) {
		return;
	}

	float AngleStart = (2.0f * pi * (Server()->Tick() + (m_Type + 1) * 4 * Server()->TickSpeed())/static_cast<float>(Server()->TickSpeed()))/10.0f;
	float AngleStep = 2.0f * pi / CAura::m_Num;
	float R = 60.0f+m_LoadingTick;
	AngleStart = AngleStart*2.0f;
	for(int i=0; i<CAura::m_Num; i++)
	{
		vec2 PosStart = m_Pos + vec2(R * cos(AngleStart + AngleStep*i), R * sin(AngleStart + AngleStep*i));
		
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_X = (int)PosStart.x;
		pObj->m_Y = (int)PosStart.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick()-1;
		pObj->m_Type = m_Type;
	}
}