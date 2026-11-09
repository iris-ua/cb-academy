-- 0 east, 1 north, 2 west, 3 south
local dir = 0
while true do
  if ir(0) < 1.6 then
    forward(2)
  else
    dir = dir + 1
    turn_to(dir * 90)
  end
end
