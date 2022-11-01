#include "GridGame.h"
#include "Server.h"
#include "Utility.h"
#include <iostream>
#include <algorithm>
#include <format>
#include <chrono>
#include <thread>

#undef max
#undef min

GridGame* g_pGridGame = nullptr;

GridGame::GridGame(Server* pServer)
{
	m_NewGame = true;
	m_GameRunning = false;
	m_QueueStartTime = 0;
	m_pServer = pServer;
	m_TurnTimeout = 0;
	m_GridWidth = 25;
	m_GridHeight = 25;

	m_Grid.resize(m_GridWidth);

	for (uint32_t x = 0; x < m_GridWidth; x++)
	{
		m_Grid[x].resize(m_GridHeight);
	}

	Instruction Connect = {
		InstructionType::TYPE_STRING, // Name
	};

	Instruction ConnectAck = {
		InstructionType::TYPE_UINT8, // Player ID
	};

	Instruction Move = {
		InstructionType::TYPE_BOOL,   // Should split
		InstructionType::TYPE_UINT32, // From X
		InstructionType::TYPE_UINT32, // From Y
		InstructionType::TYPE_UINT32, // To X
		InstructionType::TYPE_UINT32, // To Y
	};

	Instruction Broadcast = {
		InstructionType::TYPE_STRING, // Message
	};

	Instruction GameData = {
		InstructionType::TYPE_UINT8,  // Turn player ID
		InstructionType::TYPE_INT64,  // Time epoch move timeout
		InstructionType::TYPE_UINT32, // Grid width
		InstructionType::TYPE_UINT32, // Grid height
		InstructionStructure {        // Updated fields[]
			{
				InstructionType::TYPE_UINT8,  // X
				InstructionType::TYPE_UINT8,  // Y
				InstructionType::TYPE_UINT8,  // Type ID
				InstructionType::TYPE_UINT8,  // Owner ID
				InstructionType::TYPE_INT16   // Power
			}
		}
	};

	pServer->RegisterInstruction(NetDataType::NET_CONNECT, Connect);
	pServer->RegisterInstruction(NetDataType::NET_CONNECT_ACK, ConnectAck);
	pServer->RegisterInstruction(NetDataType::NET_LEAVE, Instruction());
	pServer->RegisterInstruction(NetDataType::NET_MOVE, Move);
	pServer->RegisterInstruction(NetDataType::NET_END_TURN, Instruction());
	pServer->RegisterInstruction(NetDataType::NET_BROADCAST, Broadcast);
	pServer->RegisterInstruction(NetDataType::NET_GAME_DATA, GameData);
}

void GridGame::Routine()
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		std::time_t Now = std::time(nullptr);

		// Reset queue if insufficient players
		if (m_Players.size() < 2)
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
		if (Now - m_QueueStartTime > 5)
		{
			PrepareGame();
			{
				std::lock_guard LockGuard(m_Mutex);
				StartNewTurn();
			}
			Run();
		}
	}
}

void GridGame::PrepareGame()
{
	m_NewGame = true;
	m_GameRunning = true;
	m_QueueStartTime = 0;

	for (uint32_t x = 0; x < m_GridWidth; x++)
	{
		for (uint32_t y = 0; y < m_GridHeight; y++)
		{
			m_Grid[x][y] = Field();
		}
	}

	// Generate workers
	for (auto& Player : m_Players)
	{
		uint32_t x, y;

		do
		{
			x = Utility::GetRandomInteger<uint32_t>(0, m_GridWidth - 1);
			y = Utility::GetRandomInteger<uint32_t>(0, m_GridHeight - 1);

		} while (m_Grid[x][y].m_FieldType == Field::FieldType::FIELD_WORKER);

		m_Grid[x][y] = Field(Field::FieldType::FIELD_WORKER, Player.second.m_ID, 5);
		Player.second.m_WorkersAlive = 1;
		Player.second.m_HasLostGame = false;
	}

	GenerateFood();
}

void GridGame::GenerateFood()
{
	//TODO: tell clients about next food update
	// corners or edges teleport

	// Generate food
	for (int i = 0; i < m_Players.size() * 2; i++)
	{
		uint32_t x, y;

		do
		{
			x = Utility::GetRandomInteger<uint32_t>(0, m_GridWidth - 1);
			y = Utility::GetRandomInteger<uint32_t>(0, m_GridHeight - 1);

		} while (m_Grid[x][y].m_FieldType != Field::FieldType::FIELD_EMPTY);

		m_Grid[x][y] = Field(Field::FieldType::FIELD_FOOD, 0, 1);
	}
}

void GridGame::Run()
{
	bool GameRunning = true;

	do
	{
		GameRunning = CheckConditions();
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	} while (GameRunning);

	std::lock_guard LockGuard(m_Mutex);
	m_GameRunning = false;
}

bool GridGame::CheckConditions()
{
	std::lock_guard LockGuard(m_Mutex);
	std::time_t Now = std::time(nullptr);
	std::size_t PlayersAlive = m_Players.size();
	int FoodCount = 0;

	if (m_NewGame)
	{
		m_TurnTimeout = Now + 10;
		m_NewGame = false;
	}

	// Move time exceeded
	if (Now >= m_TurnTimeout)
		StartNewTurn();

	for (uint32_t x = 0; x < m_GridWidth; x++)
	{
		for (uint32_t y = 0; y < m_GridHeight; y++)
		{
			if (m_Grid[x][y].m_FieldType == Field::FieldType::FIELD_FOOD)
				FoodCount++;
		}
	}

	if (FoodCount <= 0)
		GenerateFood();

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
	else if (PlayersAlive == 1)
	{
		for (auto& Player : m_Players)
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

	return true;
}

void GridGame::StartNewTurn()
{
	PlayerIterator PlayerNextIt;
	PlayerIterator PlayerIt = GetPlayerFromClient(m_TurnPlayer.m_Client);

	if (PlayerIt == m_Players.end())
		PlayerNextIt = m_Players.begin();
	else
		PlayerNextIt = std::next(PlayerIt, 1);
	
	if (PlayerNextIt == m_Players.end())
		PlayerNextIt = m_Players.begin();

	m_TurnPlayer = PlayerNextIt->second;
	m_TurnTimeout = std::time(nullptr) + 10;

	for (auto& Player : m_Players)
	{
		SendClientUpdate(Player.second);
	}
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

	for (uint32_t x = 0; x < m_GridWidth; x++)
	{
		for (uint32_t y = 0; y < m_GridHeight; y++)
		{
			Field* pField = &m_Grid[x][y];
			pField->m_WasMoved = false;

			// Set new worker count
			if (pField->m_FieldType == Field::FieldType::FIELD_WORKER && pField->m_OwnerID != FIELD_NO_OWNER)
				m_Players[pField->m_OwnerID].m_WorkersAlive++;
		}
	}

	StartNewTurn();
}

void GridGame::SendClientUpdate(Player Player)
{
	std::vector<PacketStruct> FieldUpdates;

	for (Move Move : m_Moves)
	{
		FieldUpdates.push_back(
			{
				(uint8_t)Move.X,
				(uint8_t)Move.Y,
				(uint8_t)Move.Field.m_FieldType,
				(uint8_t)Move.Field.m_OwnerID,
				(uint16_t)Move.Field.m_Power
			}
		);
	}

	Packet Packet(NetDataType::NET_GAME_DATA);
	Packet.push_back(m_TurnPlayer.m_ID);
	Packet.push_back(m_TurnTimeout);
	Packet.push_back(m_GridWidth);
	Packet.push_back(m_GridHeight);
	Packet.push_back(FieldUpdates);

	m_pServer->GetSerializer()->SerializeSend(
		Packet,
		Player.m_Client.m_Socket
	);
}

void GridGame::Kick(Client Client)
{
	// Check if this client is actually a player
	PlayerIterator PlayerIt = GetPlayerFromClient(Client);
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
	std::lock_guard LockGuard(m_Mutex);

	if (Data.m_Magic == NetDataType::NET_CONNECT)
	{
		HandleConnect(Data, Client);
		return;
	}

	// Check if this client is actually a player
	auto PlayerIt = GetPlayerFromClient(Client);
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
	std::lock_guard LockGuard(m_Mutex);

	// Check if this client is actually a player
	auto PlayerIt = GetPlayerFromClient(Client);
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
	PlayerIterator PlayerIt = GetPlayerFromIP(Client.m_IP);
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
		int Index = std::distance(m_Players.begin(), m_Players.end());

		uint8_t LowestID = 0;
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
		m_pServer->GetSerializer()->SerializeSend(
			Broadcast,
			Player.second.m_Client.m_Socket
		);
	}

	if (IsReconnect)
	{
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

		pOriginField->m_Power = (int)std::floor(Power / 2.0);

		pMover->m_FieldType = Field::FieldType::FIELD_WORKER;
		pMover->m_OwnerID = pOriginField->m_OwnerID;
		pMover->m_Power = (int)std::ceil(Power / 2.0);
		pMover->m_WasMoved = true;
	}
	else 
	{
		pMover->m_FieldType = pOriginField->m_FieldType;
		pMover->m_OwnerID = pOriginField->m_OwnerID;
		pMover->m_Power = pOriginField->m_Power;
		pMover->m_WasMoved = true;

		// Reset the field we come from
		pOriginField->Reset();

		m_Moves.push_back(Move(ToX, ToY, *pOriginField));
	}

	if (pTargetField->m_FieldType == Field::FieldType::FIELD_EMPTY)
	{
		// Move
		*pTargetField = *pMover;

		m_Moves.push_back(Move(ToX, ToY, *pTargetField));

		return;
	}

	if (pTargetField->m_FieldType == Field::FieldType::FIELD_FOOD)
	{
		// Move & eat food
		pTargetField->m_OwnerID = pMover->m_OwnerID;
		pTargetField->m_Power = pMover->m_Power + pTargetField->m_Power;
		pTargetField->m_FieldType = Field::FieldType::FIELD_WORKER;
		pTargetField->m_WasMoved = true;

		m_Moves.push_back(Move(ToX, ToY, *pTargetField));

		return;
	}

	if (pTargetField->m_FieldType == Field::FieldType::FIELD_WORKER)
	{
		if (pTargetField->m_OwnerID == pMover->m_OwnerID)
		{
			// Move and merge
			pMover->m_Power += pTargetField->m_Power;
			*pTargetField = *pMover;

			m_Moves.push_back(Move(ToX, ToY, *pTargetField));

			return;
		}

		if (pMover->m_Power == pTargetField->m_Power)
		{
			// Kill eachother
			pTargetField->Reset();

			m_Moves.push_back(Move(ToX, ToY, *pTargetField));

			return;
		}

		if (pMover->m_Power > pTargetField->m_Power)
		{
			// Win fight and "gain" 1 power
			*pTargetField = *pMover;
			pTargetField->m_Power = (pMover->m_Power - pTargetField->m_Power) + 1;

			m_Moves.push_back(Move(ToX, ToY, *pTargetField));

			return;
		}

		if (pMover->m_Power < pTargetField->m_Power)
		{
			// Lose fight and enemy "gain" 1 power
			pTargetField->m_Power = (pTargetField->m_Power - pMover->m_Power) + 1;

			m_Moves.push_back(Move(ToX, ToY, *pTargetField));

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

PlayerIterator GridGame::GetPlayerFromIP(std::string IP)
{
	auto It = std::find_if(
		m_Players.begin(),
		m_Players.end(),
		[IP](auto Player) { return Player.second.m_Client.m_IP == IP && Player.second.m_HasLostConnection == true; }
	);

	return It;
}

PlayerIterator GridGame::GetPlayerFromClient(Client Client)
{
	auto It = std::find_if(
		m_Players.begin(), 
		m_Players.end(), 
		[Client](auto Player) { return Player.second.m_Client.m_Socket == Client.m_Socket; }
	);

	return It;
}