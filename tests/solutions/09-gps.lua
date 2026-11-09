-- average n GPS readings, standing still
local function where(n)
  local sx, sy = 0, 0
  for _ = 1, n do
    wait()
    local x, y = gps()
    sx, sy = sx + x, sy + y
  end
  return sx / n, sy / n
end
local x0, y0 = where(30)
local gx, gy = x0 + 12, y0 + 7
local fx, fy = gps()
while true do
  -- drive with a smoothed fix until close
  while true do
    local x, y = gps()
    fx, fy = fx + 0.2 * (x - fx), fy + 0.2 * (y - fy)
    local dx, dy = gx - fx, gy - fy
    if dx * dx + dy * dy < 1.5 * 1.5 then break end
    local e = (math.deg(math.atan(dy, dx)) - compass() + 180) % 360 - 180
    if math.abs(e) > 45 then turn(e) else motors(0.12 - 0.004 * e, 0.12 + 0.004 * e) end
  end
  -- careful last steps
  motors(0, 0) wait(3)
  fx, fy = where(40)
  local dx, dy = gx - fx, gy - fy
  turn_to(math.deg(math.atan(dy, dx)))
  forward(math.sqrt(dx * dx + dy * dy))
  if ground() >= 0 then break end
  fx, fy = where(20)
end
