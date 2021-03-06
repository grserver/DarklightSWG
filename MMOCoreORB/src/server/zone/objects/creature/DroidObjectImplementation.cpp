/*
Copyright (C) 2013 <SWGEmu>

This File is part of Core3.

This program is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software
Foundation; either version 2 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to
the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Linking Engine3 statically or dynamically with other modules
is making a combined work based on Engine3.
Thus, the terms and conditions of the GNU Lesser General Public License
cover the whole combination.

In addition, as a special exception, the copyright holders of Engine3
give you permission to combine Engine3 program with free software
programs or libraries that are released under the GNU LGPL and with
code included in the standard release of Core3 under the GNU LGPL
license (or modified versions of such code, with unchanged license).
You may copy and distribute such a system following the terms of the
GNU LGPL for Engine3 and the licenses of the other code concerned,
provided that you include the source code of that other code when
and as the GNU LGPL requires distribution of source code.

Note that people who make modified versions of Engine3 are not obligated
to grant this special exception for their modified versions;
it is their choice whether to do so. The GNU Lesser General Public License
gives permission to release a modified version without this exception;
this exception also makes it possible to release a modified version
which carries forward this exception.
 */

#include "server/zone/objects/creature/DroidObject.h"
#include "server/zone/objects/intangible/PetControlDevice.h"
#include "server/zone/packets/object/ObjectMenuResponse.h"
#include "server/zone/packets/object/StartNpcConversation.h"
#include "server/zone/packets/object/StopNpcConversation.h"
#include "server/zone/templates/mobile/ConversationScreen.h"
#include "server/zone/managers/collision/CollisionManager.h"
#include "server/zone/managers/components/ComponentManager.h"
#include "server/zone/templates/customization/AssetCustomizationManagerTemplate.h"
#include "server/zone/objects/tangible/tool/CraftingTool.h"
#include "server/zone/objects/tangible/components/droid/BaseDroidModuleComponent.h"
#include "server/zone/objects/tangible/components/droid/DroidCraftingModuleDataComponent.h"
#include "server/zone/objects/tangible/components/droid/DroidArmorModuleDataComponent.h"
#include "server/zone/objects/tangible/components/droid/DroidPersonalityModuleDataComponent.h"
#include "server/zone/objects/tangible/components/droid/DroidMaintenanceModuleDataComponent.h"
#include "server/zone/objects/tangible/components/droid/DroidPersonalityModuleDataComponent.h"
#include "server/zone/managers/crafting/labratories/DroidMechanics.h"
#include "server/zone/objects/structure/StructureObject.h"
#include "server/zone/objects/creature/conversation/ConversationObserver.h"

void DroidObjectImplementation::initializeTransientMembers() {
	AiAgentImplementation::initializeTransientMembers();
	initDroidModules();
	if(isCombatDroid()) {
		// Fix messed up ham, that could occur when overall quality was negative during experimentation.
		if (this->getMaxHAM(0) < 0) {
			int overallQuality = 1;
			float maxHam = DroidMechanics::determineHam(overallQuality,species);
			for (int i = 0; i < 9; ++i) {
				if (i % 3 == 0) {
					setMaxHAM(i,maxHam,true);
					setHAM(i,maxHam,true);
				} else {
					setMaxHAM(i,maxHam/100,true);
					setHAM(i,maxHam/100,true);
				}
			}
		}
	}
}

void DroidObjectImplementation::fillAttributeList(AttributeListMessage* msg, CreatureObject* object){

	AiAgentImplementation::fillAttributeList( msg, object );

	float percentPower = ((float)power/(float)MAX_POWER)*100.0;
	msg->insertAttribute("@obj_attr_n:battery_power", String::valueOf((int)percentPower) + "%");

	if (paintCount > 0){
		msg->insertAttribute("customization_cnt", paintCount);
	}
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if( module != NULL ){
			module->fillAttributeList(msg, object);
		}
	}
}

int DroidObjectImplementation::handleObjectMenuSelect(CreatureObject* player, byte selectedID){

	// Allow modules to handle radials if desired
	PetControlDevice* pcd = getControlDevice().get().castTo<PetControlDevice*>();
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->handleObjectMenuSelect(player, selectedID, pcd);
	}

	return SceneObjectImplementation::handleObjectMenuSelect(player, selectedID); // PetMenuComponent

}

void DroidObjectImplementation::fillObjectMenuResponse(ObjectMenuResponse* menuResponse, CreatureObject* player){

	SceneObjectImplementation::fillObjectMenuResponse( menuResponse, player ); // PetMenuComponent

	// Allow modules to add radials
	PetControlDevice* pcd = getControlDevice().get().castTo<PetControlDevice*>();
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->fillObjectMenuResponse( _this.get(), menuResponse, player );
	}

}

void DroidObjectImplementation::notifyInsertToZone(Zone* zone) {
	SceneObjectImplementation::notifyInsertToZone(zone);

	if( this->linkedCreature == NULL )
		return;

	ManagedReference<CreatureObject* > linkedCreature = this->linkedCreature.get();
	if( linkedCreature == NULL )
		return;

	// Decay customized paint (if any)
	if (paintCount > 0){

		// Paint starts to fade when there are 4 calls left
		if (paintCount <= 4){

			// Send player notification of decay
			if( paintCount == 1 ){
				linkedCreature->sendSystemMessage("@pet/pet_menu:customization_gone"); // "Your droid's customization has completely faded away."
			}
			else{
				linkedCreature->sendSystemMessage("@pet/pet_menu:customization_fading"); // "Your droid's customization is fading away."
			}

			// Fade color to white
			String appearanceFilename = getObjectTemplate()->getAppearanceFilename();
			VectorMap<String, Reference<CustomizationVariable*> > variables;
			AssetCustomizationManagerTemplate::instance()->getCustomizationVariables(appearanceFilename.hashCode(), variables, false);
			for(int i = 0; i< variables.size(); ++i){
				String varkey = variables.elementAt(i).getKey();
				if (varkey.contains("color")){
					setCustomizationVariable(varkey, paintCount-1, true); // Palette values 3,2,1,0 are grey->white
				}
			}
		}
		--paintCount;
	}

}

int DroidObjectImplementation::rechargeFromBattery(CreatureObject* player){

	// Find droid battery in player inventory
	ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");
	if (inventory == NULL){
		player->sendSystemMessage("Player inventory not found");
		return 0;
	}

	ManagedReference<SceneObject*> batterySceno = NULL;
	for (int i = 0; i < inventory->getContainerObjectsSize(); ++i) {
		ManagedReference<SceneObject*> sceno = inventory->getContainerObject(i);
		if( sceno->getObjectTemplate()->getFullTemplateString() == "object/tangible/droid_battery/battery.iff" ){
			batterySceno = sceno;
		}
	}

	// Battery not found
	if( batterySceno == NULL ){
		showFlyText("npc_reaction/flytext","nobattery", 204, 0, 0); // "You don't have a power storage device."
		return 0;
	}

	// Battery found
	ManagedReference<TangibleObject*> batteryTano = cast<TangibleObject*>(batterySceno.get());
	if( batteryTano == NULL ){
		player->sendSystemMessage("Error with droid battery object");
		return 0;
	}

	// Reset power to max
	power = MAX_POWER;

	// Consume battery
	batteryTano->decreaseUseCount();

	showFlyText("npc_reaction/flytext","recharged", 0, 153, 0);  // "*Recharged*"
	doAnimation("power_up");
	return 0;

}

void DroidObjectImplementation::rechargeFromDroid(){

	// Reset power to max
	power = MAX_POWER;

	showFlyText("npc_reaction/flytext","recharged", 0, 153, 0);  // "*Recharged*"
	doAnimation("power_up");
	return;

}

void DroidObjectImplementation::rechargeOtherDroid(DroidObject* otherDroid){

	otherDroid->rechargeFromDroid();
	usePower(100);

}

void DroidObjectImplementation::handleLowPower(){

	// Send fly text
	showFlyText("npc_reaction/flytext","low_power", 204, 0, 0);  // "*Low Power*"
	doAnimation("power_down");

	// Stop following
	setOblivious();

	// Deactivate all modules
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->deactivate();
	}

	return;
}
void DroidObjectImplementation::setSpecies(int i) {
	species = i;
}
bool DroidObjectImplementation::isPowerDroid(){
	if(species == 0)
		return getObjectTemplate()->getFullTemplateString().contains( "eg_6_power_droid" );
	else
		return POWER_DROID == species;
}
void DroidObjectImplementation::initDroidModules(){
	modules.removeAll();
	ManagedReference<SceneObject*> container = getSlottedObject("crafted_components");
	if(container != NULL && container->getContainerObjectsSize() > 0) {
		SceneObject* satchel = container->getContainerObject(0);
		if(satchel != NULL && satchel->getContainerObjectsSize() > 0) {
			for (int i = 0; i < satchel->getContainerObjectsSize(); ++i) {
				ManagedReference<SceneObject*> sceno = satchel->getContainerObject(i);
				if( sceno == NULL ){
					continue;
				}
				DataObjectComponentReference* data = sceno->getDataObjectComponent();
				if(data == NULL || data->get() == NULL || !data->get()->isDroidModuleData() ){
					continue;
				}
				BaseDroidModuleComponent* module = cast<BaseDroidModuleComponent*>(data->get());
				if( module != NULL ){
					modules.add(module);
				}
			}
		}
	}
}
CraftingStation* DroidObjectImplementation::getCraftingStation(int type){
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if( module != NULL && module->actsAsCraftingStation()){
			DroidCraftingModuleDataComponent* craftingModule = dynamic_cast<DroidCraftingModuleDataComponent*>(module);
			if( craftingModule != NULL ){
				CraftingStation* craftingStation = craftingModule->getCraftingStation();
				if (craftingStation != NULL) {
					// case here to check each type
					if (craftingModule->validCraftingType(type) || (type == CraftingTool::JEDI && craftingModule->isWeaponDroidGeneric()) || (type == CraftingTool::GENERIC && craftingModule->isWeaponDroidGeneric()))
					{
						return craftingStation;
					}
				}
			}
		}
	}
	return NULL;
}

String DroidObjectImplementation::getPersonalityBase() {
	for( int i=0; i<modules.size(); i++){
		DroidPersonalityModuleDataComponent* module = cast<DroidPersonalityModuleDataComponent*>(modules.get(i));
		if( module != NULL){
			return module->getPersonalityBase();
		}
	}
	return "";
}

void DroidObjectImplementation::onStore() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->onStore();
	}
}
void DroidObjectImplementation::onCall() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->onCall();
	}
}
void DroidObjectImplementation::loadSkillMods(CreatureObject* player) {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->loadSkillMods(player);
	}
}
void DroidObjectImplementation::unloadSkillMods(CreatureObject* player) {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->unloadSkillMods(player);
	}
}

void DroidObjectImplementation::handleChat(CreatureObject* speaker, const String& message){
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		module->handlePetCommand( message, speaker );
	}
}
BaseDroidModuleComponent* DroidObjectImplementation::getModule(const String& name) {
	for(int i=0;i<modules.size();i++) {
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->getModuleName() == name) {
			return module;
		}
	}
	return NULL;
}
bool DroidObjectImplementation::isAdvancedModel() {
	return getCreatureTemplate()->getObjectName().contains("advanced");
}
void DroidObjectImplementation::runModulePowerDrain() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		int drain = module->getBatteryDrain();
		if(drain > 0)
			usePower(drain);
	}
}
bool DroidObjectImplementation::isCombatDroid() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->isCombatModule()) {
			return true;
		}
	}
	// inante comabt ability, regardless of module installed
	if (species == PROBOT || species == DZ70)
		return true;

	return false;
}
bool DroidObjectImplementation::isTrapDroid() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->getModuleName() == "trap_module") {
			return true;
		}
	}
	return false;
}
bool DroidObjectImplementation::hasStorage() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->getModuleName() == "item_storage_module") {
			return true;
		}
	}
	return false;
}

bool DroidObjectImplementation::isMaintenanceDroid() {
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->getModuleName() == "maintenance_module") {
			return true;
		}
	}
	return false;
}

bool DroidObjectImplementation::assignStructure( StructureObject* structure ){
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->getModuleName() == "maintenance_module") {
			DroidMaintenanceModuleDataComponent* maintModule = dynamic_cast<DroidMaintenanceModuleDataComponent*>(module);
			if( maintModule != NULL ){
				return maintModule->assignStructure( structure->getObjectID() );
			}
		}
	}
	return false;
}
bool DroidObjectImplementation::isStructureAssigned( StructureObject* structure ){
	for( int i=0; i<modules.size(); i++){
		BaseDroidModuleComponent* module = modules.get(i);
		if(module->getModuleName() == "maintenance_module") {
			DroidMaintenanceModuleDataComponent* maintModule = dynamic_cast<DroidMaintenanceModuleDataComponent*>(module);
			if( maintModule != NULL ){
				return maintModule->isAssignedTo( structure->getObjectID() );
			}
		}
	}
	return false;
}
bool DroidObjectImplementation::sendConversationStartTo(SceneObject* player) {
	if (!player->isPlayerCreature() || isDead())
		return false;

	BaseDroidModuleComponent* m = getModule("personality_chip");
	if (m == NULL) {
		return false;
	}

	DroidPersonalityModuleDataComponent* personality = dynamic_cast<DroidPersonalityModuleDataComponent*>(m);
	if (personality == NULL) {
		return false;
	}

	if (personality->getPersonalityConversationTemplate() == 0) {
		return false;
	}

	//Face player.
	faceObject(player);

	PatrolPoint current(coordinates.getPosition(), getParent().get());

	broadcastNextPositionUpdate(&current);

	CreatureObject* playerCreature = cast<CreatureObject*>( player);
	StartNpcConversation* conv = new StartNpcConversation(playerCreature, getObjectID(), "");
	player->sendMessage(conv);

	SortedVector<ManagedReference<Observer*> > observers = getObservers(ObserverEventType::STARTCONVERSATION);

	for (int i = 0;  i < observers.size(); ++i) {
		if (dynamic_cast<ConversationObserver*>(observers.get(i).get()) != NULL) {
			return true;
		}
	}
	//Create conversation observer.
	ConversationObserver* conversationObserver = ConversationManager::instance()->getConversationObserver(personality->getPersonalityConversationTemplate());

	if (conversationObserver != NULL) {
		//Register observers.
		registerObserver(ObserverEventType::CONVERSE, conversationObserver);
		registerObserver(ObserverEventType::STARTCONVERSATION, conversationObserver);
		registerObserver(ObserverEventType::SELECTCONVERSATION, conversationObserver);
		registerObserver(ObserverEventType::STOPCONVERSATION, conversationObserver);
	} else {
		error("Could not create conversation observer.");
		return false;
	}

	return true;
}
String DroidObjectImplementation::getPersonalityStf() {
	for( int i=0; i<modules.size(); i++){
		DroidPersonalityModuleDataComponent* module = cast<DroidPersonalityModuleDataComponent*>(modules.get(i));
		if( module != NULL){
			return module->getPersonalityStf();
		}
	}
	return "";

}
