#include "Server.h"
#include "GridGame.h"

int main()
{
    Server* pServer = new Server();

    g_pGridGame = new GridGame(pServer);
    std::thread GameThread(&GridGame::Routine, g_pGridGame);

    pServer->Start();
}