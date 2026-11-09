-- go somewhere: angle() is the direction to it; when a wall blocks the way,
-- follow it (wall on the right) until the goal is ahead on the left
local function to_light(a)
  if math.abs(a) > 40 then turn(a) return end
  local side = math.max(0, ir(2) - 1) - math.max(0, ir(1) - 1) -- keep clear of side walls
  local s = 0.003 * a + 0.04 * side
  motors(0.1 - s, 0.1 + s)
end
local r = 0
local function follow_wall()
  r = 0.6 * r + 0.4 * ir(2)
  if ir(0) > 1.2 or ir(2) > 3 then
    motors(-0.06, 0.06)
  else
    local e = math.max(-1, math.min(1, r - 1.5))
    motors(0.08 - 0.05 * e, 0.08 + 0.05 * e)
  end
end
local function adiff(a, b) return (a - b + 180) % 360 - 180 end
local function go(angle, arrived)
  local following = false
  while not arrived() do
    local a = angle()
    if not following and ir(0) > 1.2 then
      following, r = true, ir(2)
    elseif following and a > 20 and a < 100 and ir(0) < 0.6 and ir(1) < 0.8 then
      following = false
    end
    if following then follow_wall() else to_light(a) end
  end
end
-- average of n GPS readings, standing still
local function where(n)
  local sx, sy = 0, 0
  for _ = 1, n do
    wait()
    local x, y = gps()
    sx, sy = sx + x, sy + y
  end
  return sx / n, sy / n
end

local hx, hy = where(20)
repeat
  go(function() return beacon() or 0 end, function() return ground() == 0 end)
  motors(0, 0)
  wait(3)
until ground() == 0
led("visiting", true)
wait()
led("visiting", false)
led("returning", true)

local fx, fy = gps()
local function home_angle()
  local x, y = gps()
  fx, fy = fx + 0.3 * (x - fx), fy + 0.3 * (y - fy)
  return adiff(math.deg(math.atan(hy - fy, hx - fx)), compass())
end
local function near() return (hx - fx) ^ 2 + (hy - fy) ^ 2 < 1 end
while true do
  go(home_angle, near)
  motors(0, 0)
  wait(3)
  fx, fy = where(40)
  if near() then break end
end
local dx, dy = hx - fx, hy - fy
turn(adiff(math.deg(math.atan(dy, dx)), compass()))
forward(math.sqrt(dx * dx + dy * dy))
