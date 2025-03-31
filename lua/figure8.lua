local math = require("math")
local angle = 0.0
local direction = 4.0

function clither_next_cmd(world, snake, sim_tick_rate)
  local rot_speed = 2 * math.pi / sim_tick_rate
  angle = angle + rot_speed / direction
  if angle > math.pi or angle < -math.pi then
    direction = -direction
  end
  return angle, 0.5
end
