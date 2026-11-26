#include "keybinds.h"

Pong_Binds pong_binds = { 'W', 'S', VK_UP, VK_DOWN, VK_ESCAPE, true };
Breaker_Binds breaker_binds = { 'A', 'D', VK_SPACE, VK_ESCAPE, true };

void
reset_pong_binds() {
	pong_binds = { 'W', 'S', VK_UP, VK_DOWN, VK_ESCAPE, true };
}

void
reset_breaker_binds() {
	breaker_binds = { 'A', 'D', VK_SPACE, VK_ESCAPE, true };
}

const char*
vk_name(int vk) {
	static char single[2] = { 0, 0 };
	if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
		single[0] = (char)vk;
		return single;
	}
	switch (vk) {
		case VK_LBUTTON:  return "LMB";
		case VK_RBUTTON:  return "RMB";
		case VK_SPACE:    return "SPACE";
		case VK_RETURN:   return "ENTER";
		case VK_ESCAPE:   return "ESC";
		case VK_SHIFT:    return "SHIFT";
		case VK_LSHIFT:   return "LSHIFT";
		case VK_RSHIFT:   return "RSHIFT";
		case VK_CONTROL:  return "CTRL";
		case VK_LCONTROL: return "LCTRL";
		case VK_RCONTROL: return "RCTRL";
		case VK_MENU:     return "ALT";
		case VK_LMENU:    return "LALT";
		case VK_RMENU:    return "RALT";
		case VK_TAB:      return "TAB";
		case VK_BACK:     return "BACK";
		case VK_UP:       return "UP";
		case VK_DOWN:     return "DOWN";
		case VK_LEFT:     return "LEFT";
		case VK_RIGHT:    return "RIGHT";
		case VK_F1:       return "F1";
		case VK_F2:       return "F2";
		case VK_F3:       return "F3";
		default:          return "KEY";
	}
}
