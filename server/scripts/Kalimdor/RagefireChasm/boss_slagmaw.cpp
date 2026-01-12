/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
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

#include "Containers.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ragefire_chasm.h"

enum SlagmawSpells
{
    SPELL_LAVA_SPIT             = 119434,
    SPELL_MAGNAW_SUBMERGE       = 120384,
    SPELL_MAGNAW_TELEPORT_NORTH = 119424, // Serverside
    SPELL_MAGNAW_TELEPORT_EAST  = 119425, // Serverside
    SPELL_MAGNAW_TELEPORT_SOUTH = 119426, // Serverside
    SPELL_MAGNAW_TELEPORT_WEST  = 119428  // Serverside
};

Position const SlagmawTeleportPositions[4] = 
{
    {-225.288, 164.8182, -25.65, 3.933},//n
    {-228.07, 135.326, -24.46, 2.322},//e
    {-262.4424, 136.401, -23.58, 0.646643},//s
    {-257.857, 173.358, -25.265, 5.5386}//w
};

enum SlagmawEvents
{
    EVENT_LAVA_SPIT       = 1,
    EVENT_TELEPORT,
    EVENT_EMERGE,
    EVENT_BOUNDARY_CHECK,
};

std::array<uint32, 4> const SlagmawTeleportSpells =
{
    SPELL_MAGNAW_TELEPORT_NORTH,
    SPELL_MAGNAW_TELEPORT_EAST,
    SPELL_MAGNAW_TELEPORT_SOUTH,
    SPELL_MAGNAW_TELEPORT_WEST
};

// 61463 - Slagmaw
struct boss_slagmaw : public BossAI
{
    boss_slagmaw(Creature* creature) : BossAI(creature, BOSS_SLAGMAW), _lavaSpitCounter(0), _lastTeleportSpell(SPELL_MAGNAW_TELEPORT_WEST) 
    {
        SetCombatMovement(false);
    }

    void Reset() override
    {
        _Reset();
        _lavaSpitCounter = 0;
        _lastTeleportSpell = SPELL_MAGNAW_TELEPORT_WEST;
        SetCombatMovement(false);
        // me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
    }

    void EnterEvadeMode() override
    {
        BossAI::EnterEvadeMode();
        _DespawnAtEvade();
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
    }

    void EnterCombat(Unit* who) override
    {
        BossAI::EnterCombat(who);

        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);

        events.ScheduleEvent(EVENT_LAVA_SPIT, 1s);
        events.ScheduleEvent(EVENT_BOUNDARY_CHECK, 2500ms);
    }

    void HandleSubmergePhase()
    {
        DoCastSelf(SPELL_MAGNAW_SUBMERGE);
        _lavaSpitCounter = 0;

        events.ScheduleEvent(EVENT_TELEPORT, 3s);
    }

    int8 _lastTeleportIndex = -1;
    uint8 GetNextTeleportIndex()
    {
        std::array<uint8, 3> indices = { };
        uint8 writeIdx = 0;

        // Fill with 0..3 except the last teleport index
        for (uint8 i = 0; i < 4; ++i)
        {
            if (i == _lastTeleportIndex)
                continue;
            indices[writeIdx++] = i;
        }

        uint8 newIndex = Trinity::Containers::SelectRandomContainerElement(indices);
        _lastTeleportIndex = newIndex;
        return newIndex;
    }
    // uint32 GetNextTeleportSpell()
    // {
    //     std::array<uint32, 3> teleportSpells = { };
    //     std::ranges::remove_copy(SlagmawTeleportSpells, teleportSpells.begin(), _lastTeleportSpell);
    //     _lastTeleportSpell = Trinity::Containers::SelectRandomContainerElement(teleportSpells);
    //     return _lastTeleportSpell;
    // }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_LAVA_SPIT:
            {
                if (_lavaSpitCounter < 5)
                {
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                    {
                        DoCast(target, SPELL_LAVA_SPIT);
                        _lavaSpitCounter++;
                    }
                    events.Repeat(1s);
                    break;
                }
                else if (_lavaSpitCounter == 5)
                {
                    HandleSubmergePhase();
                    break;
                }
                break;
            }
            case EVENT_TELEPORT:
            {
                uint8 index = GetNextTeleportIndex();

                me->NearTeleportTo(SlagmawTeleportPositions[index], true);

                events.ScheduleEvent(EVENT_EMERGE, 1s);
                break;
            }
            case EVENT_EMERGE:
            {
                me->RemoveAurasDueToSpell(SPELL_MAGNAW_SUBMERGE);
                events.ScheduleEvent(EVENT_LAVA_SPIT, 1s);
                break;
            }
            case EVENT_BOUNDARY_CHECK:
            {
                if (me->getVictim()->GetDistance(me) > 50.0f)
                    EnterEvadeMode();
                events.ScheduleEvent(EVENT_BOUNDARY_CHECK, 2500ms);
                break;
            }
            default:
                break;
        }

        DoMeleeAttackIfReady();
    }

private:
    uint8 _lavaSpitCounter;
    uint32 _lastTeleportSpell;
};

void AddSC_boss_slagmaw()
{
    RegisterRagefireChasmCreatureAI(boss_slagmaw);
}
