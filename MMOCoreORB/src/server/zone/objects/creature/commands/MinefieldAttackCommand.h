/*
 * MinefieldAttackCommand.h
 *
 *  Created on: Dec 21, 2012
 *      Author: root
 */

#ifndef MINEFIELDATTACKCOMMAND_H_
#define MINEFIELDATTACKCOMMAND_H_
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/managers/combat/CombatManager.h"
#include "CombatQueueCommand.h"

class MinefieldAttackCommand : public CombatQueueCommand {
public:

	MinefieldAttackCommand(const String& name, ZoneProcessServer* server)
		: CombatQueueCommand(name, server) {
	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		return 0;
	}

};






#endif /* MINEFIELDATTACKCOMMAND_H_ */
