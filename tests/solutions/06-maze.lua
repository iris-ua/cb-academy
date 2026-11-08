while true do
  if ir(1) < 1.5 then
    turn(90) forward(2)
  elseif ir(0) < 1.5 then
    forward(2)
  else
    turn(-90)
  end
end
