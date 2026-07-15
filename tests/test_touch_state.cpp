#include "hdmi_test/touch_state.hpp"

#include <cassert>

int main() {
  hdmi_test::TouchState touches;
  for (int id = 0; id < 5; ++id) touches.down(id, static_cast<float>(id), 10.0F);
  assert(touches.active_count() == 5U);

  touches.move(3, 42.0F, 24.0F);
  assert(touches.points()[3].x == 42.0F);
  assert(touches.points()[3].y == 24.0F);

  touches.down(9, 1.0F, 1.0F);
  assert(touches.active_count() == 5U);
  touches.up(3);
  assert(touches.active_count() == 4U);
  touches.clear();
  assert(touches.active_count() == 0U);
}
