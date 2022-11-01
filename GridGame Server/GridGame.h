#pragma once
#include "Serializer.h"
#include "Server.h"
#include "Player.h"
#include "Field.h"
#include <mutex>
#include <ctime>
#include <vector>
#include <queue>
#include "Packet.h"
#include "Field.h"

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
	void Run();
	void PrepareGame();
	void GenerateFood();
	void SendClientUpdate(Player Player);
	void StartNewTurn();

	bool CheckConditions();
	bool IsValidMove(bool Split, int FromX, int FromY, int ToX, int ToY, PlayerIterator PlayerIt);
	PlayerIterator GetPlayerFromIP(std::string IP);
	PlayerIterator GetPlayerFromClient(Client Client);

private:
	bool m_NewGame;
	bool m_GameRunning;
	uint32_t m_GridWidth;
	uint32_t m_GridHeight;
	Player m_TurnPlayer;
	Server* m_pServer;
	std::time_t m_QueueStartTime;
	std::time_t m_TurnTimeout;
	std::mutex m_Mutex;
	std::map<uint8_t, Player> m_Players;
	std::vector<Move> m_Moves;
	std::vector<FoodUpdate> m_FoodUpdates;
	std::vector<std::vector<Field>> m_Grid;
};

extern GridGame* g_pGridGame;

