local want = 1.5
local r = ir(2)
while true do
  r = 0.6 * r + 0.4 * ir(2)
  if ir(0) > 1.2 or ir(2) > 3 then
    motors(-0.06, 0.06) -- wall ahead: turn left on the spot
  else
    local e = math.max(-1, math.min(1, r - want))
    motors(0.08 - 0.05 * e, 0.08 + 0.05 * e)
  end
end
