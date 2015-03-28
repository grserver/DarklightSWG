--Automatically generated by SWGEmu Spawn Tool v0.12 loot editor.

carbine_ee3 = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "",
	directObjectTemplate = "object/weapon/ranged/carbine/carbine_ee3.iff",
	craftingValues = {
		{"mindamage",35,65,0},
		{"maxdamage",91,169,0},
		{"attackspeed",4.7,3.2,1},
		{"woundchance",5,9,0},
		{"roundsused",30,65,0},
		{"hitpoints",750,1500,0},
		{"zerorangemod",-25,-25,0},
		{"maxrangemod",-80,-80,0},
		{"midrange",27,27,0},
		{"midrangemod",-5,5,0},
	},
	customizationStringNames = {},
	customizationValues = {},

	-- randomDotChance: The chance of this weapon object dropping with a random dot on it. Higher number means less chance. Set to 0 to always have a random dot.
	randomDotChance = 800,
	junkDealerTypeNeeded = JUNKWEAPONS,
	junkMinValue = 25,
	junkMaxValue = 45

}

addLootItemTemplate("carbine_ee3", carbine_ee3)
