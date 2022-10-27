#pragma once
#include <string>
#include "Client.h"

class Player
{
public:
	Player();
	Player(uint32_t ID, Client Client, std::string Name);
	bool operator==(const Player& Player) const;
	bool operator!=(const Player& Player) const;

public:
	bool m_HasLostGame;
	bool m_HasLostConnection;
	uint8_t m_ID;
	uint32_t m_WorkersAlive;
	Client m_Client;
	std::string m_Name;
};