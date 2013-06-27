/*
 * This file is part of the BlizzLikeCore Project.
 * See CREDITS and LICENSE files for Copyright information.
 */

#ifndef __BATTLEGROUNDBE_H
#define __BATTLEGROUNDBE_H

class BattleGround;

enum BattleGroundBEObjectTypes
{
    BG_BE_OBJECT_DOOR_1         = 0,
    BG_BE_OBJECT_DOOR_2         = 1,
    BG_BE_OBJECT_DOOR_3         = 2,
    BG_BE_OBJECT_DOOR_4         = 3,
    BG_BE_OBJECT_BUFF_1         = 4,
    BG_BE_OBJECT_BUFF_2         = 5,
    BG_BE_OBJECT_MAX            = 6
};

enum BattleGroundBEObjects
{
    BG_BE_OBJECT_TYPE_DOOR_1    = 183971,
    BG_BE_OBJECT_TYPE_DOOR_2    = 183973,
    BG_BE_OBJECT_TYPE_DOOR_3    = 183970,
    BG_BE_OBJECT_TYPE_DOOR_4    = 183972,
    BG_BE_OBJECT_TYPE_BUFF_1    = 184663,
    BG_BE_OBJECT_TYPE_BUFF_2    = 184664
};

class BattleGroundBEScore : public BattleGroundScore
{
    public:
        BattleGroundBEScore() {};
        virtual ~BattleGroundBEScore() {};
};

class BattleGroundBE : public BattleGround
{
    friend class BattleGroundMgr;

    public:
        BattleGroundBE();
        ~BattleGroundBE();
        void Update(time_t diff);

        /* inherited from BattlegroundClass */
        virtual void AddPlayer(Player* plr);
        virtual void StartingEventCloseDoors();
        virtual void StartingEventOpenDoors();

        void RemovePlayer(Player* plr, uint64 guid);
        void HandleAreaTrigger(Player* Source, uint32 Trigger);
        bool SetupBattleGround();
        void ResetBGSubclass();
        virtual void FillInitialWorldStates(WorldPacket &d);
        void HandleKillPlayer(Player* player, Player* killer);
        bool HandlePlayerUnderMap(Player* plr);

        /* Scorekeeping */
        void UpdatePlayerScore(Player* Source, uint32 type, uint32 value);
};
#endif

