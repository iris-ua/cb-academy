-- Right-hand rule, one 2x2 square at a time. The compass keeps the heading
-- square, a wall in front (or behind) puts the mouse back in the middle.
local dir = math.floor(compass() / 90 + 0.5)
local function open(i) return ir(i) < 1.6 end
-- a wall 0.4 from sensor i (0 front, 3 back) means we are in the middle
local function align(i)
  if ir(i) < 1.2 then return false end
  local s = i == 0 and 1 or -1
  for _ = 1, 40 do
    local e = ir(i) - 2.5
    if math.abs(e) < 0.1 then break end
    local v = (e < 0 and 0.03 or -0.03) * s
    motors(v, v)
  end
  return true
end
local function face(h) -- compass is noisy: average a few reads
  turn_to(h, 0.15)
  local err = 0
  for _ = 1, 4 do wait() err = err + (h - compass() + 180) % 360 - 180 end
  turn(err / 4)
end
-- one square ahead: steer by the compass, and keep in the middle when there are walls on both sides
local function step(h)
  local hit = false
  for _ = 1, 16 do
    local e = (h - compass() + 180) % 360 - 180
    local l, r = ir(1), ir(2)
    if l > 1.2 and r > 1.2 then e = e + 15 * (r - l) end
    motors(0.125 - 0.002 * e, 0.125 + 0.002 * e)
    hit = hit or bump()
  end
  motors(0, 0)
  return not hit
end
-- squares walked, relative to the start; stepping back drops the last one
local x, y = 0, 0
local path = { { 0, 0 } }
local DX, DY = { [0] = 1, 0, -1, 0 }, { [0] = 0, 1, 0, -1 }
local function moved()
  x, y = x + DX[dir % 4], y + DY[dir % 4]
  local prev = path[#path - 1]
  if prev and prev[1] == x and prev[2] == y then path[#path] = nil
  else path[#path + 1] = { x, y } end
end
-- false if a wall was in the way (the mouse is still in its square)
local function go_square()
  if not align(0) then align(3) end
  local ok = step((dir % 4) * 90)
  align(0)
  return ok
end
while true do
  if ground() == 0 then
    motors(0, 0)
    wait(3)
    if ground() == 0 then break end
  end
  local was = dir
  if open(2) then dir = dir - 1
  elseif open(0) then
  elseif open(1) then dir = dir + 1
  else dir = dir + 2 end
  if dir ~= was then face((dir % 4) * 90) end
  if go_square() then moved() end
end
led("visiting", true)
wait()
led("visiting", false)
led("returning", true)
for k = #path - 1, 1, -1 do
  local tx, ty = path[k][1], path[k][2]
  for d = 0, 3 do
    if x + DX[d] == tx and y + DY[d] == ty then dir = d end
  end
  face(dir * 90)
  while not go_square() do end
  x, y = tx, ty
end
