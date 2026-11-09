while true do
  motors(0.1, 0.1)
  if bump() then
    forward(-0.5)
    turn(90)
  end
end
