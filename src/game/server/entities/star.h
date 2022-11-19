/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Copyright � 2013 Neox.                                                                                                */
/* If you are missing that file, acquire a complete release at https://www.teeworlds.com/forum/viewtopic.php?pid=106934  */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef GAME_SERVER_ENTITIES_STAR_H
#define GAME_SERVER_ENTITIES_STAR_H

#include <game/server/entity.h>

class CStar : public CEntity
{
public:
	CStar(CGameWorld *pGameWorld, int Owner);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

private:
	int m_Owner;
};

#endif