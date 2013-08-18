/*
 * This file is part of the BlizzLikeCore Project.
 * See CREDITS and LICENSE files for Copyright information.
 */

/* ScriptData
Name: boss_felblood_kaelthas
Complete(%): 90
Comment: Normal and Heroic Support. Issues: Arcane Spheres do not initially follow targets.
Category: Magisters' Terrace
EndScriptData */

#include "ScriptPCH.h"
#include "magisters_terrace.h"
#include "WorldPacket.h"

#define SAY_AGGRO                   -1585023                //This yell should be done when the room is cleared. For now, set it as a movelineofsight yell.
#define SAY_PHOENIX                 -1585024
#define SAY_FLAMESTRIKE             -1585025
#define SAY_GRAVITY_LAPSE           -1585026
#define SAY_TIRED                   -1585027
#define SAY_RECAST_GRAVITY          -1585028
#define SAY_DEATH                   -1585029

/*** Spells ***/

// Phase 1 spells
#define SPELL_FIREBALL_NORMAL         44189                 // Deals 2700-3300 damage at current target
#define SPELL_FIREBALL_HEROIC         46164                 //       4950-6050

#define SPELL_PHOENIX                 44194                 // Summons a phoenix (Doesn't work?)
#define SPELL_PHOENIX_BURN            44197                 // A spell Phoenix uses to damage everything around
#define SPELL_REBIRTH_DMG             44196                 // DMG if a Phoenix rebirth happen

#define SPELL_FLAMESTRIKE1_NORMAL     44190                 // Damage part
#define SPELL_FLAMESTRIKE1_HEROIC     46163                 // Heroic damage part
#define SPELL_FLAMESTRIKE2            44191                 // Flamestrike indicator before the damage
#define SPELL_FLAMESTRIKE3            44192                 // Summons the trigger + animation (projectile)

#define SPELL_SHOCK_BARRIER           46165                 // Heroic only; 10k damage shield, followed by Pyroblast
#define SPELL_PYROBLAST               36819                 // Heroic only; 45-55k fire damage

// Phase 2 spells
#define SPELL_GRAVITY_LAPSE_INITIAL   44224                 // Cast at the beginning of every Gravity Lapse
#define SPELL_GRAVITY_LAPSE_CHANNEL   44251                 // Channeled; blue beam animation to every enemy in range
#define SPELL_TELEPORT_CENTER         44218                 // Should teleport people to the center. Requires DB entry in spell_target_position.
#define SPELL_GRAVITY_LAPSE_FLY       44227                 // Hastens flyspeed and allows flying for 1 minute. For some reason removes 44226.
#define SPELL_GRAVITY_LAPSE_DOT       44226                 // Knocks up in the air and applies a 300 DPS DoT.
#define SPELL_ARCANE_SPHERE_PASSIVE   44263                 // Passive auras on Arcane Spheres
#define SPELL_POWER_FEEDBACK          44233                 // Stuns him, making him take 50% more damage for 10 seconds. Cast after Gravity Lapse

/*** Creatures ***/
#define CREATURE_PHOENIX              24674
#define CREATURE_PHOENIX_EGG          24675
#define CREATURE_ARCANE_SPHERE        24708

// Miscalenous
#define SPELL_ESCAPE_TO_IOQD          46841

/** Locations **/
float KaelLocations[3][2]=
{
    {148.744659f, 181.377426f},
    {140.823883f, 195.403046f},
    {156.574188f, 195.650482f},
};

#define LOCATION_Z                  -16.727455f

struct boss_felblood_kaelthasAI : public ScriptedAI
{
    boss_felblood_kaelthasAI(Creature* c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
        Heroic = c->GetMap()->IsHeroic();
    }

    ScriptedInstance* pInstance;

    uint32 FireballTimer;
    uint32 PhoenixTimer;
    uint32 FlameStrikeTimer;
    uint32 CombatPulseTimer;

    //Heroic only
    uint32 PyroblastTimer;

    uint32 GravityLapseTimer;
    uint32 GravityLapsePhase;
    // 0 = No Gravity Lapse
    // 1 = Casting Gravity Lapse visual
    // 2 = Teleported people to self
    // 3 = Knocked people up in the air
    // 4 = Applied an aura that allows them to fly, channeling visual, relased Arcane Orbs.

    bool FirstGravityLapse;
    bool Heroic;
    bool HasTaunted;

    uint8 Phase;
    // 0 = Not started
    // 1 = Fireball; Summon Phoenix; Flamestrike
    // 2 = Gravity Lapses

    void Reset()
    {
        // TODO: Timers
        FireballTimer = 0;
        PhoenixTimer = 10000;
        FlameStrikeTimer = 25000;
        CombatPulseTimer = 0;

        PyroblastTimer = 60000;

        GravityLapseTimer = 0;
        GravityLapsePhase = 0;

        FirstGravityLapse = true;
        HasTaunted = false;

        Phase = 0;

        if (pInstance)
        {
            pInstance->SetData(DATA_KAELTHAS_EVENT, NOT_STARTED);
            pInstance->HandleGameObject(pInstance->GetData64(DATA_KAEL_DOOR), true);
           // Open the big encounter door. Close it in Aggro and open it only in JustDied(and here)
           // Small door opened after event are expected to be closed by default
        }
    }

    void JustDied(Unit* /*killer*/)
    {
        DoScriptText(SAY_DEATH, me);

        if (!pInstance)
            return;

        pInstance->HandleGameObject(pInstance->GetData64(DATA_KAEL_DOOR), true);
        // Open the encounter door
    }

    void DamageTaken(Unit* /*done_by*/, uint32 &damage)
    {
        if (damage > me->GetHealth())
            RemoveGravityLapse();                           // Remove Gravity Lapse so that players fall to ground if they kill him when in air.
    }

    void EnterCombat(Unit* /*who*/)
    {
        if (!pInstance)
            return;

        pInstance->HandleGameObject(pInstance->GetData64(DATA_KAEL_DOOR), false);
       //Close the encounter door, open it in JustDied/Reset
    }

    void MoveInLineOfSight(Unit* who)
    {
        if (!HasTaunted && me->IsWithinDistInMap(who, 40.0f))
        {
            DoScriptText(SAY_AGGRO, me);
            HasTaunted = true;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void SetThreatList(Creature* SummonedUnit)
    {
        if (!SummonedUnit)
            return;

        std::list<HostileReference*>& m_threatlist = me->getThreatManager().getThreatList();
        std::list<HostileReference*>::const_iterator i = m_threatlist.begin();
        for (i = m_threatlist.begin(); i != m_threatlist.end(); ++i)
        {
            Unit* pUnit = Unit::GetUnit((*me), (*i)->getUnitGuid());
            if (pUnit && pUnit->isAlive())
            {
                float threat = me->getThreatManager().getThreat(pUnit);
                SummonedUnit->AddThreat(pUnit, threat);
            }
        }
    }

    void TeleportPlayersToSelf()
    {
        float x = KaelLocations[0][0];
        float y = KaelLocations[0][1];
        me->GetMap()->CreatureRelocation(me, x, y, LOCATION_Z, 0.0f);
        std::list<HostileReference*>::const_iterator i = me->getThreatManager().getThreatList().begin();
        for (i = me->getThreatManager().getThreatList().begin(); i!= me->getThreatManager().getThreatList().end(); ++i)
        {
            Unit* pUnit = Unit::GetUnit((*me), (*i)->getUnitGuid());
            if (pUnit && (pUnit->GetTypeId() == TYPEID_PLAYER))
                pUnit->CastSpell(pUnit, SPELL_TELEPORT_CENTER, true);
        }
        DoCast(me, SPELL_TELEPORT_CENTER, true);
    }

    void CastGravityLapseKnockUp()
    {
        std::list<HostileReference*>::const_iterator i = me->getThreatManager().getThreatList().begin();
        for (i = me->getThreatManager().getThreatList().begin(); i!= me->getThreatManager().getThreatList().end(); ++i)
        {
            Unit* pUnit = Unit::GetUnit((*me), (*i)->getUnitGuid());
            if (pUnit && (pUnit->GetTypeId() == TYPEID_PLAYER))
                // Knockback into the air
                pUnit->CastSpell(pUnit, SPELL_GRAVITY_LAPSE_DOT, true, 0, 0, me->GetGUID());
        }
    }

    void CastGravityLapseFly()                              // Use Fly Packet hack for now as players can't cast "fly" spells unless in map 530. Has to be done a while after they get knocked into the air...
    {
        std::list<HostileReference*>::const_iterator i = me->getThreatManager().getThreatList().begin();
        for (i = me->getThreatManager().getThreatList().begin(); i!= me->getThreatManager().getThreatList().end(); ++i)
        {
            Unit* pUnit = Unit::GetUnit((*me), (*i)->getUnitGuid());
            if (pUnit && (pUnit->GetTypeId() == TYPEID_PLAYER))
            {
                // Also needs an exception in spell system.
                pUnit->CastSpell(pUnit, SPELL_GRAVITY_LAPSE_FLY, true, 0, 0, me->GetGUID());

                // Use packet hack
                WorldPacket data(12);
                data.SetOpcode(SMSG_MOVE_SET_CAN_FLY);
                data << pUnit->GetPackGUID();
                data << uint32(0);
                pUnit->SendMessageToSet(&data, true);
            }
        }
    }

    void RemoveGravityLapse()
    {
        std::list<HostileReference*>::const_iterator i = me->getThreatManager().getThreatList().begin();
        for (i = me->getThreatManager().getThreatList().begin(); i!= me->getThreatManager().getThreatList().end(); ++i)
        {
            Unit* pUnit = Unit::GetUnit((*me), (*i)->getUnitGuid());
            if (pUnit && (pUnit->GetTypeId() == TYPEID_PLAYER))
            {
                pUnit->RemoveAurasDueToSpell(SPELL_GRAVITY_LAPSE_FLY);
                pUnit->RemoveAurasDueToSpell(SPELL_GRAVITY_LAPSE_DOT);

                WorldPacket data(12);
                data.SetOpcode(SMSG_MOVE_UNSET_CAN_FLY);
                data << pUnit->GetPackGUID();
                data << uint32(0);
                pUnit->SendMessageToSet(&data, true);
            }
        }
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        switch (Phase)
        {
            case 0:
            {
                // *Heroic mode only:
                if (Heroic)
                {
                    if (PyroblastTimer <= diff)
                    {
                        me->InterruptSpell(CURRENT_CHANNELED_SPELL);
                        me->InterruptSpell(CURRENT_GENERIC_SPELL);
                        DoCast(me, SPELL_SHOCK_BARRIER, true);
                        DoCast(me->getVictim(), SPELL_PYROBLAST);
                        PyroblastTimer = 60000;
                    } else PyroblastTimer -= diff;
                }

                if (FireballTimer <= diff)
                {
                    DoCast(me->getVictim(), Heroic ? SPELL_FIREBALL_HEROIC : SPELL_FIREBALL_NORMAL);
                    FireballTimer = urand(2000,6000);
                } else FireballTimer -= diff;

                if (PhoenixTimer <= diff)
                {

                    Unit* pTarget = SelectUnit(SELECT_TARGET_RANDOM,1);

                    uint8 random = urand(1,2);
                    float x = KaelLocations[random][0];
                    float y = KaelLocations[random][1];

                    Creature* Phoenix = me->SummonCreature(CREATURE_PHOENIX, x, y, LOCATION_Z, 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 60000);
                    if (Phoenix)
                    {
                        Phoenix->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE + UNIT_FLAG_NON_ATTACKABLE);
                        SetThreatList(Phoenix);
                        Phoenix->AI()->AttackStart(pTarget);
                    }

                    DoScriptText(SAY_PHOENIX, me);

                    PhoenixTimer = 60000;
                } else PhoenixTimer -= diff;

                if (FlameStrikeTimer <= diff)
                {
                    if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                    {
                        me->InterruptSpell(CURRENT_CHANNELED_SPELL);
                        me->InterruptSpell(CURRENT_GENERIC_SPELL);
                        DoCast(pTarget, SPELL_FLAMESTRIKE3, true);
                        DoScriptText(SAY_FLAMESTRIKE, me);
                    }
                    FlameStrikeTimer = urand(15000,25000);
                } else FlameStrikeTimer -= diff;

                // Below 50%
                if (me->GetMaxHealth() * 0.5f > me->GetHealth())
                {
                    me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_INTERRUPT_CAST, true);
                    me->StopMoving();
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveIdle();
                    GravityLapseTimer = 0;
                    GravityLapsePhase = 0;
                    Phase = 1;
                }

                DoMeleeAttackIfReady();
            }
            break;

            case 1:
            {
                if (GravityLapseTimer <= diff)
                {
                    switch (GravityLapsePhase)
                    {
                        case 0:
                            if (FirstGravityLapse)          // Different yells at 50%, and at every following Gravity Lapse
                            {
                                DoScriptText(SAY_GRAVITY_LAPSE, me);
                                FirstGravityLapse = false;

                                if (pInstance)
                                {
                                    pInstance->HandleGameObject(pInstance->GetData64(DATA_KAEL_STATUE_LEFT), true);
                                    pInstance->HandleGameObject(pInstance->GetData64(DATA_KAEL_STATUE_RIGHT), true);
                                }
                            } else
                            {
                                DoScriptText(SAY_RECAST_GRAVITY, me);
                            }

                            DoCast(me, SPELL_GRAVITY_LAPSE_INITIAL);
                            GravityLapseTimer = 2000 + diff;// Don't interrupt the visual spell
                            GravityLapsePhase = 1;
                            break;

                        case 1:
                            TeleportPlayersToSelf();
                            GravityLapseTimer = 1000;
                            GravityLapsePhase = 2;
                            break;

                        case 2:
                            CastGravityLapseKnockUp();
                            GravityLapseTimer = 1000;
                            GravityLapsePhase = 3;
                            break;

                        case 3:
                            CastGravityLapseFly();
                            GravityLapseTimer = 30000;
                            GravityLapsePhase = 4;

                            for (uint8 i = 0; i < 3; ++i)
                            {
                                Unit* pTarget = NULL;
                                pTarget = SelectUnit(SELECT_TARGET_RANDOM,0);

                                Creature* Orb = DoSpawnCreature(CREATURE_ARCANE_SPHERE, 5, 5, 0, 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 30000);
                                if (Orb && pTarget)
                                {
                                    Orb->SetSpeed(MOVE_RUN, 0.5f);
                                    Orb->AddThreat(pTarget, 1000000.0f);
                                    Orb->AI()->AttackStart(pTarget);
                                }

                            }

                            DoCast(me, SPELL_GRAVITY_LAPSE_CHANNEL);
                            break;

                        case 4:
                            me->InterruptNonMeleeSpells(false);
                            DoScriptText(SAY_TIRED, me);
                            DoCast(me, SPELL_POWER_FEEDBACK);
                            RemoveGravityLapse();
                            GravityLapseTimer = 10000;
                            GravityLapsePhase = 0;
                            break;
                    }
                } else GravityLapseTimer -= diff;
            }
            break;
        }
    }
};

struct mob_felkael_flamestrikeAI : public ScriptedAI
{
    mob_felkael_flamestrikeAI(Creature* c) : ScriptedAI(c)
    {
        Heroic = c->GetMap()->IsHeroic();
    }

    uint32 FlameStrikeTimer;
    bool Heroic;

    void Reset()
    {
        FlameStrikeTimer = 5000;

        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        me->setFaction(14);

        DoCast(me, SPELL_FLAMESTRIKE2, true);
    }

    void EnterCombat(Unit* /*who*/) {}
    void MoveInLineOfSight(Unit* /*who*/) {}
    void UpdateAI(const uint32 diff)
    {
        if (FlameStrikeTimer <= diff)
        {
            DoCast(me, Heroic ? SPELL_FLAMESTRIKE1_HEROIC : SPELL_FLAMESTRIKE1_NORMAL, true);
            me->Kill(me);
        } else FlameStrikeTimer -= diff;
    }
};

struct mob_felkael_phoenixAI : public ScriptedAI
{
    mob_felkael_phoenixAI(Creature* c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance* pInstance;
    uint32 BurnTimer;
    uint32 Death_Timer;
    bool Rebirth;
    bool FakeDeath;

    void Reset()
    {
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE + UNIT_FLAG_NON_ATTACKABLE);
        me->AddUnitMovementFlag(MOVEFLAG_ONTRANSPORT);
        DoCast(me, SPELL_PHOENIX_BURN, true);
        BurnTimer = 2000;
        Death_Timer = 3000;
        Rebirth = false;
        FakeDeath = false;
    }

    void EnterCombat(Unit* /*who*/) {}

    void DamageTaken(Unit* /*pKiller*/, uint32 &damage)
    {
        if (damage < me->GetHealth())
            return;

        //Prevent glitch if in fake death
        if (FakeDeath)
        {
            damage = 0;
            return;

        }
        //Don't really die in all phases of Kael'Thas
        if (pInstance && pInstance->GetData(DATA_KAELTHAS_EVENT) == 0)
        {
            //prevent death
            damage = 0;
            FakeDeath = true;

            me->InterruptNonMeleeSpells(false);
            me->SetHealth(0);
            me->StopMoving();
            me->ClearComboPointHolders();
            me->RemoveAllAurasOnDeath();
            me->ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
            me->ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->ClearAllReactives();
            me->SetUInt64Value(UNIT_FIELD_TARGET,0);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            me->SetStandState(UNIT_STAND_STATE_DEAD);
        }
    }

    void JustDied(Unit* /*slayer*/)
    {
        me->SummonCreature(CREATURE_PHOENIX_EGG, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 45000);
    }

    void UpdateAI(const uint32 diff)
    {
        //If we are fake death, we cast revbirth and after that we kill the phoenix to spawn the egg.
        if (FakeDeath)
        {
            if (!Rebirth)
            {
                DoCast(me, SPELL_REBIRTH_DMG);
                Rebirth = true;
            }

            if (Rebirth)
            {

                if (Death_Timer <= diff)
                {
                    me->SummonCreature(CREATURE_PHOENIX_EGG, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 45000);
                    me->DisappearAndDie();
                    Rebirth = false;
                } else Death_Timer -= diff;
            }
        }

        if (!UpdateVictim())
            return;

        if (BurnTimer <= diff)
        {
            //spell Burn should possible do this, but it doesn't, so do this for now.
            uint16 dmg = urand(1650,2050);
            me->DealDamage(me, dmg, 0, DOT, SPELL_SCHOOL_MASK_FIRE, NULL, false);
            BurnTimer += 2000;
        } BurnTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

struct mob_felkael_phoenix_eggAI : public ScriptedAI
{
    mob_felkael_phoenix_eggAI(Creature* c) : ScriptedAI(c) {}

    uint32 HatchTimer;

    void Reset()
    {
        HatchTimer = 10000;
    }

    void EnterCombat(Unit* /*who*/) {}
    void MoveInLineOfSight(Unit* /*who*/) {}

    void UpdateAI(const uint32 diff)
    {
        if (HatchTimer <= diff)
        {
            me->SummonCreature(CREATURE_PHOENIX, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 60000);
            me->Kill(me);
        } else HatchTimer -= diff;
    }
};

struct mob_arcane_sphereAI : public ScriptedAI
{
    mob_arcane_sphereAI(Creature* c) : ScriptedAI(c) { Reset(); }

    uint32 DespawnTimer;
    uint32 ChangeTargetTimer;

    void Reset()
    {
        DespawnTimer = 30000;
        ChangeTargetTimer = urand(6000,12000);

        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        me->AddUnitMovementFlag(MOVEFLAG_ONTRANSPORT);
        me->setFaction(14);
        DoCast(me, SPELL_ARCANE_SPHERE_PASSIVE, true);
    }

    void EnterCombat(Unit* /*who*/) {}

    void UpdateAI(const uint32 diff)
    {
        if (DespawnTimer <= diff)
            me->Kill(me);
        else
            DespawnTimer -= diff;

        //Return since we have no target
        if (!UpdateVictim())
            return;

        if (ChangeTargetTimer <= diff)
        {
            if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
            {
                me->AddThreat(pTarget, 1.0f);
                me->TauntApply(pTarget);
                AttackStart(pTarget);
            }

            ChangeTargetTimer = urand(5000,15000);
        } else ChangeTargetTimer -= diff;
    }
};

bool GOUse_go_kael_orb(Player* player, GameObject* _GO)
{
    ScriptedInstance* pInstance = (ScriptedInstance*)_GO->GetInstanceData();
    if (pInstance)
    {
        if(pInstance->GetData(DATA_KAELTHAS_EVENT) == 3)
            player->TeleportTo(530, 12888.18, -6874.55, 9.06, 3.11);
          //player->CastSpell(player, SPELL_ESCAPE_TO_IOQD, true);
    }
    return true;
}

CreatureAI* GetAI_boss_felblood_kaelthas(Creature* c)
{
    return new boss_felblood_kaelthasAI(c);
}

CreatureAI* GetAI_mob_arcane_sphere(Creature* c)
{
    return new mob_arcane_sphereAI(c);
}

CreatureAI* GetAI_mob_felkael_phoenix(Creature* c)
{
    return new mob_felkael_phoenixAI(c);
}

CreatureAI* GetAI_mob_felkael_phoenix_egg(Creature* c)
{
    return new mob_felkael_phoenix_eggAI(c);
}

CreatureAI* GetAI_mob_felkael_flamestrike(Creature* c)
{
    return new mob_felkael_flamestrikeAI(c);
}

void AddSC_boss_felblood_kaelthas()
{
    Script* newscript;

    newscript = new Script;
    newscript->Name = "boss_felblood_kaelthas";
    newscript->GetAI = &GetAI_boss_felblood_kaelthas;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_arcane_sphere";
    newscript->GetAI = &GetAI_mob_arcane_sphere;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_felkael_phoenix";
    newscript->GetAI = &GetAI_mob_felkael_phoenix;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_felkael_phoenix_egg";
    newscript->GetAI = &GetAI_mob_felkael_phoenix_egg;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_felkael_flamestrike";
    newscript->GetAI = &GetAI_mob_felkael_flamestrike;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "go_kael_orb";
    newscript->pGOHello = &GOUse_go_kael_orb;
    newscript->RegisterSelf();
}
