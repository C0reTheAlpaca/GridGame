#include "Player.h"

Player::Player()
{
	m_ID = 0;
	m_Client = Client(-1, "");
	m_Name = "";
	m_HasLostGame = false;
	m_HasLostConnection = false;
	m_WorkersAlive = 0;
};

Player::Player(uint32_t ID, Client Client, std::string Name)
{
	m_ID = ID;
	m_Client = Client;
	m_Name = Name;
	m_HasLostGame = false;
	m_HasLostConnection = false;
	m_WorkersAlive = 0;
}

bool Player::operator==(const Player& Player) const
{
	return m_Client == Player.m_Client;
}

bool Player::operator!=(const Player& Player) const 
{
	return m_Client != Player.m_Client;
}