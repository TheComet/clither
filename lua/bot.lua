local math = require("math")
local clither = require("clither")

function clither_next_cmd(world, snake, sim_tick_rate)
  --local ahead_dist = snake.head.speed * clither.snake_scale(snake.param)
  local ahead_dist = snake.head.speed * 5.0
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

  print("ahead: " .. ahead.x .. ", " .. ahead.y)
  print("left: " .. left.x .. ", " .. left.y)
  print("right: " .. right.x .. ", " .. right.y)

  return snake.head.angle, 0.0
end
