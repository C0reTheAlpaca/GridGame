#pragma once
#include <mutex>
#include <ctime>
#include <queue>
#include <vector>
#include "Field.h"
#include "Server.h"
#include "Player.h"
#include "Packet.h"
#include "Serializer.h"

typedef std::map<uint8_t, Player>::iterator PlayerIterator;

class GridGame
{
public:
	GridGame(Server* pServer);
	void Routine();
	void Receive(Packet Data, Client Client);
	void HandleConnect(Packet Data, Client Client);
	void Disconnect(Client Client);
	void Kick(Client Client);
	void HandleLeave(PlayerIterator PlayerIt);
	void HandleMove(Packet Data, PlayerIterator PlayerIt);
	void HandleEndTurn(PlayerIterator PlayerIt);
	void Tick();
	void StartGame();
	void PregenerateFood();
	void SendPlayerData(Player Player);
	void SendClientUpdate(Player Player);
	void StartNewTurn();

	bool CheckWinConditions();
	bool IsValidMove(bool Split, int FromX, int FromY, int ToX, int ToY, PlayerIterator PlayerIt);
	PlayerIterator GetPlayerByIP(std::string IP);
	PlayerIterator GetPlayerByClient(Client Client);

private:
	bool m_NewGame;
	bool m_TurnEnded;
	bool m_GameRunning;
	uint16_t m_GridWidth;
	uint16_t m_GridHeight;
	Server* m_pServer;
	Player m_TurnPlayer;
	std::time_t m_QueueStartTime;
	std::time_t m_TurnTimeout;
	std::mutex m_Mutex;
	std::map<uint8_t, Player> m_Players;
	std::vector<FieldUpdate> m_FieldUpdates;
	std::vector<FieldUpdate> m_FutureFieldUpdates;
	std::vector<std::vector<Field>> m_Grid;
};

extern GridGame* g_pGridGame;

