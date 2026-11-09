while true do
  while ir(0) < 1.5 do
    motors(0.1, 0.1)
  end
  if ground() >= 0 then break end
  turn(90)
end
