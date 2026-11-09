-- average n compass errors to heading h, standing still
local function err(h, n)
  local sum = 0
  for _ = 1, n do
    wait()
    sum = sum + (h - compass() + 180) % 360 - 180
  end
  return sum / n
end
local function drive(h, n)
  turn_to(h)
  turn(err(h, 20))
  local e = 0
  for _ = 1, n * 10 do
    e = 0.9 * e + 0.1 * ((h - compass() + 180) % 360 - 180)
    motors(0.1 - 0.003 * e, 0.1 + 0.003 * e)
  end
end
drive(0, 10) drive(90, 8) drive(180, 6)
