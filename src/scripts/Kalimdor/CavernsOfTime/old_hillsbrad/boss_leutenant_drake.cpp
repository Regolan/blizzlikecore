/*
 * Copyright (C) 2011-2013 BlizzLikeGroup <http://blizzlike.servegame.com/>
 * Please, look at the CREDITS.md file.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: Boss_Luetenant_Drake
Complete(%): 70
Comment: Missing proper code for patrolling area after being spawned. Script for GO (barrels) quest 10283
Category: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

#include "ScriptPCH.h"
#include "old_hillsbrad.h"
#include "ScriptedEscortAI.h"

/*######
## go_barrel_old_hillsbrad
######*/

bool GOHello_go_barrel_old_hillsbrad(Player* , GameObject* _GO)
{
    ScriptedInstance* pInstance = _GO->GetInstanceData();

    if (!pInstance)
        return false;

    if (pInstance->GetData(TYPE_BARREL_DIVERSION) == DONE)
        return false;

    pInstance->SetData(TYPE_BARREL_DIVERSION, IN_PROGRESS);

    return false;
}

/*######
## boss_lieutenant_drake
######*/

#define SAY_ENTER               -1560006
#define SAY_AGGRO               -1560007
#define SAY_SLAY1               -1560008
#define SAY_SLAY2               -1560009
#define SAY_MORTAL              -1560010
#define SAY_SHOUT               -1560011
#define SAY_DEATH               -1560012

#define SPELL_WHIRLWIND         31909
#define SPELL_HAMSTRING         9080
#define SPELL_MORTAL_STRIKE     31911
#define SPELL_FRIGHTENING_SHOUT 33789

struct Location
{
    uint32 wpId;
    float x;
    float y;
    float z;
};

static Location DrakeWP[]=
{
    {0, 2125.84f, 88.2535f, 54.8830f},
    {1, 2111.01f, 93.8022f, 52.6356f},
    {2, 2106.70f, 114.753f, 53.1965f},
    {3, 2107.76f, 138.746f, 52.5109f},
    {4, 2114.83f, 160.142f, 52.4738f},
    {5, 2125.24f, 178.909f, 52.7283f},
    {6, 2151.02f, 208.901f, 53.1551f},
    {7, 2177.00f, 233.069f, 52.4409f},
    {8, 2190.71f, 227.831f, 53.2742f},
    {9, 2178.14f, 214.219f, 53.0779f},
    {10, 2154.99f, 202.795f, 52.6446f},
    {11, 2132.00f, 191.834f, 52.5709f},
    {12, 2117.59f, 166.708f, 52.7686f},
    {13, 2093.61f, 139.441f, 52.7616f},
    {14, 2086.29f, 104.950f, 52.9246f},
    {15, 2094.23f, 81.2788f, 52.6946f},
    {16, 2108.70f, 85.3075f, 53.3294f},
    {17, 2125.50f, 88.9481f, 54.7953f},
    {18, 2128.20f, 70.9763f, 64.4221f}
};

struct boss_lieutenant_drakeAI : public ScriptedAI
{
    boss_lieutenant_drakeAI(Creature* c) : ScriptedAI(c) {}

    bool CanPatrol;
    uint32 wpId;

    uint32 Whirlwind_Timer;
    uint32 Fear_Timer;
    uint32 MortalStrike_Timer;
    uint32 ExplodingShout_Timer;

    void Reset()
    {
        CanPatrol = true;
        wpId = 0;

        Whirlwind_Timer = 20000;
        Fear_Timer = 30000;
        MortalStrike_Timer = 45000;
        ExplodingShout_Timer = 25000;
    }

    void EnterCombat(Unit*)
    {
        DoScriptText(SAY_AGGRO, me);
    }

    void KilledUnit(Unit*)
    {
        switch(rand()%2)
        {
            case 0: DoScriptText(SAY_SLAY1, me); break;
            case 1: DoScriptText(SAY_SLAY2, me); break;
        }
    }

    void JustDied(Unit*)
    {
        DoScriptText(SAY_DEATH, me);
    }

    void UpdateAI(const uint32 diff)
    {
        //TODO: make this work
        if (CanPatrol && wpId == 0)
        {
            me->GetMotionMaster()->MovePoint(DrakeWP[0].wpId, DrakeWP[0].x, DrakeWP[0].y, DrakeWP[0].z);
            wpId++;
        }

        //Return since we have no target
        if (!UpdateVictim())
            return;

        //Whirlwind
        if (Whirlwind_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_WHIRLWIND);
            Whirlwind_Timer = 20000+rand()%5000;
        } else Whirlwind_Timer -= diff;

        //Fear
        if (Fear_Timer <= diff)
        {
            DoScriptText(SAY_SHOUT, me);
            DoCast(me->getVictim(), SPELL_FRIGHTENING_SHOUT);
            Fear_Timer = 30000+rand()%10000;
        } else Fear_Timer -= diff;

        //Mortal Strike
        if (MortalStrike_Timer <= diff)
        {
            DoScriptText(SAY_MORTAL, me);
            DoCast(me->getVictim(), SPELL_MORTAL_STRIKE);
            MortalStrike_Timer = 45000+rand()%5000;
        } else MortalStrike_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_lieutenant_drake(Creature* pCreature)
{
    return new boss_lieutenant_drakeAI (pCreature);
}

void AddSC_boss_lieutenant_drake()
{
    Script *newscript;

    newscript = new Script;
    newscript->Name = "go_barrel_old_hillsbrad";
    newscript->pGOHello = &GOHello_go_barrel_old_hillsbrad;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "boss_lieutenant_drake";
    newscript->GetAI = &GetAI_boss_lieutenant_drake;
    newscript->RegisterSelf();
}

