while true do
  if bump() then
    forward(-0.7)
    turn(math.random() < 0.5 and 90 or -90)
    forward(2)
  else
    local a = beacon() or 0
    if math.abs(a) > 15 then turn(a) else motors(0.12, 0.12) end
  end
end
