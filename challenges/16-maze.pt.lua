-- 0 este, 1 norte, 2 oeste, 3 sul
local dir = 0
while true do
  if ir(0) < 1.6 then
    forward(2)
  else
    dir = dir + 1
    turn_to(dir * 90)
  end
end
