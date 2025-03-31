local math = require("math")
local angle = 0.0
local da_dir = 4.0

function clither_next_cmd(world, snake, sim_tick_rate)
  local da = 2 * math.pi / sim_tick_rate
  angle = angle + da / da_dir
  if angle > math.pi or angle < -math.pi then
    da_dir = -da_dir
  end
  return angle, 0.5
end
