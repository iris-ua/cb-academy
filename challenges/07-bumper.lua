-- Head for the light. What if a rock is in the way?
while true do
  local a = beacon() or 0
  if math.abs(a) > 15 then
    turn(a)
  else
    motors(0.12, 0.12)
  end
end
