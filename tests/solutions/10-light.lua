while true do
  local a = beacon()
  if a == nil then
    motors(-0.05, 0.05) -- look around
  elseif math.abs(a) > 40 then
    turn(a)
  else
    motors(0.12 - 0.003 * a, 0.12 + 0.003 * a)
  end
end
