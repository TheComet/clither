local math = require("math")
local clither = require("clither")

local current_target = nil
local function find_best_food(food, head_pos, look_pos, radius)
  clither.gfx.draw_debug_circle(look_pos, radius, 0xFFFFFFFF)

  -- Avoid retargeting if we are already on the target
  if current_target then
    if (current_target.x - look_pos.x) ^ 2 + (current_target.y - look_pos.y) ^ 2 < radius ^ 2 then
      return current_target
    else
      current_target = nil
    end
  end

  -- Find closest food in range
  local closest_food = nil
  local closest_dist = nil
  clither.food.for_each_in_radius(food, look_pos, radius, function(food)
    clither.gfx.draw_debug_circle(food.pos, 0.1, 0xFF8000FF)
    if closest_food == nil then
      closest_food = food
      closest_dist = (food.pos.x - head_pos.x) ^ 2 + (food.pos.y - head_pos.y) ^ 2
    else
      local dist = (food.pos.x - head_pos.x) ^ 2 + (food.pos.y - head_pos.y) ^ 2
      if dist < closest_dist then
        closest_food = food
      end
    end
  end)

  current_target = closest_food and closest_food.pos or nil
  return current_target
end

local function calculate_look_rays(snake)
  local ahead_dist = 1.0 * snake.head.speed + snake.param.scale
  local ahead = {
    x = snake.head.pos.x + math.cos(snake.head.angle) * ahead_dist,
    y = snake.head.pos.y + math.sin(snake.head.angle) * ahead_dist,
  }
  local left = {
    x = snake.head.pos.x + math.cos(snake.head.angle - math.pi / 3) * ahead_dist,
    y = snake.head.pos.y + math.sin(snake.head.angle - math.pi / 3) * ahead_dist,
  }
  local right = {
    x = snake.head.pos.x + math.cos(snake.head.angle + math.pi / 3) * ahead_dist,
    y = snake.head.pos.y + math.sin(snake.head.angle + math.pi / 3) * ahead_dist,
  }
  return ahead, left, right
end

function clither_next_cmd(world, snake, sim_tick_rate)
  --print("ahead: " .. ahead.x .. ", " .. ahead.y)
  --print("left: " .. left.x .. ", " .. left.y)
  --print("right: " .. right.x .. ", " .. right.y)

  local ahead, left, right = calculate_look_rays(snake)
  local scan_radius = snake.param.scale

  local target = find_best_food(world.food, snake.head.pos, ahead, scan_radius)
  local angle = snake.head.angle
  if target then
    angle = math.atan(target.y - snake.head.pos.y, target.x - snake.head.pos.x)
    clither.gfx.draw_debug_circle(target, 0.15, 0x00FF00FF)
  else
    -- Return to world center
    angle = math.atan(-snake.head.pos.y, -snake.head.pos.x)
  end

  return angle, 1.0
end
