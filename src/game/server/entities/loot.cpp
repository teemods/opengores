/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include "character.h"
#include "loot.h"

CLoot::CLoot(CGameWorld *pGameWorld, int Owner, const char *OwnerName, int ResponsibleTeam, vec2 Pos, int LootType, bool DotsEffect, bool Guided) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Owner = Owner;
	str_copy(m_OwnerName, OwnerName, sizeof(m_OwnerName));

	m_ResponsibleTeam = ResponsibleTeam;
	m_LootType = LootType;
	m_DotsEffect = DotsEffect;
	m_Guided = Guided;
	m_GuidedTarget = NULL;

	m_Pos = Pos;

	GameWorld()->InsertEntity(this);
	for(int i = 0; i < NUM_IDS; i++)
	{
		m_IDs[i] = Server()->SnapNewID();
	}
}

CLoot::~CLoot()
{
	for(int i = 0; i < NUM_IDS; i++)
	{
		Server()->SnapFreeID(m_IDs[i]);
	}
}

void CLoot::Reset()
{
	m_MarkedForDestroy = true;
}

void CLoot::Tick()
{
	m_TicksActive++;

	if((m_TicksActive / Server()->TickSpeed()) > g_Config.m_SvEffectLootDuration)
	{
		Reset();
		return;
	}

	// Move guided loot
	if(m_Guided)
	{
		if(!m_GuidedTarget)
		{
			FindGuided();
		}
		else
		{
			MoveGuided();
		}
	}

	// Find other players
	for(CCharacter *p = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter *)p->TypeNext())
	{
		if(p->GetPlayer()->GetCID() != m_Owner && distance(p->m_Pos, m_Pos) < 40)
		{
			if(p->GetCore().m_Solo || p->Team() != m_ResponsibleTeam)
			{
				continue;
			}

			int64_t TeamMask;
			TeamMask = p->Teams()->TeamMask(p->Team(), -1, m_Owner);

			// create pick up sound
			if(m_LootType == POWERUP_HEALTH)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, TeamMask);
				GameServer()->CreateSound(m_Pos, SOUND_PLAYER_SPAWN, TeamMask);
			}
			if(m_LootType == POWERUP_ARMOR)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, TeamMask);
				GameServer()->CreateSound(m_Pos, SOUND_PLAYER_SPAWN, TeamMask);
			}
			if(m_LootType == POWERUP_NINJA)
			{
				GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, TeamMask);
			}

			// send loot emoji
			if(m_LootType == POWERUP_HEALTH)
			{
				GameServer()->SendEmoticon(p->GetPlayer()->GetCID(), EMOTICON_HEARTS);
				p->GetPlayer()->GetCharacter()->SetEmote(EMOTE_HAPPY, Server()->Tick() + 2 * Server()->TickSpeed());
			}
			if(m_LootType == POWERUP_ARMOR)
			{
				GameServer()->SendEmoticon(p->GetPlayer()->GetCID(), EMOTICON_EYES);
				p->GetPlayer()->GetCharacter()->SetEmote(EMOTE_HAPPY, Server()->Tick() + 2 * Server()->TickSpeed());
			}
			if(m_LootType == POWERUP_NINJA)
			{
				GameServer()->SendEmoticon(p->GetPlayer()->GetCID(), EMOTICON_GHOST);
				p->GetPlayer()->GetCharacter()->SetEmote(EMOTE_SURPRISE, Server()->Tick() + 2 * Server()->TickSpeed());
			}

			// get loot name
			char lootName[128] = "Random Loot";
			if(m_LootType == POWERUP_HEALTH)
			{
				str_copy(lootName, "Heart", sizeof(lootName));
			}
			if(m_LootType == POWERUP_ARMOR)
			{
				str_copy(lootName, "Shield", sizeof(lootName));
			}
			if(m_LootType == POWERUP_NINJA)
			{
				str_copy(lootName, "Ninja Sword", sizeof(lootName));
			}

			// send message chat
			char message[128];
			str_format(message, sizeof(message), "You just got a %s that %s dropped!", lootName, m_OwnerName);

			GameServer()->SendChatTarget(p->GetPlayer()->GetCID(), message);

			// delete drop loot
			Reset();
			break;
		}
	}
}

void CLoot::FindGuided()
{
	// Find target if no one
	if(!m_GuidedTarget)
	{
		CCharacter *pClosest = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
		while(pClosest)
		{
			if(pClosest->GetPlayer()->GetCID() != m_Owner && distance(pClosest->m_Pos, m_Pos) <= g_Config.m_SvEffectGuidedLootDetection)
			{
				// check if player is not on solo in the same team
				if(pClosest->GetCore().m_Solo || pClosest->Team() != m_ResponsibleTeam)
				{
					pClosest = (CCharacter *)pClosest->TypeNext();
					continue;
				}

				// found collision
				if(GameServer()->Collision()->IntersectLine(m_Pos, pClosest->m_Pos, 0, 0))
				{
					pClosest = (CCharacter *)pClosest->TypeNext();
					continue;
				}

				m_GuidedTarget = pClosest;
			}

			pClosest = (CCharacter *)pClosest->TypeNext();
		}
	}
}

void CLoot::MoveGuided()
{
	if(!m_GuidedTarget->IsAlive())
	{
		return;
	}

	if(m_GuidedTarget->GetCore().m_Solo)
	{
		return;
	}

	// Calculate new position
	vec2 newPos = m_Pos + (m_GuidedLefpos * g_Config.m_SvEffectGuidedLootSpeed);

	// Do not enter on walls if find it
	// if(GameServer()->Collision()->CheckPoint(newPos)) {
	//     vec2 PossiblePos;
	//     bool Found = GetNearestAirPos(newPos, m_GuidedTarget->m_Pos, &PossiblePos);
	//     if(Found) {
	//     	m_Pos = PossiblePos;
	//     }
	// } else {
	m_Pos = newPos;
	// }

	m_GuidedLefpos = normalize(m_GuidedTarget->m_Pos - m_Pos);
}

void CLoot::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = 0;
	int64_t TeamMask = -1LL;

	if(SnappingClient >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(SnappingClient);

	if(pOwnerChar && pOwnerChar->IsAlive())
		TeamMask = pOwnerChar->Teams()->TeamMask(m_ResponsibleTeam);

	if(!CmaskIsSet(TeamMask, SnappingClient))
		return;

	if(m_DotsEffect)
	{
		float AngleStart = (2.0f * pi * Server()->Tick() / static_cast<float>(Server()->TickSpeed())) / 10.0f;
		float AngleStep = (2.0f * pi / NUM_SIDE);
		float R = 30.0f;

		AngleStart = AngleStart * 4.0f;
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

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pP)
	{
		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = m_LootType;
	}
}