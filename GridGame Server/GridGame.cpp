#include <format>
#include <chrono>
#include <thread>
#include <numeric>
#include <iostream>
#include <algorithm>
#include "Server.h"
#include "Utility.h"
#include "GridGame.h"
#include "GameNetInstructions.h"

#undef max
#undef min

GridGame* g_pGridGame = nullptr;

GridGame::GridGame(Server* pServer)
{
	m_NewGame = true;
	m_TurnEnded = false;
	m_GameRunning = false;
	m_QueueStartTime = 0;
	m_TurnTimeout = 0;
	m_GridWidth = 25;
	m_GridHeight = 25;
	m_pServer = pServer;

	m_Grid.resize(m_GridWidth);

	for (uint16_t x = 0; x < m_GridWidth; x++)
	{
		m_Grid[x].resize(m_GridHeight);
	}

	m_pServer->RegisterInstruction(NetDataType::NET_CONNECT, Connect);
	m_pServer->RegisterInstruction(NetDataType::NET_CONNECT_ACK, ConnectAck);
	m_pServer->RegisterInstruction(NetDataType::NET_LEAVE, Instruction());
	m_pServer->RegisterInstruction(NetDataType::NET_MOVE, Move);
	m_pServer->RegisterInstruction(NetDataType::NET_END_TURN, Instruction());
	m_pServer->RegisterInstruction(NetDataType::NET_BROADCAST, Broadcast);
	m_pServer->RegisterInstruction(NetDataType::NET_GAME_START, GameStart);
	m_pServer->RegisterInstruction(NetDataType::NET_GAME_DATA, GameData);
}

void GridGame::Routine()
{
	while (true) // todo
	{
		std::time_t Now = std::time(nullptr);

		// Reset queue if insufficient players
		if (!m_GameRunning && m_Players.size() < 2)
		{
			m_QueueStartTime = 0;
			continue;
		}

		// Wait for min 2 players and start queue timer
		if (!m_GameRunning && m_QueueStartTime == 0 && m_Players.size() >= 2)
		{
			m_QueueStartTime = Now;
			continue;
		}

		// Start game if enough players & queue timer elapsed
		if (!m_GameRunning && Now - m_QueueStartTime > 5) // todo: add proper lobbies?
		{
			std::lock_guard LockGuard(m_Mutex);
			StartGame();
			PregenerateFood();
			StartNewTurn();
			Tick();
		}

		// Move time exceeded or new turn
		if (m_GameRunning && (m_TurnEnded || Now >= m_TurnTimeout))
		{
			std::lock_guard LockGuard(m_Mutex);
			PregenerateFood();
			StartNewTurn();
			Tick();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}
}
void GridGame::StartGame()
{
	m_NewGame = true;
	m_GameRunning = true;
	m_QueueStartTime = 0;

	// Init grid
	for (uint16_t x = 0; x < m_GridWidth; x++)
	{
		for (uint16_t y = 0; y < m_GridHeight; y++)
		{
			m_Grid[x][y] = Field();
		}
	}

	for (auto& Player : m_Players)
	{
		// Generate workers
		uint16_t x, y;

		do
		{
			x = Utility::GetRandomInteger<uint16_t>(0, m_GridWidth - 1);
			y = Utility::GetRandomInteger<uint16_t>(0, m_GridHeight - 1);

		} while (m_Grid[x][y].m_FieldType != Field::FieldType::FIELD_EMPTY);

		// Add worker & prepare for submit to players
		m_Grid[x][y] = Field(Field::FieldType::FIELD_WORKER, Player.second.m_ID, 5);
		m_FieldUpdates.push_back(FieldUpdate(x, y, m_Grid[x][y]));

		// Init Player
		Player.second.m_WorkersAlive = 1;
		Player.second.m_HasLostGame = false;

		// Update player data
		SendPlayerData(Player.second);
	}
}

void GridGame::PregenerateFood()
{
	// Count amount of food on the field
	int FoodCount = 0;

	for (uint32_t x = 0; x < m_GridWidth; x++)
	{
		for (uint32_t y = 0; y < m_GridHeight; y++)
		{
			if (m_Grid[x][y].m_FieldType == Field::FieldType::FIELD_FOOD)
				FoodCount++;
		}
	}

	// Only respawn food if none left
	if (FoodCount > 0)
		return;
	
	// Integrate Pregenerate food of last update
	for (FieldUpdate& Update : m_FutureFieldUpdates)
	{
		if (Update.Field.m_FieldType != Field::FieldType::FIELD_FOOD)
			continue;

		Field* pField = &m_Grid[Update.x][Update.y];

		if (pField->m_FieldType == Field::FieldType::FIELD_WORKER)
		{
			pField->m_Power += Update.Field.m_Power;
			Update.Field = *pField;
		}
		else
		{
			*pField = Field(Field::FieldType::FIELD_FOOD, FIELD_NO_OWNER, 1);
			m_FieldUpdates.push_back(FieldUpdate(Update.x, Update.y, *pField));
		}
	}

	m_FutureFieldUpdates.clear();

	// Pregenerate food for next update unless first turn
	for (int i = 0; i < m_Players.size() * 2; i++)
	{
		uint16_t x, y;
		Field* pField = nullptr;

		// If new game repeat until food doesn't spawn on worker
		// TODO: Randomize only with empty fields
		do
		{
			x = Utility::GetRandomInteger<uint16_t>(0, m_GridWidth - 1);
			y = Utility::GetRandomInteger<uint16_t>(0, m_GridHeight - 1);
			pField = &m_Grid[x][y];
		}
		while (pField->m_FieldType != Field::FieldType::FIELD_EMPTY);

		if (m_NewGame)
		{
			// Integrate to field directly
			*pField = Field(Field::FieldType::FIELD_FOOD, FIELD_NO_OWNER, 1);
			m_FieldUpdates.push_back(FieldUpdate(x, y, *pField));
		}
		else 
		{
			// Save for later integration
			Field Food(Field::FieldType::FIELD_FOOD, FIELD_NO_OWNER, 1);
			m_FutureFieldUpdates.push_back(FieldUpdate(x, y, Food));
		}
	}
}

void GridGame::Tick()
{
	// todo: receive client data in 1 thread 
	std::time_t Now = std::time(nullptr);

	if (m_NewGame)
	{
		m_TurnTimeout = Now + 10;
		m_NewGame = false;
	}

	m_GameRunning = CheckWinConditions();
}

bool GridGame::CheckWinConditions()
{
	std::size_t PlayersAlive = m_Players.size();

	// Check lost
	for (auto& Player : m_Players)
	{
		if (!Player.second.m_HasLostGame && Player.second.m_WorkersAlive <= 0)
		{
			Player.second.m_HasLostGame = true;
			std::string Message = std::format("Player [{}] lost the game.", Player.second.m_Name);

			Packet Broadcast(NetDataType::NET_BROADCAST);
			Broadcast.push_back(Message);

			m_pServer->GetSerializer()->SerializeSend(
				Broadcast,
				Player.second.m_Client.m_Socket
			);

			std::cout << Message << std::endl;
		}

		if (Player.second.m_HasLostGame)
			PlayersAlive--;
	}

	// Too many players left
	if (PlayersAlive >= 2)
		return true;

	// Check draw
	if (PlayersAlive == 0)
	{
		std::string Message = "The game ended in draw.";

		Packet Broadcast(NetDataType::NET_BROADCAST);
		Broadcast.push_back(Message);

		for (auto& Player : m_Players)
		{
			m_pServer->GetSerializer()->SerializeSend(
				Broadcast,
				Player.second.m_Client.m_Socket
			);
		}

		std::cout << Message << std::endl;

		return false;
	}

	// Check win
	if (PlayersAlive == 1)
	{
		for (const auto& Player : m_Players)
		{
			if (Player.second.m_WorkersAlive > 0)
			{
				std::string Message = std::format("Player [{}] has won the game.", Player.second.m_Name);

				Packet Broadcast(NetDataType::NET_BROADCAST);
				Broadcast.push_back(Message);

				m_pServer->GetSerializer()->SerializeSend(
					Broadcast,
					Player.second.m_Client.m_Socket
				);

				std::cout << Message << std::endl;
			}
		}

		return false;
	}

	return false;
}

void GridGame::StartNewTurn()
{
	// Increment turn player
	PlayerIterator PlayerNextIt;
	PlayerIterator PlayerIt = GetPlayerByClient(m_TurnPlayer.m_Client);

	if (PlayerIt == m_Players.end())
		PlayerNextIt = m_Players.begin();
	else
		PlayerNextIt = std::next(PlayerIt, 1);
	
	if (PlayerNextIt == m_Players.end())
		PlayerNextIt = m_Players.begin();

	m_TurnPlayer = PlayerNextIt->second;
	m_TurnTimeout = std::time(nullptr) + 10;

	// Send updated grid data to players
	for (const auto& Player : m_Players)
	{
		SendClientUpdate(Player.second);
	}

	m_TurnEnded = false;
	m_FieldUpdates.clear();
}

void GridGame::HandleEndTurn(PlayerIterator PlayerIt)
{
	if (PlayerIt->second != m_TurnPlayer)
		return;

	// Reset worker count
	for (auto& Player : m_Players)
	{
		Player.second.m_WorkersAlive = 0;
	}

	for (uint16_t x = 0; x < m_GridWidth; x++)
	{
		for (uint16_t y = 0; y < m_GridHeight; y++)
		{
			Field* pField = &m_Grid[x][y];

			// Reset field
			pField->m_WasMoved = false;

			// Set new worker count
			if (pField->m_FieldType == Field::FieldType::FIELD_WORKER && pField->m_OwnerID != FIELD_NO_OWNER)
				m_Players[pField->m_OwnerID].m_WorkersAlive++;
		}
	}

	m_TurnEnded = true;
}

void GridGame::SendPlayerData(Player APlayer)
{
	// Send other players data to player
	std::vector<PacketStruct> Players;
	for (const std::pair<uint8_t, Player>& Player : m_Players)
	{
		Players.push_back(
			{
				(uint8_t)Player.second.m_ID,
				(std::string)Player.second.m_Name,
			}
		);
	}

	Packet Packet(NetDataType::NET_GAME_START);
	Packet.push_back(m_GridWidth);
	Packet.push_back(m_GridHeight);
	Packet.push_back(Players);

	m_pServer->GetSerializer()->SerializeSend(
		Packet,
		APlayer.m_Client.m_Socket
	);
}

void GridGame::SendClientUpdate(Player APlayer)
{
	std::vector<PacketStruct> FoodUpdates;
	std::vector<PacketStruct> FieldUpdates;

	for (const FieldUpdate& Update : m_FutureFieldUpdates)
	{
		if (Update.Field.m_FieldType != Field::FieldType::FIELD_FOOD)
			continue;

		FoodUpdates.push_back(
			{
				(uint8_t)Update.x,
				(uint8_t)Update.y,
			}
		);
	}

	for (const FieldUpdate& Update : m_FieldUpdates)
	{
		FieldUpdates.push_back(
			{
				(uint8_t)Update.x,
				(uint8_t)Update.y,
				(uint8_t)Update.Field.m_FieldType,
				(uint8_t)Update.Field.m_OwnerID,
				(uint16_t)Update.Field.m_Power
			}
		);
	}

	Packet Packet(NetDataType::NET_GAME_DATA);
	Packet.push_back(m_TurnPlayer.m_ID);
	Packet.push_back(m_TurnTimeout);
	Packet.push_back(FieldUpdates);
	Packet.push_back(FoodUpdates);

	m_pServer->GetSerializer()->SerializeSend(
		Packet,
		APlayer.m_Client.m_Socket
	);
}

void GridGame::Kick(Client Client)
{
	// Check if this client is actually a player
	PlayerIterator PlayerIt = GetPlayerByClient(Client);
	if (PlayerIt == m_Players.end())
		return;

	Player Player = PlayerIt->second;

	// Create message
	std::string Message = std::format("Player [{}] has left the game.", Player.m_Name);

	Packet Packet(NetDataType::NET_BROADCAST);
	Packet.push_back(Message);

	// Broadcast
	for (const auto& Player : m_Players)
	{
		m_pServer->GetSerializer()->SerializeSend(
			Packet,
			Player.second.m_Client.m_Socket
		);
	}

	std::cout << std::format("Player [{}] send an invalid packet and was disconnected.", Player.m_Name) << std::endl;
}

void GridGame::Receive(Packet Data, Client Client)
{
	if (Data.m_Magic == NetDataType::NET_CONNECT)
	{
		HandleConnect(Data, Client);
		return;
	}

	// Check if this client is actually a player
	auto PlayerIt = GetPlayerByClient(Client);
	if (PlayerIt == m_Players.end())
		return;

	switch (Data.m_Magic)
	{
	case NetDataType::NET_LEAVE:
		HandleLeave(PlayerIt);
		break;
	case NetDataType::NET_MOVE:
		HandleMove(Data, PlayerIt);
		break;
	case NetDataType::NET_END_TURN:
		HandleEndTurn(PlayerIt);
		break;
	}
}

void GridGame::Disconnect(Client Client)
{
	// Check if this client is actually a player
	auto PlayerIt = GetPlayerByClient(Client);
	if (PlayerIt == m_Players.end())
		return;

	Player Player = PlayerIt->second;
	m_Players[Player.m_ID].m_HasLostConnection = true;

	// Create message
	std::string Message = std::format("Player [{}] lost connection.", Player.m_Name);
	
	Packet Packet(NetDataType::NET_BROADCAST);
	Packet.push_back(Message);

	// Broadcast
	for (const auto& Player : m_Players)
	{
		m_pServer->GetSerializer()->SerializeSend(
			Packet,
			Player.second.m_Client.m_Socket
		);
	}

	std::cout << Message << std::endl;;
}

void GridGame::HandleLeave(PlayerIterator PlayerIt)
{
	Player Player = PlayerIt->second;

	// Create message
	std::string Message = std::format("Player [{}] has left the game.", Player.m_Name);

	Packet Packet(NetDataType::NET_BROADCAST);
	Packet.push_back(Message);

	// Remove player
	m_Players.erase(PlayerIt);

	// Broadcast
	for (const auto& Player : m_Players)
	{
		m_pServer->GetSerializer()->SerializeSend(
			Packet,
			Player.second.m_Client.m_Socket
		);
	}

	std::cout << Message << std::endl;;
}

void GridGame::HandleConnect(Packet PacketIn, Client Client)
{
	Player APlayer;
	PlayerIterator PlayerIt = GetPlayerByIP(Client.m_IP);
	bool IsReconnect = (PlayerIt != m_Players.end()) && PlayerIt->second.m_HasLostConnection;

	if (!IsReconnect && m_GameRunning)
		return;

	// Remove illegal chars from player name
	std::string PlayerName = std::get<std::string>(PacketIn.m_Data[0]);

	PlayerName.erase(std::remove_if(PlayerName.begin(), PlayerName.end(),
		[](auto const& Char) -> bool { return !std::isalnum(Char); }), PlayerName.end()
	);

	std::string Message;

	if (IsReconnect)
	{
		uint8_t PlayerID = PlayerIt->second.m_ID;
		m_Players[PlayerID].m_HasLostConnection = false;
		m_Players[PlayerID].m_Client.m_Socket = Client.m_Socket;
		APlayer = m_Players[PlayerID];

		Message = std::format("Player [{}] has reconnected the game.", m_Players[PlayerID].m_Name);
	}
	else
	{
		uint8_t LowestID = 0;
		uint8_t Index = (uint8_t)std::distance(m_Players.begin(), m_Players.end());

		for (uint8_t i = 0; i < Index + 1; i++)
			if (m_Players.count(i) == 0)
				LowestID = i;

		APlayer = Player(LowestID, Client, PlayerName);
		m_Players[LowestID] = APlayer;

		Message = std::format("Player [{}] has joined the game.", APlayer.m_Name);
	}

	// Send player ID
	Packet ConnectACK(NetDataType::NET_CONNECT_ACK);
	ConnectACK.push_back(APlayer.m_ID);

	m_pServer->GetSerializer()->SerializeSend(
		ConnectACK,
		APlayer.m_Client.m_Socket
	);

	// Send connect message to all players
	Packet Broadcast(NetDataType::NET_BROADCAST);
	Broadcast.push_back(Message);

	// Broadcast
	for (const auto& Player : m_Players)
	{
		if (Player.second == APlayer)
			continue;

		m_pServer->GetSerializer()->SerializeSend(
			Broadcast,
			Player.second.m_Client.m_Socket
		);
	}

	if (IsReconnect)
	{
		SendPlayerData(APlayer);
		SendClientUpdate(APlayer);
	}

	std::cout << Message << std::endl;
}

void GridGame::HandleMove(Packet Packet, PlayerIterator PlayerIt)
{
	// Ignore if this isnt the players turn
	if (m_TurnPlayer != PlayerIt->second)
		return;

	bool ShouldSplit = std::get<bool>(Packet.m_Data[0]);
	uint32_t FromX = std::get<uint32_t>(Packet.m_Data[1]);
	uint32_t FromY = std::get<uint32_t>(Packet.m_Data[2]);
	uint32_t ToX = std::get<uint32_t>(Packet.m_Data[3]);
	uint32_t ToY = std::get<uint32_t>(Packet.m_Data[4]);

	if (!IsValidMove(ShouldSplit, FromX, FromY, ToX, ToY, PlayerIt))
		return; // todo: notice player, kick, make lose?

	Field* pOriginField = &m_Grid[FromX][FromY];
	Field* pTargetField = &m_Grid[ToX][ToY];

	std::unique_ptr<Field> pMover = std::make_unique<Field>();

	if (ShouldSplit)
	{
		int16_t Power = pOriginField->m_Power;

		pMover->m_FieldType = Field::FieldType::FIELD_WORKER;
		pMover->m_OwnerID = pOriginField->m_OwnerID;
		pMover->m_Power = (int)std::ceil(Power / 2.0);
		pMover->m_WasMoved = true;
		pOriginField->m_Power = (int)std::floor(Power / 2.0);
		m_FieldUpdates.push_back(FieldUpdate(FromX, FromY, *pOriginField));
	}
	else 
	{
		pMover->m_FieldType = pOriginField->m_FieldType;
		pMover->m_OwnerID = pOriginField->m_OwnerID;
		pMover->m_Power = pOriginField->m_Power;
		pMover->m_WasMoved = true;

		// Reset the field we come from
		pOriginField->Reset();
		m_FieldUpdates.push_back(FieldUpdate(FromX, FromY, *pOriginField));
	}

	if (pTargetField->m_FieldType == Field::FieldType::FIELD_EMPTY)
	{
		// Move
		*pTargetField = *pMover;
		m_FieldUpdates.push_back(FieldUpdate(ToX, ToY, *pTargetField));

		return;
	}

	if (pTargetField->m_FieldType == Field::FieldType::FIELD_FOOD)
	{
		// Move & eat food
		pTargetField->m_OwnerID = pMover->m_OwnerID;
		pTargetField->m_Power = pMover->m_Power + pTargetField->m_Power;
		pTargetField->m_FieldType = Field::FieldType::FIELD_WORKER;
		pTargetField->m_WasMoved = true;
		m_FieldUpdates.push_back(FieldUpdate(ToX, ToY, *pTargetField));

		return;
	}

	if (pTargetField->m_FieldType == Field::FieldType::FIELD_WORKER)
	{
		if (pTargetField->m_OwnerID == pMover->m_OwnerID)
		{
			// Move and merge
			pMover->m_Power += pTargetField->m_Power;
			*pTargetField = *pMover;
			m_FieldUpdates.push_back(FieldUpdate(ToX, ToY, *pTargetField));

			return;
		}

		if (pMover->m_Power == pTargetField->m_Power)
		{
			// Kill eachother
			pTargetField->Reset();
			m_FieldUpdates.push_back(FieldUpdate(ToX, ToY, *pTargetField));

			return;
		}

		if (pMover->m_Power > pTargetField->m_Power)
		{
			// Win fight and "gain" 1 power
			pMover->m_Power = (pMover->m_Power - pTargetField->m_Power) + 1;
			*pTargetField = *pMover;
			m_FieldUpdates.push_back(FieldUpdate(ToX, ToY, *pTargetField));

			return;
		}

		if (pMover->m_Power < pTargetField->m_Power)
		{
			// Lose fight and enemy "gain" 1 power
			pTargetField->m_Power = (pTargetField->m_Power - pMover->m_Power) + 1;
			m_FieldUpdates.push_back(FieldUpdate(ToX, ToY, *pTargetField));

			return;
		}
	}
}

bool GridGame::IsValidMove(bool Split, int FromX, int FromY, int ToX, int ToY, PlayerIterator PlayerIt)
{
	// Field is no worker
	if (m_Grid[FromX][FromY].m_FieldType != Field::FieldType::FIELD_WORKER)
		return false;

	// Worker not owned by player
	if (m_Grid[FromX][FromY].m_OwnerID != PlayerIt->second.m_ID)
		return false;

	// Worker already moved this turn
	if (m_Grid[FromX][FromY].m_WasMoved)
		return false;

	// Can split
	if (Split && m_Grid[FromX][FromY].m_Power < 2)
		return false;

	// Out of bounds
	if (ToX < 0 || ToX > m_Grid.size() - 1 || ToY < 0 || ToY > m_Grid.size() - 1)
		return false;

	// Too far
	if (std::abs(FromX - ToX) > 1 || std::abs(FromY - ToY) > 1)
		return false;

	return true;
}

PlayerIterator GridGame::GetPlayerByIP(std::string IP)
{
	auto It = std::find_if(
		m_Players.begin(),
		m_Players.end(),
		[IP](auto Player) { return Player.second.m_Client.m_IP == IP && Player.second.m_HasLostConnection == true; }
	);

	return It;
}

PlayerIterator GridGame::GetPlayerByClient(Client Client)
{
	auto It = std::find_if(
		m_Players.begin(), 
		m_Players.end(), 
		[Client](auto Player) { return Player.second.m_Client.m_Socket == Client.m_Socket; }
	);

	return It;
}