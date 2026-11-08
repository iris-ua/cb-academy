-- Example robot: head for the beacon, steer away from walls.
-- Run: ./build/ciberrato examples/robot.lua   (save the file to reload)

say("Hello! Looking for the cheese...")
led("visiting", false)

while ground() < 0 do
  local front, left, right = ir(0), ir(1), ir(2)
  local b = beacon()

  if front > 2.5 then
    -- wall ahead: spin toward the more open side
    if left < right then motors(-0.06, 0.06) else motors(0.06, -0.06) end
  elseif b and math.abs(b) > 20 and front < 1.5 then
    -- turn toward the beacon
    if b > 0 then motors(0.02, 0.08) else motors(0.08, 0.02) end
  else
    -- cruise, nudged away from side walls
    local steer = 0.03 * (left - right)
    motors(0.1 + steer, 0.1 - steer)
  end
end

led("visiting", true)
say("Found it at tick " .. time())
finish()
