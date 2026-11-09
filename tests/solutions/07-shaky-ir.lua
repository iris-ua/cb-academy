local h = 0
while true do
  local last = { 0, 0, 0, 0, 0 }
  local i, avg = 0, 0
  while avg < 1.5 do
    local e = (h - compass() + 180) % 360 - 180
    motors(0.1 - 0.003 * e, 0.1 + 0.003 * e)
    i = i % 5 + 1
    last[i] = ir(0)
    avg = (last[1] + last[2] + last[3] + last[4] + last[5]) / 5
  end
  if ground() >= 0 then break end
  h = h + 90
  turn_to(h)
end
