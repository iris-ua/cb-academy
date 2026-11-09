-- go to light i: head for it; when a wall blocks the way, follow it (wall
-- on the right) until the light is ahead on the left, away from the wall
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
local function go_to(i)
  local following = false
  while ground() ~= i do
    local a = beacon(i) or 0
    if not following and ir(0) > 1.2 then
      following, r = true, ir(2)
    elseif following and a > 20 and a < 100 and ir(0) < 0.6 and ir(1) < 0.8 then
      following = false
    end
    if following then follow_wall() else to_light(a) end
  end
end
go_to(0)
