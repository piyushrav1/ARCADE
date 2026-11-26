#pragma once

#include "platform.h"

enum Game_Result {
	GAME_CONTINUE = 0,
	GAME_EXIT_TO_HUB = 1
};

namespace Pong {
	void init();
	Game_Result update(Input* input, float dt, float fps);
}

namespace Breaker {
	void init();
	Game_Result update(Input* input, float dt);
}

